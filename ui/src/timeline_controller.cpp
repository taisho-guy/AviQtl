#include "timeline_controller.hpp"
#include "../../core/include/audio_decoder.hpp"
#include "../../core/include/project_serializer.hpp"
#include "../../core/include/video_decoder.hpp"
#include "../../engine/plugin/audio_plugin_manager.hpp"
#include "../../scripting/lua_host.hpp"
#include "commands.hpp"
#include "core/include/video_frame_store.hpp"
#include "effect_registry.hpp"
#include "project_service.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include "timeline_service.hpp"
#include "transport_service.hpp"
#include <QFile>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>

namespace Rina::UI {

TimelineController::TimelineController(QObject *parent) : QObject(parent) {
    initializeServices();
    setupConnections();

    // 初期状態の設定
    m_selection->select(-1, QVariantMap());
    m_engineSync->rebuildClipIndex();
    updateClipActiveState();
}

void TimelineController::initializeServices() {
    m_project = new ProjectService(this);
    m_transport = new TransportService(this);
    m_selection = new SelectionService(this);
    m_timeline = new TimelineService(m_selection, this);

    m_mediaManager = new TimelineMediaManager(this, this);
    m_engineSync = new TimelineEngineSynchronizer(this, this);
    m_exportManager = new TimelineExportManager(this, this);
}

void TimelineController::setupConnections() {
    connect(
        m_timeline, &TimelineService::clipsChanged, this,
        [this]() -> void {
            m_engineSync->rebuildClipIndex();
            emit clipsChanged();
            m_mediaManager->updateMediaDecoders();
            m_mediaManager->onCurrentFrameChanged();
            updateActiveClipsList();
            m_transport->setTotalFrames(timelineDuration());
        },
        Qt::QueuedConnection);

    connect(m_selection, &SelectionService::selectedClipDataChanged, this, [this]() -> void {
        emit clipStartFrameChanged();
        emit clipDurationFramesChanged();
        emit layerChanged();
        emit activeObjectTypeChanged();
        updateClipActiveState();
    });

    connect(m_timeline, &TimelineService::scenesChanged, this, [this]() -> void {
        m_mediaManager->updateMediaDecoders();
        m_mediaManager->onCurrentFrameChanged();
        updateActiveClipsList();
        emit scenesChanged();
    });
    connect(m_timeline, &TimelineService::currentSceneIdChanged, this, &TimelineController::currentSceneIdChanged);
    connect(m_timeline, &TimelineService::clipEffectsChanged, this, [this](int id) -> void {
        m_mediaManager->onCurrentFrameChanged();
        updateActiveClipsList();
        emit clipEffectsChanged(id);
    });
    connect(m_timeline, &TimelineService::effectParamChanged, this, [this]() -> void {
        m_mediaManager->onCurrentFrameChanged();
        updateActiveClipsList();
    });

    // 画像や動画の準備ができたらUI側に再描画を促す
    connect(m_mediaManager, &TimelineMediaManager::frameUpdated, this, &TimelineController::clipEffectsChanged);

    connect(m_exportManager, &TimelineExportManager::exportStarted, this, &TimelineController::exportStarted);
    connect(m_exportManager, &TimelineExportManager::exportProgressChanged, this, &TimelineController::exportProgressChanged);
    connect(m_exportManager, &TimelineExportManager::exportFinished, this, &TimelineController::exportFinished);

    // FPSが変更されたら再生タイマーの間隔を更新
    connect(m_project, &ProjectService::fpsChanged, this, [this]() -> void { m_transport->updateTimerInterval(m_project->fps()); });
    m_transport->updateTimerInterval(m_project->fps());

    // 再生状態の変化をデコーダーに伝播
    connect(m_transport, &TransportService::isPlayingChanged, this, &TimelineController::onPlayingChanged);

    // 再生位置が変わったらプレビュー更新
    connect(m_transport, &TransportService::currentFrameChanged, this, &TimelineController::onCurrentFrameChanged);

    // QML(VideoObject)からのフレーム要求をMediaManagerへ中継
    connect(this, &TimelineController::videoFrameRequested, m_mediaManager, &TimelineMediaManager::requestVideoFrame);
    connect(this, &TimelineController::imageLoadRequested, m_mediaManager, &TimelineMediaManager::requestImageLoad);
}

void TimelineController::onPlayingChanged() { m_mediaManager->onPlayingChanged(); }

void TimelineController::onCurrentFrameChanged() {
    [[maybe_unused]] int nextFrame = m_transport->currentFrame();
    m_mediaManager->onCurrentFrameChanged();
    updateActiveClipsList();
}

void TimelineController::handleClipClick(int clipId, int modifiers) { // NOLINT(bugprone-easily-swappable-parameters)
    if ((modifiers & Qt::ControlModifier) != 0U) {
        m_timeline->toggleSelection(clipId, QVariantMap());
    } else {
        m_timeline->applySelectionIds({clipId});
    }
}

void TimelineController::updateSelectionPreview(int frameA, int frameB, int layerA, int layerB, bool additive) { // NOLINT(bugprone-easily-swappable-parameters)
    QVariantList ids;
    if (additive && (m_selection != nullptr)) {
        ids = m_selection->selectedClipIds();
    }

    int minF = std::min(frameA, frameB);
    int maxF = std::max(frameA, frameB);
    int minL = std::min(layerA, layerB);
    int maxL = std::max(layerA, layerB);

    for (const auto &clip : m_timeline->clips()) {
        // GroupControlによるレイヤーの拡張分を考慮
        int groupLayerCount = 0;
        for (auto *eff : clip.effects) {
            if (eff->id() == QLatin1String("GroupControl")) {
                groupLayerCount = eff->params().value(QStringLiteral("layerCount"), 0).toInt();
                break;
            }
        }
        int clipMaxL = clip.layer + groupLayerCount;

        int clipEnd = clip.startFrame + clip.durationFrames;
        if (clip.startFrame < maxF && minF < clipEnd && clipMaxL >= minL && clip.layer <= maxL) {
            if (!ids.contains(clip.id)) {
                ids.append(clip.id);
            }
        }
    }

    if (m_previewSelectionIds != ids) {
        m_previewSelectionIds = ids;
        emit previewSelectionIdsChanged();
    }
}

void TimelineController::finalizeSelectionPreview() {
    applySelectionIds(m_previewSelectionIds);
    clearSelectionPreview();
}

void TimelineController::clearSelectionPreview() {
    if (!m_previewSelectionIds.isEmpty()) {
        m_previewSelectionIds.clear();
        emit previewSelectionIdsChanged();
    }
}

auto TimelineController::previewSelectionIds() const -> QVariantList { return m_previewSelectionIds; }

void TimelineController::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    qDebug() << "TimelineController: VideoFrameStore set. Updating decoders...";
    m_mediaManager->setVideoFrameStore(store);
}

