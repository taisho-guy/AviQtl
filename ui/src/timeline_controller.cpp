#include "timeline_controller.hpp"
#include "../../core/include/project_serializer.hpp"
#include "../../core/include/video_decoder.hpp"
#include "../../core/include/video_encoder.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "../../scripting/lua_host.hpp"
#include "clip_model.hpp"
#include "commands.hpp"
#include "core/include/video_frame_store.hpp"
#include "effect_registry.hpp"
#include "project_service.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include "timeline_service.hpp"
#include "transport_service.hpp"
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQmlContext>
#include <QQuickRenderTarget>
#include <QQuickView>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>

namespace Rina::UI { // namespace開始

TimelineController::TimelineController(QObject *parent) : QObject(parent) {
    // サービスの初期化
    m_project = new ProjectService(this);
    m_transport = new TransportService(this);
    m_selection = new SelectionService(this);
    m_timeline = new TimelineService(m_selection, this);

    // Phase 1: 初期状態の明示的設定
    m_selection->select(-1, QVariantMap());

    m_clipModel = new ClipModel(m_transport, this);

    // TimelineServiceからのシグナル接続
    connect(m_timeline, &TimelineService::clipsChanged, this, [this]() {
        rebuildClipIndex();

        // 【追加】最終フレームを自動計算して更新
        recalculateTotalFrames();

        emit clipsChanged();
        updateVideoDecoders();
        updateActiveClipsList();
    });
    connect(m_selection, &SelectionService::selectedClipDataChanged, this, [this]() {
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
        updateClipActiveState();
    });

    connect(m_timeline, &TimelineService::scenesChanged, this, &TimelineController::scenesChanged);
    connect(m_timeline, &TimelineService::currentSceneIdChanged, this, &TimelineController::currentSceneIdChanged);
    connect(m_timeline, &TimelineService::clipEffectsChanged, this, &TimelineController::clipEffectsChanged);

    // サービス間の連携: FPSが変更されたら再生タイマーの間隔を更新
    connect(m_project, &ProjectService::fpsChanged, [this]() { m_transport->updateTimerInterval(m_project->fps()); });
    m_transport->updateTimerInterval(m_project->fps());

    // 再生位置が変わったらプレビュー更新
    connect(m_transport, &TransportService::currentFrameChanged, this, [this]() {
        int nextFrame = m_transport->currentFrame();
        // ループ再生ロジック
        if (nextFrame > m_project->totalFrames()) {
            m_transport->setCurrentFrame(0);
        }
        updateClipActiveState();

        // アクティブな動画クリップのフレームをシーク
        for (auto it = m_videoDecoders.begin(); it != m_videoDecoders.end(); ++it) {
            const auto *clip = m_timeline->findClipById(it.key());
            if (clip && (nextFrame >= clip->startFrame && nextFrame < clip->startFrame + clip->durationFrames)) {
                it.value()->seekToFrame(nextFrame - clip->startFrame, m_project->fps());
            }
        }

        updateActiveClipsList();
    });

    rebuildClipIndex();
    updateClipActiveState();
}

void TimelineController::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    qDebug() << "TimelineController: VideoFrameStore set. Updating decoders...";
    // ストアがセットされたので、既存のクリップに対するデコーダを生成する
    updateVideoDecoders();
}

void TimelineController::togglePlay() {
    if (m_transport)
        m_transport->togglePlay();
}

void TimelineController::undo() { m_timeline->undo(); }
void TimelineController::redo() { m_timeline->redo(); }

double TimelineController::timelineScale() const { return m_timelineScale; }
void TimelineController::setTimelineScale(double scale) {
    if (qAbs(m_timelineScale - scale) > 0.001) {
        m_timelineScale = scale;
        emit timelineScaleChanged();
    }
}

