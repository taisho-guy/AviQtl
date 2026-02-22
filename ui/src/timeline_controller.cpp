#include "timeline_controller.hpp"
#include "../../core/include/audio_decoder.hpp"
#include "../../core/include/project_serializer.hpp"
#include "../../core/include/video_decoder.hpp"
#include "../../core/include/video_encoder.hpp"
#include "../../engine/audio_mixer.hpp"
#include "../../engine/plugin/audio_plugin_manager.hpp"
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
#include <QQuickItemGrabResult>
#include <QQuickRenderTarget>
#include <QQuickView>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>

namespace Rina::UI {

TimelineController::TimelineController(QObject *parent) : QObject(parent) {
    // サービスの初期化
    m_project = new ProjectService(this);
    m_transport = new TransportService(this);
    m_selection = new SelectionService(this);
    m_timeline = new TimelineService(m_selection, this);
    m_audioMixer = new Rina::Engine::AudioMixer(this);

    // 初期状態の設定
    m_selection->select(-1, QVariantMap());

    m_clipModel = new ClipModel(m_transport, this);

    // TimelineServiceからのシグナル接続
    connect(m_timeline, &TimelineService::clipsChanged, this, [this]() {
        rebuildClipIndex();

        emit clipsChanged();
        updateMediaDecoders();
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

    // FPSが変更されたら再生タイマーの間隔を更新
    connect(m_project, &ProjectService::fpsChanged, [this]() { m_transport->updateTimerInterval(m_project->fps()); });
    m_transport->updateTimerInterval(m_project->fps());

    // 再生状態の変化をデコーダーに伝播
    connect(m_transport, &TransportService::isPlayingChanged, this, [this]() {
        bool playing = m_transport->isPlaying();
        for (auto *decoder : m_videoDecoders) {
            decoder->setPlaying(playing);
        }
    });

    // 再生位置が変わったらプレビュー更新
    connect(m_transport, &TransportService::currentFrameChanged, this, [this]() {
        int nextFrame = m_transport->currentFrame();
        // ループ再生ロジック
        if (nextFrame > m_timelineDuration && m_timelineDuration > 0) {
            m_transport->setCurrentFrame(0);
            // 音声エンジンのリセットとデコーダのシーク
            for (auto *decoder : m_audioDecoders) {
                decoder->seek(0);
            }
            return; // ループ時はここで処理を切り上げ、0フレーム目に任せる
        }
        updateClipActiveState();

        // アクティブな動画クリップのフレームをシーク
        for (auto it = m_videoDecoders.begin(); it != m_videoDecoders.end(); ++it) {
            const auto *clip = m_timeline->findClipById(it.key());
            if (clip && (nextFrame >= clip->startFrame && nextFrame < clip->startFrame + clip->durationFrames)) {
                it.value()->seekToFrame(nextFrame - clip->startFrame, m_project->fps());
            }
        }

        // 音声処理
        if (m_transport->isPlaying()) {
            // 1フレーム分のサンプル数 (48kHz / fps)
            int samples = 48000 / m_project->fps();
            m_audioMixer->processFrame(nextFrame, m_project->fps(), samples);
        }

        updateActiveClipsList();
    });

    rebuildClipIndex();
    updateClipActiveState();
}

void TimelineController::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    m_videoFrameStore = store;
    qDebug() << "TimelineController: VideoFrameStore set. Updating decoders...";
    updateMediaDecoders();
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

// プロパティアクセサ
void TimelineController::setClipProperty(const QString &name, const QVariant &value) {
    int id = m_selection->selectedClipId();
    if (id == -1)
        return;

    for (auto &clip : m_timeline->clipsMutable()) {
        if (clip.id == id) {
            // 暫定対応：プロパティ名に応じて適切なエフェクトのパラメータを更新する
            bool found = false;
            for (int i = 0; i < clip.effects.size(); ++i) {
                if (clip.effects[i]->params().contains(name)) {
                    // Undo/Redo対応のためコマンド経由で更新
                    updateClipEffectParam(id, i, name, value);

                    // プレビューをアップデート
                    updateActiveClipsList();
                    found = true;
                    break;
                }
            }

            if (!found && !clip.effects.isEmpty()) {
                int targetIndex = 0;
                // transform に属するキーかどうかで対象エフェクトを振り分け
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
    // 矩形判定
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

        // クリップの種類やID以外に表示したい情報があればここに追加する
        // テキストエフェクトがあればそのテキストを表示
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

    // レイヤー順にソート
    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer < b->layer; });

    m_clipModel->updateClips(active);

    // ECSの音声状態も更新
    for (const auto *clip : active) {
        if (clip->type == "audio" || clip->type == "video") {
            float vol = 1.0f;
            float pan = 0.0f;
            bool mute = false;
            // パラメータ取得
            for (auto *eff : clip->effects) {
                if (eff->id() == "audio") {
                    vol = eff->evaluatedParam("volume", m_transport->currentFrame() - clip->startFrame).toFloat();
                    pan = eff->evaluatedParam("pan", m_transport->currentFrame() - clip->startFrame).toFloat();
                    mute = eff->params().value("mute").toBool();
                }
            }
            Rina::Engine::Timeline::ECS::instance().updateAudioClipState(clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }
}

void TimelineController::rebuildClipIndex() {
    m_sortedClips.clear();
    m_maxDuration = 0;
    m_timelineDuration = 0;
    auto &clips = m_timeline->clipsMutable();
    m_sortedClips.reserve(clips.size());
    for (auto &clip : clips) {
        m_sortedClips.push_back(&clip);
        if (clip.durationFrames > m_maxDuration)
            m_maxDuration = clip.durationFrames;

        int end = clip.startFrame + clip.durationFrames;
        if (end > m_timelineDuration)
            m_timelineDuration = end;
    }
    std::sort(m_sortedClips.begin(), m_sortedClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });
}

void TimelineController::updateMediaDecoders() {
    const auto &clips = m_timeline->clips();
    QSet<int> currentVideoClipIds;
    QSet<int> currentAudioClipIds;

    // 新規、既存の動画クリップをチェック
    for (const auto &clip : clips) {
        if (clip.type == "video") {
            currentVideoClipIds.insert(clip.id);

            // 既にデコーダがある場合はスキップ
            if (!m_videoDecoders.contains(clip.id)) {
                // Videoエフェクトを検索
                const EffectModel *videoEffect = nullptr;
                for (const auto *eff : clip.effects) {
                    if (eff->id() == "video") {
                        videoEffect = eff;
                        break;
                    }
                }
                if (videoEffect) {
                    QUrl sourceUrl = QUrl::fromLocalFile(videoEffect->params().value("path").toString());
                    qDebug() << "TimelineController: Found video clip" << clip.id << "path:" << sourceUrl;

                    if (sourceUrl.isValid() && !sourceUrl.isEmpty()) {
                        if (!m_videoFrameStore) {
                            qWarning() << "VideoFrameStore not set!";
                            continue;
                        }
                        auto *decoder = new Rina::Core::VideoDecoder(clip.id, sourceUrl, m_videoFrameStore, this);
                        decoder->setPlaying(m_transport->isPlaying());
                        m_videoDecoders.insert(clip.id, decoder);
                        qDebug() << "TimelineController: VideoDecoder created for clipId:" << clip.id << "source:" << sourceUrl;
                    }
                }
            }
        }
        // 音声クリップのチェック
        if (clip.type == "audio") {
            currentAudioClipIds.insert(clip.id);
            if (!m_audioDecoders.contains(clip.id)) {
                // Audioエフェクトを検索
                const EffectModel *audioEffect = nullptr;
                for (const auto *eff : clip.effects) {
                    if (eff->id() == "audio") {
                        audioEffect = eff;
                        break;
                    }
                }
                if (audioEffect) {
                    QUrl sourceUrl = QUrl::fromLocalFile(audioEffect->params().value("source").toString());
                    if (sourceUrl.isValid() && !sourceUrl.isEmpty()) {
                        auto *decoder = new Rina::Core::AudioDecoder(clip.id, sourceUrl, this);
                        m_audioDecoders.insert(clip.id, decoder);
                        m_audioMixer->registerDecoder(clip.id, decoder);
                        qDebug() << "TimelineController: AudioDecoder created for clipId:" << clip.id;
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
    // 音声デコーダーのクリーンアップ
    for (auto it = m_audioDecoders.begin(); it != m_audioDecoders.end();) {
        if (!currentAudioClipIds.contains(it.key())) {
            m_audioMixer->unregisterDecoder(it.key());
            it.value()->deleteLater();
            it = m_audioDecoders.erase(it);
        } else {
            ++it;
        }
    }
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
    int totalFrames = m_timelineDuration;
    // ディレクトリ作成等の処理は省略（ファイル名として渡される前提）
    // 連番ファイル名を生成するロジックが必要だけどいいや☆
    QString baseName = dir;
    if (baseName.endsWith(".png"))
        baseName.chop(4);

    const int sequencePadding = Rina::Core::SettingsManager::instance().settings().value("exportSequencePadding", 6).toInt();

    if (m_compositeView)
        m_compositeView->setProperty("exportMode", true);
    QQuickItem *view3D = m_compositeView ? m_compositeView->property("view3D").value<QQuickItem *>() : nullptr;
    QQuickItem *targetItem = view3D ? view3D : (m_compositeView ? m_compositeView.data() : nullptr);

    for (int frame = 0; frame < totalFrames; ++frame) {
        m_transport->setCurrentFrame(frame);

        // QMLシーングラフから直接キャプチャ
        QImage renderedFrame;

        if (targetItem) {
            auto grabResult = targetItem->grabToImage(QSize(m_project->width(), m_project->height()));
            if (grabResult) {
                QEventLoop loop;
                connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                // タイムアウト設定 (無限待ち防止)
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                renderedFrame = grabResult->image();
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

    if (m_compositeView)
        m_compositeView->setProperty("exportMode", false);
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
    // ビットレート計算 (簡易): 1080p60fps -> 12Mbps 程度？
    config.bitrate = static_cast<int>(config.width * config.height * m_project->fps() * 0.15);
    config.outputUrl = path;
    config.codecName = "h264_vaapi";

    if (!encoder.open(config)) {
        emit errorOccurred("エンコーダーの初期化に失敗しました。VA-APIドライバを確認してください。");
        return false;
    }

    // 音声ストリーム追加
    encoder.addAudioStream(48000, 2, 192000);

    if (m_compositeView)
        m_compositeView->setProperty("exportMode", true);
    QQuickItem *view3D = m_compositeView ? m_compositeView->property("view3D").value<QQuickItem *>() : nullptr;
    QQuickItem *targetItem = view3D ? view3D : (m_compositeView ? m_compositeView.data() : nullptr);

    int totalFrames = m_timelineDuration;
    for (int frame = 0; frame < totalFrames; ++frame) {
        m_transport->setCurrentFrame(frame);

        // QMLシーングラフからキャプチャ (CPUメモリへのダウンロード発生)
        QImage renderedFrame;
        if (targetItem) {
            auto grabResult = targetItem->grabToImage(QSize(m_project->width(), m_project->height()));
            if (grabResult) {
                QEventLoop loop;
                connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
                QTimer::singleShot(2000, &loop, &QEventLoop::quit);
                loop.exec();
                renderedFrame = grabResult->image();
            }
        } else {
            renderedFrame = renderCurrentFrame();
        }

        // エンコーダーへプッシュ (CPU -> GPU アップロード発生)
        if (!encoder.pushFrame(renderedFrame, frame)) {
            qWarning() << "Frame encoding failed at:" << frame;
        }

        // 音声エンコード
        int samplesNeeded = 48000 / m_project->fps();
        std::vector<float> audioSamples = m_audioMixer->mix(frame, m_project->fps(), samplesNeeded);
        encoder.pushAudio(audioSamples.data(), samplesNeeded);

        emit exportProgressChanged((frame * 100) / totalFrames);
    }

    if (m_compositeView)
        m_compositeView->setProperty("exportMode", false);
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

    const int endFrame = m_timelineDuration;
    const int width = m_project->width();
    const int height = m_project->height();

    qInfo() << "Starting HW Export (QImage path): 0 to" << endFrame;

    // 音声ストリーム追加
    encoder->addAudioStream(48000, 2, 192000);

    m_compositeView->setProperty("exportMode", true);
    QQuickItem *view3D = m_compositeView->property("view3D").value<QQuickItem *>();
    QQuickItem *targetItem = view3D ? view3D : m_compositeView.data();

    for (int frame = 0; frame < endFrame; ++frame) {
        m_transport->setCurrentFrame(frame);

        // grabToImage はレンダリングをスケジュールし、完了を通知する
        QImage img;
        auto grabResult = targetItem->grabToImage(QSize(width, height));
        if (grabResult) {
            QEventLoop loop;
            connect(grabResult.get(), &QQuickItemGrabResult::ready, &loop, &QEventLoop::quit);
            QTimer::singleShot(2000, &loop, &QEventLoop::quit);
            loop.exec();
            img = grabResult->image();
        }

        if (!encoder->pushFrame(img, frame)) {
            qWarning() << "Failed to encode frame" << frame;
            break;
        }

        // 音声エンコード
        int samplesNeeded = 48000 / m_project->fps();
        std::vector<float> audioSamples = m_audioMixer->mix(frame, m_project->fps(), samplesNeeded);
        encoder->pushAudio(audioSamples.data(), samplesNeeded);

        if (frame % 10 == 0) {
            emit exportProgressChanged((frame * 100) / endFrame);
            QCoreApplication::processEvents();
        }
    }

    m_compositeView->setProperty("exportMode", false);
    encoder->close();
    emit exportProgressChanged(100);
    qInfo() << "HW Export finished.";
}

QImage TimelineController::renderCurrentFrame() const {
    // この実装はCPU転送を伴うため非効率
    // 代わりに m_compositeView->grabWindow() を使用すべき

    qWarning() << "[パフォーマンス警告] CPUベースのフォールバックレンダリングが使用されました。" << "GPUアクセラレーションを利用するgrabWindow()の使用を検討してください。";
    // 実装しません
    QImage img(m_project->width(), m_project->height(), QImage::Format_ARGB32);
    img.fill(Qt::black);
    return img;
}

void TimelineController::selectClip(int id) {
    if (m_timeline)
        m_timeline->selectClip(id);
}

// エフェクト・オブジェクト操作

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

QVariantList TimelineController::getAvailableAudioPlugins() const { return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginList(); }

void TimelineController::addAudioPlugin(int clipId, const QString &pluginId) {
    auto plugin = Rina::Engine::Plugin::AudioPluginManager::instance().createPlugin(pluginId);
    if (plugin) {
        qInfo() << "Adding audio plugin:" << plugin->name() << "to clip" << clipId;
        m_audioMixer->getChain(clipId).add(std::move(plugin));
        emit clipEffectsChanged(clipId);
    } else {
        qWarning() << "Failed to create audio plugin:" << pluginId;
    }
}

void TimelineController::removeAudioPlugin(int clipId, int index) {
    m_audioMixer->getChain(clipId).remove(index);
    emit clipEffectsChanged(clipId);
}

QVariantList TimelineController::getPluginCategories() const {
    // AudioPluginManagerから重複のないカテゴリ名リストを抽出
    return Rina::Engine::Plugin::AudioPluginManager::instance().getCategories();
}

QVariantList TimelineController::getPluginsByCategory(const QString &category) const {
    // 特定カテゴリに属するプラグインのみを返す
    return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginsInCategory(category);
}

bool TimelineController::isAudioClip(int clipId) const {
    const auto *clip = m_timeline->findClipById(clipId);
    return clip && clip->type == "audio";
}

QVariantList TimelineController::getClipEffectStack(int clipId) const {
    QVariantList list;
    if (clipId < 0)
        return list;

    auto &chain = m_audioMixer->getChain(clipId);
    for (int i = 0; i < chain.count(); ++i) {
        auto *plugin = chain.get(i);
        if (plugin) {
            QVariantMap effectInfo;
            effectInfo["name"] = plugin->name();
            effectInfo["format"] = plugin->format();
            list.append(effectInfo);
        }
    }
    return list;
}

QVariantList TimelineController::getEffectParameters(int clipId, int effectIndex) const {
    QVariantList list;
    if (clipId < 0)
        return list;

    auto &chain = m_audioMixer->getChain(clipId);
    auto *plugin = chain.get(effectIndex);
    if (plugin) {
        for (int i = 0; i < plugin->paramCount(); ++i) {
            QVariantMap paramInfo;
            auto info = plugin->getParamInfo(i);

            paramInfo["pIdx"] = i;
            paramInfo["name"] = info.name;
            paramInfo["current"] = plugin->getParam(i);
            paramInfo["min"] = info.min;
            paramInfo["max"] = info.max;

            if (info.isToggle)
                paramInfo["type"] = "bool";
            else if (info.isInteger)
                paramInfo["type"] = "int";
            else
                paramInfo["type"] = "slider";

            list.append(paramInfo);
        }
    }
    return list;
}

void TimelineController::setEffectParameter(int clipId, int effectIndex, int paramIndex, float value) {
    if (clipId < 0)
        return;
    auto &chain = m_audioMixer->getChain(clipId);
    auto *plugin = chain.get(effectIndex);
    if (plugin) {
        plugin->setParam(paramIndex, value);
    }
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
            // キーフレーム評価を行わず生パラメータまたはデフォルト値を渡す
            // SceneObject内で時間に応じて評価されるため
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