void TimelineController::togglePlay() {
    if (m_transport != nullptr) {
        m_transport->togglePlay();
    }
}

void TimelineController::undo() { m_timeline->undo(); }
void TimelineController::redo() { m_timeline->redo(); }

auto TimelineController::timelineScale() const -> double { return m_timelineScale; }
void TimelineController::setTimelineScale(double scale) {
    if (qAbs(m_timelineScale - scale) > 0.001) {
        m_timelineScale = scale;
        emit timelineScaleChanged();
    }
}

// プロパティアクセサ
void TimelineController::setClipProperty(const QString &name, const QVariant &value) {
    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    m_timeline->undoStack()->beginMacro(tr("プロパティ変更: %1").arg(name));

    for (const QVariant &vId : ids) {
        int id = vId.toInt();
        const ClipData *clip = m_timeline->findClipById(id);
        if (clip == nullptr) {
            continue;
        }

        int targetEffectIndex = -1;
        for (int i = 0; i < clip->effects.size(); ++i) {
            if (clip->effects.value(i)->params().contains(name)) {
                targetEffectIndex = i;
                break;
            }
        }

        if (targetEffectIndex == -1 && !clip->effects.isEmpty()) {
            targetEffectIndex = 0;
            static const QStringList transformKeys = {"x", "y", "z", "scale", "aspect", "rotationX", "rotationY", "rotationZ", "opacity"};
            if (!transformKeys.contains(name) && clip->effects.size() > 1) {
                targetEffectIndex = 1;
            }
        }

        if (targetEffectIndex != -1 && targetEffectIndex < clip->effects.size()) {
            updateClipEffectParam(id, targetEffectIndex, name, value);
        }
    }

    m_timeline->undoStack()->endMacro();
}

auto TimelineController::getClipProperty(const QString &name) const -> QVariant { return m_selection->selectedClipData().value(name); }

auto TimelineController::clipStartFrame() const -> int { return m_selection->selectedClipData().value(QStringLiteral("startFrame"), 0).toInt(); }
void TimelineController::setClipStartFrame(int frame) {
    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    m_timeline->undoStack()->beginMacro(tr("開始フレーム変更"));
    for (const QVariant &vId : ids) {
        int id = vId.toInt();
        if (const auto *c = m_timeline->findClipById(id)) {
            m_timeline->updateClip(id, c->layer, frame, c->durationFrames);
        }
    }
    m_timeline->undoStack()->endMacro();
}