// --- 汎用プロパティアクセサ ---
void TimelineController::setClipProperty(const QString &name, const QVariant &value) {
    int id = m_selection->selectedClipId();
    if (id == -1)
        return;

    for (auto &clip : m_timeline->clipsMutable()) {
        if (clip.id == id) {
            // 暫定対応: プロパティ名に応じて適切なエフェクトのパラメータを更新する
            // POD化に伴い、直接書き込みではなくService経由で更新する
            bool found = false;
            for (int i = 0; i < clip.effects.size(); ++i) {
                if (clip.effects[i]->params().contains(name)) {
                    // Undo/Redo対応のためコマンド経由で更新
                    updateClipEffectParam(id, i, name, value);

                    // キャッシュ更新 (現在のUI用のフラット化されたビュー)
                    // updateClipEffectParam内でSelection更新も行われるため削除

                    // Update preview
                    updateActiveClipsList();
                    found = true;
                    break;
                }
            }

            if (!found && !clip.effects.isEmpty()) {
                int targetIndex = 0;
                // "transform" に属するキーかどうかで対象エフェクトを振り分け
                static const QStringList transformKeys = {"x", "y", "z", "scale", "aspect", "rotationX", "rotationY", "rotationZ", "opacity"};
                if (!transformKeys.contains(name) && clip.effects.size() > 1) {
                    targetIndex = 1;
                }
                if (targetIndex < clip.effects.size()) {
                    updateClipEffectParam(id, targetIndex, name, value);
                    updateActiveClipsList();
                }
            }
            break;
        }
    }
}

QVariant TimelineController::getClipProperty(const QString &name) const { return m_selection->selectedClipData().value(name); }

int TimelineController::clipStartFrame() const { return m_selection->selectedClipData().value("startFrame", 0).toInt(); }
void TimelineController::setClipStartFrame(int frame) {
    int id = m_selection->selectedClipId();
    if (id < 0)
        return;
    m_timeline->updateClip(id, layer(), frame, clipDurationFrames());
}

int TimelineController::clipDurationFrames() const { return m_selection->selectedClipData().value("durationFrames", 100).toInt(); }
void TimelineController::setClipDurationFrames(int frames) {
    int id = m_selection->selectedClipId();
    if (id < 0)
        return;
    m_timeline->updateClip(id, layer(), clipStartFrame(), frames);
}

int TimelineController::layer() const { return m_selection->selectedClipData().value("layer", 0).toInt(); }
void TimelineController::setLayer(int layer) {
    int id = m_selection->selectedClipId();
    if (id < 0)
        return;
    m_timeline->updateClip(id, layer, clipStartFrame(), clipDurationFrames());
}

void TimelineController::setSelectedLayer(int layer) {
    if (m_selectedLayer != layer) {
        m_selectedLayer = layer;
        emit selectedLayerChanged();
    }
}

bool TimelineController::isClipActive() const { return m_isClipActive; }

void TimelineController::updateClipActiveState() {
    int current = m_transport->currentFrame();
    int start = clipStartFrame();
    int duration = clipDurationFrames();
    // シンプルな矩形判定: Start <= Current < Start + Duration
    bool active = (current >= start) && (current < start + duration);
    if (m_isClipActive != active) {
        m_isClipActive = active;
        emit isClipActiveChanged();
    }

    if (m_isClipActive) {
        int id = m_selection->selectedClipId();
        if (id >= 0) {
            Rina::Engine::Timeline::ECS::instance().updateClipState(id, layer(), current - start);
        }
    }
}

QString TimelineController::activeObjectType() const { return m_selection->selectedClipData().value("type", "rect").toString(); }

void TimelineController::createObject(const QString &type, int startFrame, int layer) {
    if (m_timeline)
        m_timeline->createClip(type, startFrame, layer);
}

QList<QObject *> TimelineController::getClipEffectsModel(int clipId) const {
    QList<QObject *> list;
    for (const auto &clip : m_timeline->clips()) {
        if (clip.id == clipId) {
            for (auto *eff : clip.effects) {
                list.append(eff);
            }
            break;
        }
    }
    return list;
}

