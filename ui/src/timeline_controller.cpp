#include "timeline_controller.hpp"
#include "../../core/include/project_serializer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include <QImage>
#include <QQuickWindow>
#include "clip_model.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "project_service.hpp"
#include "selection_service.hpp"
#include "timeline_service.hpp"
#include "transport_service.hpp"
#include <QFile>
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

    // Phase 1: 初期状態の明示的設定
    m_selection->select(-1, QVariantMap());

    m_clipModel = new ClipModel(m_transport, this);

    // TimelineService signals
    connect(m_timeline, &TimelineService::clipsChanged, this, [this]() {
        rebuildClipIndex();
        emit clipsChanged();
        updateActiveClipsList();
    });
    connect(m_timeline, &TimelineService::clipEffectsChanged, this, &TimelineController::clipEffectsChanged);
    // シーン機能は未実装のためコメントアウト
    // connect(m_timeline, &TimelineService::scenesChanged, this, &TimelineController::scenesChanged);
    // connect(m_timeline, &TimelineService::currentSceneIdChanged, this, &TimelineController::currentSceneIdChanged);

    // SelectionService signals
    connect(m_selection, &SelectionService::selectedClipDataChanged, this, [this]() {
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
        updateClipActiveState();
    });

    // サービス間の連携
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
        updateActiveClipsList();
    });

    rebuildClipIndex();
    updateClipActiveState();
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

// --- Generic Property Accessors ---
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
                    // コマンド経由で更新 (Undo/Redo対応)
                    updateClipEffectParam(id, i, name, value);

                    // Update cache (flattened view for current UI)
                    // updateClipEffectParam内でSelection更新も行われるため削除

                    // Update preview
                    updateActiveClipsList();
                    found = true;
                    break;
                }
            }

            if (!found && !clip.effects.isEmpty()) {
                int targetIndex = 0;
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
    // 現在フレームにあるクリップを抽出
    int current = m_transport->currentFrame();
    QList<ClipData *> active;

    // Binary search for the first clip that starts AFTER current frame
    auto it = std::upper_bound(m_sortedClips.begin(), m_sortedClips.end(), current, [](int frame, const ClipData *clip) { return frame < clip->startFrame; });

    // Iterate backwards to find overlapping clips
    // Stop if startFrame is too far back (cannot overlap even with maxDuration)
    int threshold = current - m_maxDuration;

    for (auto i = it; i != m_sortedClips.begin();) {
        --i;
        ClipData *clip = *i;
        if (clip->startFrame < threshold)
            break;
        if (current >= clip->startFrame && current < clip->startFrame + clip->durationFrames) {
            active.append(clip);
        }
    }

    // レイヤー順（昇順）にソート
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
        qWarning() << "Failed to open project file for info:" << path;
        return result;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse project JSON:" << error.errorString();
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
    if (localPath.isEmpty()) localPath = fileUrl;

    if (format == "image_sequence") {
        return exportImageSequence(localPath, quality);
    } else if (format == "avi" || format == "mp4") {
        return exportVideo(localPath, format, quality);
    }

    qWarning() << "Unsupported export format:" << format;
    return false;
}

bool TimelineController::exportImageSequence(const QString &dir, int quality) {
    int totalFrames = m_project->totalFrames();
    // ディレクトリ作成等の処理は省略（ファイル名として渡される前提）
    // 実際は連番ファイル名を生成するロジックが必要
    QString baseName = dir;
    if (baseName.endsWith(".png")) baseName.chop(4);

    for (int frame = 0; frame < totalFrames; ++frame) {
        m_transport->setCurrentFrame(frame);

        // GPU→CPUコピーを最小化: QMLシーングラフから直接キャプチャ
        QImage renderedFrame;

        if (m_compositeView) {
            // オプション1: QQuickWindow::grabWindow() (推奨)
            // GPU→CPUコピーは1回のみ、効率的
            renderedFrame = m_compositeView->grabWindow();

            // プロジェクト解像度にリサイズ（必要な場合）
            QSize targetSize(m_project->width(), m_project->height());
            if (renderedFrame.size() != targetSize) {
                renderedFrame = renderedFrame.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
        } else {
            qWarning() << "CompositeView not set, using fallback rendering";
            renderedFrame = renderCurrentFrame();
        }

        QString filename = QString("%1_%2.png").arg(baseName).arg(frame, 6, 10, QChar('0'));

        if (!renderedFrame.save(filename, "PNG", quality)) {
            qWarning() << "Failed to save frame:" << filename;
            return false;
        }

        emit exportProgressChanged((frame * 100) / totalFrames);
    }

    emit exportProgressChanged(100);
    return true;
}

bool TimelineController::exportVideo(const QString &path, const QString &format, int quality) {
    qWarning() << "Video export not implemented yet (requires FFmpeg integration)";
    return false;
}

QImage TimelineController::renderCurrentFrame() const {
    // DEPRECATED: この実装はCPU転送を伴うため非効率
    // 代わりに exportImageSequence() 内で m_compositeView->grabWindow() を使用

    qWarning() << "[Performance Warning] CPU-based rendering fallback used. " << "Consider using GPU-accelerated grabWindow() instead.";
    // フォールバック: 空画像を返す
    QImage img(m_project->width(), m_project->height(), QImage::Format_ARGB32);
    img.fill(Qt::black);
    return img;
}

void TimelineController::selectClip(int id) {
    if (m_timeline)
        m_timeline->selectClip(id);
}

// === エフェクト操作 (Internal実装) ===

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

void TimelineController::addEffect(int clipId, const QString &effectId) { m_timeline->addEffect(clipId, effectId); }

void TimelineController::removeEffect(int clipId, int effectIndex) { m_timeline->removeEffect(clipId, effectIndex); }

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

// シーン機能は未実装のため、TimelineServiceへの呼び出しを削除してダミー値を返す
QVariantList TimelineController::scenes() const {
    return QVariantList();
}

int TimelineController::currentSceneId() const {
    return 0;
}

void TimelineController::createScene(const QString &name) {
    qWarning() << "createScene not implemented:" << name;
}

void TimelineController::removeScene(int sceneId) {
    Q_UNUSED(sceneId);
}

void TimelineController::switchScene(int sceneId) {
    Q_UNUSED(sceneId);
}

QVariantList TimelineController::getSceneClips(int sceneId) const {
    Q_UNUSED(sceneId);
    return QVariantList();
}

} // namespace Rina::UI