auto TimelineController::clipDurationFrames() const -> int { return m_selection->selectedClipData().value(QStringLiteral("durationFrames"), 100).toInt(); }
void TimelineController::setClipDurationFrames(int frames) {
    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    m_timeline->undoStack()->beginMacro(tr("長さ変更"));
    for (const QVariant &vId : ids) {
        int id = vId.toInt();
        if (const auto *c = m_timeline->findClipById(id)) {
            m_timeline->updateClip(id, c->layer, c->startFrame, frames);
        }
    }
    m_timeline->undoStack()->endMacro();
}

auto TimelineController::layer() const -> int { return m_selection->selectedClipData().value(QStringLiteral("layer"), 0).toInt(); }
void TimelineController::setLayer(int layer) {
    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    m_timeline->undoStack()->beginMacro(tr("レイヤー変更"));
    for (const QVariant &vId : ids) {
        int id = vId.toInt();
        if (const auto *c = m_timeline->findClipById(id)) {
            m_timeline->updateClip(id, layer, c->startFrame, c->durationFrames);
        }
    }
    m_timeline->undoStack()->endMacro();
}

void TimelineController::setSelectedLayer(int layer) {
    if (m_selectedLayer != layer) {
        m_selectedLayer = layer;
        emit selectedLayerChanged();
    }
}

auto TimelineController::isClipActive() const -> bool { return m_isClipActive; }

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
}

auto TimelineController::activeObjectType() const -> QString { return m_selection->selectedClipData().value(QStringLiteral("type"), "rect").toString(); }

void TimelineController::createObject(const QString &type, int startFrame, int layer) {
    if (m_timeline != nullptr) {
        m_timeline->createClip(type, startFrame, layer);
    }
}

auto TimelineController::getClipEffectsModel(int clipId) const -> QList<QObject *> {
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

auto TimelineController::clips() const -> QVariantList {
    QVariantList list;
    for (const auto &clip : m_timeline->clips()) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), clip.id);
        map.insert(QStringLiteral("type"), clip.type);
        map.insert(QStringLiteral("startFrame"), clip.startFrame);
        map.insert(QStringLiteral("durationFrames"), clip.durationFrames);
        map.insert(QStringLiteral("layer"), clip.layer);

        // オブジェクトのQMLパスを取得して追加
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip.type);
        map.insert(QStringLiteral("name"), !meta.name.isEmpty() ? meta.name : clip.type);
        if (!meta.qmlSource.isEmpty()) {
            map.insert(QStringLiteral("qmlSource"), meta.qmlSource);
        }

        // params を構築して追加
        QVariantMap params;
        // 基本情報もparamsに入れておく（QML側での利便性とBaseObjectでの参照用）
        params.insert(QStringLiteral("layer"), clip.layer);
        params.insert(QStringLiteral("startFrame"), clip.startFrame);
        params.insert(QStringLiteral("durationFrames"), clip.durationFrames);
        params.insert(QStringLiteral("id"), clip.id);

        int groupLayerCount = 0;
        for (auto *eff : clip.effects) {
            if (eff->id() == QLatin1String("GroupControl")) {
                groupLayerCount = eff->params().value(QStringLiteral("layerCount"), 0).toInt();
                break;
            }
        }
        map.insert(QStringLiteral("groupLayerCount"), groupLayerCount);

        for (auto *eff : clip.effects) {
            QVariantMap p = eff->params();
            for (auto it = p.begin(); it != p.end(); ++it) {
                params.insert(it.key(), it.value());
            }
        }
        map.insert(QStringLiteral("params"), params);

        list.append(map);
    }
    return list;
}

void TimelineController::updateActiveClipsList() { m_engineSync->updateActiveClipsList(); }

void TimelineController::log(const QString &msg) { qDebug() << "[TimelineBridge] " << msg; }

void TimelineController::moveSelectedClips(int deltaLayer, int deltaFrame) {
    if (m_timeline != nullptr) {
        m_timeline->moveSelectedClips(deltaLayer, deltaFrame);
    }
}

void TimelineController::applyClipBatchMove(const QVariantList &moves) {
    if (m_timeline != nullptr) {
        m_timeline->applyClipBatchMove(moves);
    }
}

void TimelineController::resizeSelectedClips(int deltaStartFrame, int deltaDuration) {
    if (m_timeline != nullptr) {
        m_timeline->resizeSelectedClips(deltaStartFrame, deltaDuration);
    }
}