void TimelineController::updateClipEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) { m_timeline->updateEffectParam(clipId, effectIndex, paramName, value); }

QVariantList TimelineController::clips() const {
    QVariantList list;
    for (const auto &clip : m_timeline->clips()) {
        QVariantMap map;
        map["id"] = clip.id;
        map["type"] = clip.type;
        map["startFrame"] = clip.startFrame;
        map["durationFrames"] = clip.durationFrames;
        map["layer"] = clip.layer;

        // オブジェクトのQMLパスを取得して追加
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip.type);
        if (!meta.qmlSource.isEmpty()) {
            map["qmlSource"] = meta.qmlSource;
        } else {
            // 見つからない場合のフォールバック
            // 実行時のカレントディレクトリ基準になるので注意が必要
            if (clip.type == "text")
                map["qmlSource"] = "file:objects/TextObject.qml";
            else if (clip.type == "rect")
                map["qmlSource"] = "file:objects/RectObject.qml";
        }

        // Include some properties for list view if needed
        // 暫定: テキストエフェクトがあればそのテキストを表示
        for (auto *eff : clip.effects) {
            if (eff->id() == "text") {
                QVariantMap params = eff->params();
                if (params.contains("text"))
                    map["text"] = params["text"];
                break;
            }
        }

        list.append(map);
    }
    return list;
}

void TimelineController::updateActiveClipsList() {
    int current = m_transport->currentFrame();
    // ネスト解決済みのクリップリストを取得
    QList<ClipData *> active = m_timeline->resolvedActiveClipsAt(current);

    // レイヤー順 (昇順) にソート
    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer < b->layer; });

    m_clipModel->updateClips(active);
}

void TimelineController::rebuildClipIndex() {
    m_sortedClips.clear();
    m_maxDuration = 0;
    auto &clips = m_timeline->clipsMutable();
    m_sortedClips.reserve(clips.size());
    for (auto &clip : clips) {
        m_sortedClips.push_back(&clip);
        if (clip.durationFrames > m_maxDuration)
            m_maxDuration = clip.durationFrames;
    }
    std::sort(m_sortedClips.begin(), m_sortedClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });
}

void TimelineController::updateVideoDecoders() {
    const auto &clips = m_timeline->clips();
    QSet<int> currentVideoClipIds;

    // 新規・既存の動画クリップをチェック
    for (const auto &clip : clips) {
        if (clip.type == "video") {
            currentVideoClipIds.insert(clip.id);

            // 既にデコーダがある場合はスキップ
            if (!m_videoDecoders.contains(clip.id)) {
                // Videoエフェクト(index 1)からパスを取得
                const auto *videoEffect = (clip.effects.size() > 1) ? clip.effects[1] : nullptr;
                if (videoEffect) {
                    QUrl sourceUrl = QUrl::fromLocalFile(videoEffect->params().value("path").toString());
                    qDebug() << "TimelineController: Found video clip" << clip.id << "path:" << sourceUrl;

                    if (sourceUrl.isValid() && !sourceUrl.isEmpty()) {
                        if (!m_videoFrameStore) {
                            qWarning() << "VideoFrameStore not set!";
                            continue;
                        }
                        m_videoDecoders.insert(clip.id, new Rina::Core::VideoDecoder(clip.id, sourceUrl, m_videoFrameStore, this));
                        qDebug() << "TimelineController: VideoDecoder created for clipId:" << clip.id << "source:" << sourceUrl;
                    }
                }
            }
        }
    }

    // 削除されたクリップのデコーダーをクリーンアップ
    for (auto it = m_videoDecoders.begin(); it != m_videoDecoders.end();) {
        if (!currentVideoClipIds.contains(it.key())) {
            it.value()->deleteLater();
            it = m_videoDecoders.erase(it);
        } else {
            ++it;
        }
    }
}