void TimelineController::updateClip(int id, int layer, int startFrame, int duration) {
    const auto *clip = m_timeline->findClipById(id);

    if (clip != nullptr) {
        const int projectFps = static_cast<int>(project()->fps());

        if (clip->type == QLatin1String("video")) {
            auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(m_mediaManager->decoderForClip(id));
            if ((vid != nullptr) && vid->isReady()) {
                int startVideoFrame = 0;
                double speed = 100.0;
                bool isDirectMode = false;

                for (const auto *eff : clip->effects) {
                    if (eff->id() != "video") {
                        continue;
                    }
                    const QString playMode = eff->params().value(QStringLiteral("playMode"), "開始フレーム＋再生速度").toString();
                    if (playMode == QStringLiteral("フレーム直接指定")) {
                        isDirectMode = true;
                        break;
                    }
                    startVideoFrame = eff->params().value(QStringLiteral("startFrame"), 0).toInt();
                    speed = eff->params().value(QStringLiteral("speed"), 100.0).toDouble();
                    break;
                }

                double srcFps = vid->sourceFps();
                if (srcFps <= 0.0) {
                    srcFps = projectFps;
                }
                int maxDuration = duration;

                if (isDirectMode) {
                    // フレーム直接指定: 単純に「素材の総フレーム数 / srcFps * projectFps」を限界とする
                    const double totalSec = static_cast<double>(vid->totalFrameCount()) / srcFps;
                    maxDuration = static_cast<int>(totalSec * projectFps);
                } else if (speed > 0.0) {
                    const double startSec = static_cast<double>(startVideoFrame) / srcFps;
                    const double remainingSec = (static_cast<double>(vid->totalFrameCount()) / srcFps) - startSec;
                    if (remainingSec > 0.0) {
                        maxDuration = static_cast<int>(remainingSec / (speed / 100.0) * projectFps);
                    }
                }

                if (maxDuration > 0 && duration > maxDuration) {
                    duration = maxDuration;
                }
            }
        } else if (clip->type == QLatin1String("audio")) {
            auto *aud = qobject_cast<Rina::Core::AudioDecoder *>(m_mediaManager->decoderForClip(id));
            if ((aud != nullptr) && aud->isReady()) {
                double startTime = 0.0;
                double speed = 100.0;
                bool isDirectMode = false;

                for (const auto *eff : clip->effects) {
                    if (eff->id() != "audio") {
                        continue;
                    }
                    const QString playMode = eff->params().value(QStringLiteral("playMode"), "開始時間＋再生速度").toString();
                    if (playMode == QStringLiteral("時間直接指定")) {
                        isDirectMode = true;
                        break;
                    }
                    startTime = eff->params().value(QStringLiteral("startTime"), 0.0).toDouble();
                    speed = eff->params().value(QStringLiteral("speed"), 100.0).toDouble();
                    break;
                }

                const double totalSec = aud->totalDurationSec();
                int maxDuration = duration;

                if (isDirectMode) {
                    // 時間直接指定: 素材の総秒数 * projectFps を限界とする
                    maxDuration = static_cast<int>(totalSec * projectFps);
                } else if (speed > 0.0) {
                    const double remainingSec = totalSec - startTime;
                    if (remainingSec > 0.0) {
                        maxDuration = static_cast<int>(remainingSec / (speed / 100.0) * projectFps);
                    }
                }

                if (maxDuration > 0 && duration > maxDuration) {
                    duration = maxDuration;
                }
            }
        } else if (clip->type == QLatin1String("scene")) {
            int targetSceneId = 0;
            double speed = 1.0;
            int offset = 0;
            [[maybe_unused]] bool isDirectMode = false;

            for (const auto *eff : clip->effects) {
                if (eff->id() != "scene") {
                    continue;
                }
                // ※ 現在Sceneには playMode パラメータは無いが、将来のために概念として考慮。
                // 現状の仕様では「speed と offset」による計算。
                targetSceneId = eff->params().value(QStringLiteral("targetSceneId"), 0).toInt();
                speed = eff->params().value(QStringLiteral("speed"), 1.0).toDouble();
                offset = eff->params().value(QStringLiteral("offset"), 0).toInt();
                break;
            }

            const int sceneDur = getSceneDuration(targetSceneId);
            if (sceneDur > 0 && speed > 0.0) {
                const double rhs = (static_cast<double>(sceneDur - 1 - offset)) / speed;
                int maxDuration = static_cast<int>(rhs) + 1;
                maxDuration = std::max(maxDuration, 1);
                duration = std::min(duration, maxDuration);
            }
        }
    }

    m_timeline->updateClip(id, layer, startFrame, duration);
}

void TimelineController::moveClipWithCollisionCheck(int clipId, int layer, int startFrame) {
    const ClipData *clip = m_timeline->findClipById(clipId);
    if (clip == nullptr) {
        return;
    }

    int duration = clip->durationFrames;
    int fixedStart = m_timeline->findVacantFrame(layer, startFrame, duration, clipId);
    updateClip(clipId, layer, fixedStart, duration);
}

auto TimelineController::saveProject(const QString &fileUrl) -> bool {
    // 渡されたパスが空の場合は内部で保持しているパスを割り当てる
    QString targetUrl = fileUrl.isEmpty() ? m_currentProjectUrl : fileUrl;

    // パスが空の場合は新規作成直後なのでエラーで返す
    if (targetUrl.isEmpty()) {
        emit errorOccurred(tr("保存先のファイルパスが不明です"));
        return false;
    }

    QString error;
    bool result = Rina::Core::ProjectSerializer::save(targetUrl, m_timeline, m_project, &error);

    if (result) {
        // 保存に成功したパスを現在のプロジェクトパスとして記憶する
        m_currentProjectUrl = targetUrl;
        emit currentProjectUrlChanged();
    } else {
        emit errorOccurred(error);
    }
    return result;
}

auto TimelineController::loadProject(const QString &fileUrl) -> bool {
    QString error;
    bool result = Rina::Core::ProjectSerializer::load(fileUrl, m_timeline, m_project, &error);

    if (result) {
        // 読み込みに成功したパスを現在のプロジェクトパスとして記憶する
        m_currentProjectUrl = fileUrl;
        emit currentProjectUrlChanged();
    } else {
        emit errorOccurred(error);
    }
    return result;
}

auto TimelineController::getProjectInfo(const QString &fileUrl) -> QVariantMap {
    QVariantMap result;
    QString path = QUrl(fileUrl).toLocalFile();
    if (path.isEmpty()) {
        path = fileUrl;
    }

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
    if (root.contains(QStringLiteral("settings"))) {
        result = root.value(QStringLiteral("settings")).toObject().toVariantMap();
    }

    return result;
}

auto TimelineController::exportMedia(const QString &fileUrl, const QString &format, int quality) -> bool { return m_exportManager->exportMedia(fileUrl, format, quality); }

void TimelineController::exportVideoAsync(const QVariantMap &cfg) {
    Rina::Core::VideoEncoder::Config c;
    c.width = cfg.value(QStringLiteral("width"), 1920).toInt();
    c.height = cfg.value(QStringLiteral("height"), 1080).toInt();
    c.fps_num = cfg.value(QStringLiteral("fps_num"), 60000).toInt();
    c.fps_den = cfg.value(QStringLiteral("fps_den"), 1000).toInt();
    c.bitrate = cfg.value(QStringLiteral("bitrate"), 15'000'000).toLongLong();
    c.crf = cfg.value(QStringLiteral("crf"), -1).toInt();
    c.codecName = cfg.value(QStringLiteral("codecName"), "h264_vaapi").toString();
    c.audioCodecName = cfg.value(QStringLiteral("audioCodecName"), "aac").toString();
    c.audioBitrate = cfg.value(QStringLiteral("audioBitrate"), 192'000).toLongLong();
    c.outputUrl = cfg.value(QStringLiteral("outputUrl")).toString();
    c.startFrame = cfg.value(QStringLiteral("startFrame"), 0).toInt();
    c.endFrame = cfg.value(QStringLiteral("endFrame"), -1).toInt();
    m_exportManager->exportVideoAsync(c);
}

void TimelineController::cancelExport() { m_exportManager->cancelExport(); }
auto TimelineController::isExporting() const -> bool { return m_exportManager->isExporting(); }

void TimelineController::selectClip(int id) {
    if (m_timeline != nullptr) {
        m_timeline->applySelectionIds(QVariantList{id});
    }
}

void TimelineController::selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive) {
    if (m_timeline != nullptr) {
        m_timeline->selectClipsInRange(frameA, frameB, layerA, layerB, additive);
    }
}

void TimelineController::toggleSelection(int id, const QVariantMap &data) {
    if (m_timeline != nullptr) {
        m_timeline->toggleSelection(id, data);
    }
}

void TimelineController::applySelectionIds(const QVariantList &ids) {
    if (m_timeline != nullptr) {
        m_timeline->applySelectionIds(ids);
    }
}

// エフェクト・オブジェクト操作

auto TimelineController::getAvailableEffects() -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "filter") {
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("id"), meta.id);
        m.insert(QStringLiteral("name"), meta.name);
        list.append(m);
    }
    return list;
}

auto TimelineController::getAvailableObjects() -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();
    for (const auto &meta : effects) {
        if (meta.category != "object") {
            continue;
        }
        QVariantMap m;
        m.insert(QStringLiteral("id"), meta.id);
        m.insert(QStringLiteral("name"), meta.name);
        list.append(m);
    }
    return list;
}

auto TimelineController::getClipTypeColor(const QString &type) -> QString { return Rina::Core::EffectRegistry::instance().getEffect(type).color; }