void TimelineController::recalculateTotalFrames() {
    int maxEndFrame = 0;

    // 現在のシーンの全クリップを走査
    // m_timeline->clips() は現在のシーンのクリップリストを返す
    for (const auto &clip : m_timeline->clips()) {
        int endFrame = clip.startFrame + clip.durationFrames;
        if (endFrame > maxEndFrame) {
            maxEndFrame = endFrame;
        }
    }

    // プロジェクト設定を更新
    // 無限ループを防ぐため、値が変わった時だけセットされる(ProjectService側でチェック済み)
    // 最低でも1フレームは確保する
    m_project->setTotalFrames(std::max(1, maxEndFrame));
}

void TimelineController::log(const QString &msg) { qDebug() << "[TimelineBridge] " << msg; }

void TimelineController::updateClip(int id, int layer, int startFrame, int duration) { m_timeline->updateClip(id, layer, startFrame, duration); }

bool TimelineController::saveProject(const QString &fileUrl) {
    QString error;
    bool result = Rina::Core::ProjectSerializer::save(fileUrl, m_timeline, m_project, &error);
    if (!result) {
        emit errorOccurred(error);
    }
    return result;
}

bool TimelineController::loadProject(const QString &fileUrl) {
    QString error;
    bool result = Rina::Core::ProjectSerializer::load(fileUrl, m_timeline, m_project, &error);
    if (!result) {
        emit errorOccurred(error);
    }
    return result;
}

QVariantMap TimelineController::getProjectInfo(const QString &fileUrl) const {
    QVariantMap result;
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty())
        path = fileUrl;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "プロジェクトファイル情報取得のためファイルを開けませんでした:" << path;
        return result;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "プロジェクトJSONの解析に失敗しました:" << error.errorString();
        return result;
    }

    QJsonObject root = doc.object();
    if (root.contains("settings")) {
        result = root["settings"].toObject().toVariantMap();
    }

    return result;
}

bool TimelineController::exportMedia(const QString &fileUrl, const QString &format, int quality) {
    QString localPath = QUrl(fileUrl).toLocalFile();
    if (localPath.isEmpty())
        localPath = fileUrl;

    if (format == "image_sequence") {
        return exportImageSequence(localPath, quality);
    } else if (format == "avi" || format == "mp4") {
        return exportVideo(localPath, format, quality);
    }

    qWarning() << "サポートされていないエクスポート形式です:" << format;
    return false;
}