auto TimelineController::getAvailableObjects(const QString &category) -> QVariantList {
    QVariantList list;
    const auto effects = Rina::Core::EffectRegistry::instance().getAllEffects();

    for (const auto &meta : effects) {
        if (meta.category == category) {
            QVariantMap map;
            map.insert(QStringLiteral("id"), meta.id);
            map.insert(QStringLiteral("name"), meta.name);
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

void TimelineController::setEffectEnabled(int clipId, int effectIndex, bool enabled) {
    if (m_timeline != nullptr) {
        m_timeline->setEffectEnabled(clipId, effectIndex, enabled);
    }
}

void TimelineController::reorderEffects(int clipId, int oldIndex, int newIndex) {
    if (m_timeline != nullptr) {
        m_timeline->reorderEffects(clipId, oldIndex, newIndex);
    }
}

void TimelineController::copyEffect(int clipId, int effectIndex) { m_timeline->copyEffect(clipId, effectIndex); }

void TimelineController::pasteEffect(int clipId, int targetIndex) { m_timeline->pasteEffect(clipId, targetIndex); }

void TimelineController::cutEffect(int clipId, int effectIndex) {
    m_timeline->copyEffect(clipId, effectIndex);
    m_timeline->removeEffect(clipId, effectIndex);
}

auto TimelineController::getAvailableAudioPlugins() -> QVariantList { return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginList(); }

void TimelineController::addAudioPlugin(int clipId, const QString &pluginId) {
    auto plugin = Rina::Engine::Plugin::AudioPluginManager::instance().createPlugin(pluginId);
    if (plugin) {
        qInfo() << "Adding audio plugin:" << plugin->name() << "to clip" << clipId;
        m_mediaManager->audioMixer()->getChain(clipId).add(std::move(plugin));
        emit clipEffectsChanged(clipId);
    } else {
        qWarning() << "Failed to create audio plugin:" << pluginId;
    }
}

void TimelineController::removeAudioPlugin(int clipId, int index) {
    m_mediaManager->audioMixer()->getChain(clipId).remove(index);
    emit clipEffectsChanged(clipId);
}

void TimelineController::setAudioPluginEnabled(int clipId, int index, bool enabled) {
    if (m_timeline != nullptr) {
        m_timeline->setAudioPluginEnabled(clipId, index, enabled);
    }
}

void TimelineController::reorderAudioPlugins(int clipId, int oldIndex, int newIndex) {
    if (m_timeline != nullptr) {
        m_timeline->reorderAudioPlugins(clipId, oldIndex, newIndex);
    }
}

auto TimelineController::getPluginCategories() -> QVariantList {
    // AudioPluginManagerから重複のないカテゴリ名リストを抽出
    return Rina::Engine::Plugin::AudioPluginManager::instance().getCategories();
}

auto TimelineController::getPluginsByCategory(const QString &category) -> QVariantList {
    // 特定カテゴリに属するプラグインのみを返す
    return Rina::Engine::Plugin::AudioPluginManager::instance().getPluginsInCategory(category);
}

auto TimelineController::isAudioClip(int clipId) const -> bool {
    const auto *clip = m_timeline->findClipById(clipId);
    return (clip != nullptr) && clip->type == QLatin1String("audio");
}

auto TimelineController::getWaveformPeaks(int clipId, int pixelWidth, int displayDurationFrames) const -> QVariantList { // NOLINT(bugprone-easily-swappable-parameters)
    if (pixelWidth <= 0 || displayDurationFrames <= 0) {
        return {};
    }

    const auto *clip = m_timeline->findClipById(clipId);
    if ((clip == nullptr) || clip->type != "audio") {
        return {};
    }

    auto *decoder = qobject_cast<Rina::Core::AudioDecoder *>((m_mediaManager != nullptr) ? m_mediaManager->decoderForClip(clipId) : nullptr);
    if ((decoder == nullptr) || !decoder->isReady()) {
        return QVariantList(pixelWidth, 0.0);
    }

    int fps = static_cast<int>(m_project->fps());
    if (fps <= 0) {
        fps = 60;
    }
    // 渡された displayDurationFrames で秒数を計算 (ドラフト値が来たらそれを使う)
    double displaySec = static_cast<double>(displayDurationFrames) / fps;

    std::vector<float> rawPeaks = decoder->getPeaks(0.0, displaySec, pixelWidth);
    QVariantList result;
    result.reserve(static_cast<qsizetype>(rawPeaks.size()));
    for (float p : rawPeaks) {
        result.append(static_cast<double>(p));
    }

    return result;
}

auto TimelineController::getClipEffectStack(int clipId) const -> QVariantList {
    QVariantList list;
    if (clipId < 0) {
        return list;
    }

    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
    for (int i = 0; i < chain.count(); ++i) {
        auto *plugin = chain.get(i);
        if (plugin != nullptr) {
            QVariantMap effectInfo;
            effectInfo.insert(QStringLiteral("name"), plugin->name());
            effectInfo.insert(QStringLiteral("format"), plugin->format());
            list.append(effectInfo);
        }
    }
    return list;
}

auto TimelineController::getEffectParameters(int clipId, int effectIndex) const -> QVariantList {
    QVariantList list;
    if (clipId < 0) {
        return list;
    }
    // NOLINT(bugprone-easily-swappable-parameters)
    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
    auto *plugin = chain.get(effectIndex);
    if (plugin != nullptr) {
        for (int i = 0; i < plugin->paramCount(); ++i) {
            QVariantMap paramInfo;
            auto info = plugin->getParamInfo(i);

            paramInfo.insert(QStringLiteral("pIdx"), i);
            paramInfo.insert(QStringLiteral("name"), info.name);
            paramInfo.insert(QStringLiteral("current"), plugin->getParam(i));
            paramInfo.insert(QStringLiteral("min"), info.min);
            paramInfo.insert(QStringLiteral("max"), info.max);

            if (info.isToggle) {
                paramInfo.insert(QStringLiteral("type"), "bool");
            } else if (info.isInteger) {
                paramInfo.insert(QStringLiteral("type"), "int");
            } else {
                paramInfo.insert(QStringLiteral("type"), "slider");
            }

            list.append(paramInfo);
        }
    }
    return list;
}

void TimelineController::setEffectParameter(int clipId, int effectIndex, int paramIndex, float value) {
    if (clipId < 0) {
        return;
    }
    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
    auto *plugin = chain.get(effectIndex); // NOLINT(bugprone-easily-swappable-parameters)
    if (plugin != nullptr) {
        plugin->setParam(paramIndex, value);
    }
}

void TimelineController::setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) { m_timeline->setKeyframe(clipId, effectIndex, paramName, frame, value, options); }

void TimelineController::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) { m_timeline->removeKeyframe(clipId, effectIndex, paramName, frame); }

void TimelineController::deleteClip(int clipId) { requestDelete(clipId); }

void TimelineController::requestDelete(int targetClipId) {
    if ((m_timeline == nullptr) || (m_selection == nullptr)) {
        return;
    }

    QVariantList selected = m_selection->selectedClipIds();

    // 選択が1件以上ある場合
    if (!selected.isEmpty()) {
        // 全体削除（Delキーなど）または、選択対象を右クリックして削除する場合
        if (targetClipId < 0 || selected.contains(targetClipId)) {
            m_timeline->deleteClipsByIds(selected);
            return;
        }
    }

    // 選択されていない対象を直接削除しようとする場合
    if (targetClipId >= 0) {
        QVariantList ids{targetClipId};
        m_timeline->applySelectionIds(ids); // 内部的に選択状態を同期
        m_timeline->deleteClipsByIds(ids);
    }
}

void TimelineController::splitClip(int clipId, int frame) {
    if (m_timeline != nullptr) {
        m_timeline->splitClip(clipId, frame);
    }
}

void TimelineController::splitSelectedClips(int frame) {
    if (m_timeline != nullptr) {
        m_timeline->splitSelectedClips(frame);
    }
}

auto TimelineController::evaluateClipParams(int clipId, int relFrame) const -> QVariantMap {
    // relFrame は無視し、EngineSynchronizer が計算済みの最新キャッシュを返す。
    // これにより UI スレッドでの Lua 実行が完全に排除される。
    return m_engineSync->getCachedParams(clipId);
}
// NOLINT(bugprone-easily-swappable-parameters)
void TimelineController::copyClip(int clipId) { m_timeline->copyClip(clipId); }

void TimelineController::cutClip(int clipId) { m_timeline->cutClip(clipId); }

void TimelineController::pasteClip(int frame, int layer) { m_timeline->pasteClip(frame, layer); }

void TimelineController::copySelectedClips() {
    if (m_timeline != nullptr) {
        m_timeline->copySelectedClips();
    }
}

void TimelineController::cutSelectedClips() {
    if (m_timeline != nullptr) {
        m_timeline->cutSelectedClips();
    }
}

void TimelineController::deleteSelectedClips() { requestDelete(-1); }

auto TimelineController::scenes() const -> QVariantList { return m_timeline->scenes(); }

auto TimelineController::currentSceneId() const -> int { return m_timeline->currentSceneId(); }

void TimelineController::createScene(const QString &name) { m_timeline->createScene(name); }

void TimelineController::removeScene(int sceneId) { m_timeline->removeScene(sceneId); }

void TimelineController::switchScene(int sceneId) { m_timeline->switchScene(sceneId); }

void TimelineController::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision, bool enableSnap,
                                             int magneticSnapRange) {
    m_timeline->updateSceneSettings(sceneId, name, width, height, fps, totalFrames, gridMode, gridBpm, gridOffset, gridInterval, gridSubdivision, enableSnap, magneticSnapRange);
}

auto TimelineController::getSceneClips(int sceneId) const -> QVariantList {
    QVariantList list;
    const auto &clips = m_timeline->clips(sceneId);

    for (const auto &clip : clips) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), clip.id);
        map.insert(QStringLiteral("type"), clip.type);
        map.insert(QStringLiteral("startFrame"), clip.startFrame);
        map.insert(QStringLiteral("durationFrames"), clip.durationFrames);
        map.insert(QStringLiteral("layer"), clip.layer);

        // QMLソースの解決
        auto meta = Rina::Core::EffectRegistry::instance().getEffect(clip.type);
        if (!meta.qmlSource.isEmpty()) {
            map.insert(QStringLiteral("qmlSource"), meta.qmlSource);
        }

        // パラメータの収集 (エフェクトからフラット化)
        QVariantMap params;
        // 基本情報もparamsに入れておく
        params.insert(QStringLiteral("layer"), clip.layer);
        params.insert(QStringLiteral("startFrame"), clip.startFrame);
        params.insert(QStringLiteral("durationFrames"), clip.durationFrames);
        params.insert(QStringLiteral("id"), clip.id);

        for (auto *eff : clip.effects) {
            if (!eff->isEnabled()) {
                continue;
            }
            // キーフレーム評価を行わず生パラメータまたはデフォルト値を渡す
            // SceneObject内で時間に応じて評価されるため
            QVariantMap p = eff->params();
            for (auto it = p.begin(); it != p.end(); ++it) {
                params.insert(it.key(), it.value());
            }
        }
        map.insert(QStringLiteral("params"), params);
        list.append(map);
    }
    return list;
}