bool TimelineController::exportImageSequence(const QString &dir, int quality) {
    int totalFrames = m_project->totalFrames();
    // ディレクトリ作成等の処理は省略（ファイル名として渡される前提）
    // 実際は連番ファイル名を生成するロジックが必要
    QString baseName = dir;
    if (baseName.endsWith(".png"))
        baseName.chop(4);

    const int sequencePadding = Rina::Core::SettingsManager::instance().settings().value("exportSequencePadding", 6).toInt();
    for (int frame = 0; frame < totalFrames; ++frame) {
        m_transport->setCurrentFrame(frame);

        // GPU→CPUコピーを最小化: QMLシーングラフから直接キャプチャ
        QImage renderedFrame;

        if (m_compositeView) {
            // QQuickWindow::grabWindow() (推奨): GPU→CPUコピーは1回のみで効率的
            renderedFrame = m_compositeView->grabWindow();

            // プロジェクト解像度にリサイズ（必要な場合）
            QSize targetSize(m_project->width(), m_project->height());
            if (renderedFrame.size() != targetSize) {
                renderedFrame = renderedFrame.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
        } else {
            qWarning() << "CompositeViewが設定されていません。フォールバックレンダリングを使用します。";
            renderedFrame = renderCurrentFrame();
        }

        QString filename = QString("%1_%2.png").arg(baseName).arg(frame, sequencePadding, 10, QChar('0'));

        if (!renderedFrame.save(filename, "PNG", quality)) {
            qWarning() << "フレームの保存に失敗しました:" << filename;
            return false;
        }

        emit exportProgressChanged((frame * 100) / totalFrames);
    }

    emit exportProgressChanged(100);
    return true;
}

bool TimelineController::exportVideo(const QString &path, const QString &format, int quality) {
    Rina::Core::VideoEncoder encoder;
    Rina::Core::VideoEncoder::Config config;

    config.width = m_project->width();
    config.height = m_project->height();
    config.fps_num = static_cast<int>(m_project->fps() * 1000);
    config.fps_den = 1000;
    // ビットレート計算 (簡易): 1080p60fps -> 12Mbps 程度を目安に
    config.bitrate = static_cast<int>(config.width * config.height * m_project->fps() * 0.15);
    config.outputUrl = path;
    config.codecName = "h264_vaapi"; // AMD Radeon 780M 推奨

    if (!encoder.open(config)) {
        emit errorOccurred("エンコーダーの初期化に失敗しました。VA-APIドライバを確認してください。");
        return false;
    }

    int totalFrames = m_project->totalFrames();
    for (int frame = 0; frame < totalFrames; ++frame) {
        m_transport->setCurrentFrame(frame);

        // QMLシーングラフからキャプチャ (CPUメモリへのダウンロード発生)
        QImage renderedFrame;
        if (m_compositeView) {
            renderedFrame = m_compositeView->grabWindow();
        } else {
            renderedFrame = renderCurrentFrame();
        }

        // エンコーダーへプッシュ (CPU -> GPU アップロード発生)
        if (!encoder.pushFrame(renderedFrame, frame)) {
            qWarning() << "Frame encoding failed at:" << frame;
        }
        emit exportProgressChanged((frame * 100) / totalFrames);
    }

    encoder.close();
    emit exportProgressChanged(100);
    return true;
}

void TimelineController::exportVideoHW(Rina::Core::VideoEncoder *encoder) {
    if (!encoder || !m_project)
        return;

    if (!m_compositeView) {
        qWarning() << "Export failed: CompositeView is not initialized.";
        return;
    }

    const int endFrame = m_project->totalFrames();
    const int width = m_project->width();
    const int height = m_project->height();

    qInfo() << "Starting HW Export (QImage path): 0 to" << endFrame;

    for (int frame = 0; frame < endFrame; ++frame) {
        m_transport->setCurrentFrame(frame);

        // 描画完了待ち
        QEventLoop loop;
        QObject::connect(m_compositeView, &QQuickWindow::afterRendering, &loop, &QEventLoop::quit);
        m_compositeView->update();
        QTimer::singleShot(1000, &loop, &QEventLoop::quit);
        loop.exec();

        // 安定した方法でフレーム取得
        QImage img = m_compositeView->grabWindow();

        // 【修正】フォーマットを明示的に変換 (これでメモリ配列が R, G, B, A の順になる)
        if (img.format() != QImage::Format_RGBA8888) {
            img = img.convertToFormat(QImage::Format_RGBA8888);
        }

        if (img.size() != QSize(width, height)) {
            img = img.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        if (!encoder->pushFrame(img, frame)) {
            qWarning() << "Failed to encode frame" << frame;
            break;
        }

        if (frame % 10 == 0) {
            emit exportProgressChanged((frame * 100) / endFrame);
            QCoreApplication::processEvents();
        }
    }

    encoder->close();
    emit exportProgressChanged(100);
    qInfo() << "HW Export finished.";
}

QImage TimelineController::renderCurrentFrame() const {
    // DEPRECATED: この実装はCPU転送を伴うため非効率
    // 代わりに exportImageSequence() 内の m_compositeView->grabWindow() を使用すべき

    qWarning() << "[パフォーマンス警告] CPUベースのフォールバックレンダリングが使用されました。" << "GPUアクセラレーションを利用するgrabWindow()の使用を検討してください。";
    // フォールバック: 空画像を返す
    QImage img(m_project->width(), m_project->height(), QImage::Format_ARGB32);
    img.fill(Qt::black);
    return img;
}

void TimelineController::selectClip(int id) {
    if (m_timeline)
        m_timeline->selectClip(id);
}

// === エフェクト・オブジェクト操作 ===

QVariantList TimelineController::getAvailableEffects() const {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "filter")
            continue;
        QVariantMap m;
        m["id"] = meta.id;
        m["name"] = meta.name;
        list.append(m);
    }
    return list;
}

QVariantList TimelineController::getAvailableObjects() const {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "object")
            continue;
        QVariantMap m;
        m["id"] = meta.id;
        m["name"] = meta.name;
        list.append(m);
    }
    return list;
}

QVariantList TimelineController::getAvailableObjects(const QString &category) const {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();

    for (const auto &meta : effects) {
        if (meta.category == category) {
            QVariantMap map;
            map["id"] = meta.id;
            map["name"] = meta.name;
            list.append(map);
        }
    }
    return list;
}

void TimelineController::addEffect(int clipId, const QString &effectId) {
    m_timeline->addEffect(clipId, effectId);
    updateActiveClipsList();
}

void TimelineController::removeEffect(int clipId, int effectIndex) {
    m_timeline->removeEffect(clipId, effectIndex);
    updateActiveClipsList();
}

void TimelineController::deleteClip(int clipId) {
    if (m_timeline)
        m_timeline->deleteClip(clipId);
    if (m_selection->selectedClipId() == clipId)
        selectClip(-1);
}

void TimelineController::splitClip(int clipId, int frame) {
    if (m_timeline)
        m_timeline->splitClip(clipId, frame);
}

void TimelineController::copyClip(int clipId) { m_timeline->copyClip(clipId); }

void TimelineController::cutClip(int clipId) { m_timeline->cutClip(clipId); }

void TimelineController::pasteClip(int frame, int layer) { m_timeline->pasteClip(frame, layer); }

QVariantList TimelineController::scenes() const { return m_timeline->scenes(); }

int TimelineController::currentSceneId() const { return m_timeline->currentSceneId(); }

void TimelineController::createScene(const QString &name) { m_timeline->createScene(name); }

void TimelineController::removeScene(int sceneId) { m_timeline->removeScene(sceneId); }

void TimelineController::switchScene(int sceneId) { m_timeline->switchScene(sceneId); }

void TimelineController::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames) { m_timeline->updateSceneSettings(sceneId, name, width, height, fps, totalFrames); }

QVariantList TimelineController::getSceneClips(int sceneId) const {
    QVariantList list;
    const auto &clips = m_timeline->clips(sceneId);

    for (const auto &clip : clips) {
        QVariantMap map;
        map["id"] = clip.id;
        map["type"] = clip.type;
        map["startFrame"] = clip.startFrame;
        map["durationFrames"] = clip.durationFrames;
        map["layer"] = clip.layer;

        // QMLソースの解決
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip.type);
        if (!meta.qmlSource.isEmpty()) {
            map["qmlSource"] = meta.qmlSource;
        } else {
            if (clip.type == "text")
                map["qmlSource"] = "file:objects/TextObject.qml";
            else if (clip.type == "rect")
                map["qmlSource"] = "file:objects/RectObject.qml";
        }

        // パラメータの収集 (エフェクトからフラット化)
        QVariantMap params;
        for (auto *eff : clip.effects) {
            if (!eff->isEnabled())
                continue;
            // ここではキーフレーム評価を行わず、生パラメータまたはデフォルト値を渡す
            // (SceneObject内で時間に応じて評価されるため)
            QVariantMap p = eff->params();
            for (auto it = p.begin(); it != p.end(); ++it)
                params.insert(it.key(), it.value());
        }
        map["params"] = params;
        list.append(map);
    }
    return list;
}

QString TimelineController::debugRunLua(const QString &script) {
    // テスト用に time=currentFrame/fps, index=0, value=0 で実行
    double time = m_transport ? m_transport->currentFrame() / m_project->fps() : 0.0;
    double result = Rina::Scripting::LuaHost::instance().evaluate(script.toStdString(), time, 0, 0.0);
    return QString::number(result);
}

} // namespace Rina::UI