auto TimelineController::getSceneInfo(int sceneId) const -> QVariantMap {
    for (const auto &s : m_timeline->getAllScenes()) {
        if (s.id == sceneId) {
            return {{"id", s.id}, {"name", s.name}, {"width", s.width}, {"height", s.height}, {"fps", s.fps}, {"totalFrames", s.totalFrames}};
        }
    }
    return {};
}

auto TimelineController::getSceneDuration(int sceneId) const -> int {
    for (const auto &s : m_timeline->getAllScenes()) {
        if (s.id == sceneId) {
            return s.totalFrames;
        }
    }
    return 0;
}

void TimelineController::updateViewport(double x, double y) {
    // このメソッドは、QMLのレンダリングタイマーから呼び出され、現在の表示範囲をC++側に通知します。
    // 将来的に、描画範囲外のクリップのレンダリング計算をスキップする等の最適化に使用できます。
    Q_UNUSED(x)
    Q_UNUSED(y)
} // NOLINT(bugprone-easily-swappable-parameters)

auto TimelineController::resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame, const QVariantList &batchIds) -> QPoint { return m_timeline->resolveDragPosition(clipId, targetLayer, proposedStartFrame, batchIds); }

auto TimelineController::resolveDragDelta(int clipId, int deltaFrame, int deltaLayer, const QVariantList &batchIds, int minFrame, int minLayer, int maxLayer, int totalLayers) -> QPoint {
    const auto *clip = m_timeline->findClipById(clipId);
    if (clip == nullptr) {
        return {0, 0};
    }
    // NOLINT(bugprone-easily-swappable-parameters)
    // 1. 衝突判定を含めた座標解決
    QPoint resolved = m_timeline->resolveDragPosition(clipId, clip->layer + deltaLayer, clip->startFrame + deltaFrame, batchIds);

    int dF = resolved.x() - clip->startFrame;
    int dL = resolved.y() - clip->layer;

    // 2. タイムライン境界によるクランプ (QMLから移行)
    if (minFrame + dF < 0) {
        dF = -minFrame;
    }
    if (minLayer + dL < 0) {
        dL = -minLayer;
    }
    if (maxLayer + dL >= totalLayers) {
        dL = totalLayers - 1 - maxLayer;
    }

    return {dF, dL};
}

auto TimelineController::debugRunLua(const QString &script) -> QString {
    // テスト用に time=currentFrame/fps, index=0, value=0 で実行
    double time = (m_transport != nullptr) ? m_transport->currentFrame() / m_project->fps() : 0.0;
    double result = Rina::Scripting::LuaHost::instance().evaluate(script.toStdString(), time, 0, 0.0);
    return QString::number(result);
}

void TimelineController::requestVideoFrame(int clipId, int relFrame) {
    // MediaManagerは直接触れないので、TimelineService側にイベントを発火させる等するか、
    // MediaManagerに直接シグナルで飛ばす。
    // ここでは一番手っ取り早い「シグナル」を追加してMediaManagerに拾わせる。
    emit videoFrameRequested(clipId, relFrame);
}

void TimelineController::requestImageLoad(int clipId, const QString &path) { emit imageLoadRequested(clipId, path); }

} // namespace Rina::UI
