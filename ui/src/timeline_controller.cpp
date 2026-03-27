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
        [this]() {
            m_engineSync->rebuildClipIndex();
            emit clipsChanged();
            m_mediaManager->updateMediaDecoders();
            updateActiveClipsList();
            m_transport->setTotalFrames(timelineDuration());
        },
        Qt::QueuedConnection);

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
    connect(m_timeline, &TimelineService::effectParamChanged, this, [this]() { updateActiveClipsList(); });

    connect(m_exportManager, &TimelineExportManager::exportStarted, this, &TimelineController::exportStarted);
    connect(m_exportManager, &TimelineExportManager::exportProgressChanged, this, &TimelineController::exportProgressChanged);
    connect(m_exportManager, &TimelineExportManager::exportFinished, this, &TimelineController::exportFinished);

    // FPSが変更されたら再生タイマーの間隔を更新
    connect(m_project, &ProjectService::fpsChanged, [this]() { m_transport->updateTimerInterval(m_project->fps()); });
    m_transport->updateTimerInterval(m_project->fps());

    // 再生状態の変化をデコーダーに伝播
    connect(m_transport, &TransportService::isPlayingChanged, this, &TimelineController::onPlayingChanged);

    // 再生位置が変わったらプレビュー更新
    connect(m_transport, &TransportService::currentFrameChanged, this, &TimelineController::onCurrentFrameChanged);

    // QML(VideoObject)からのフレーム要求をMediaManagerへ中継
    connect(this, &TimelineController::videoFrameRequested, m_mediaManager, &TimelineMediaManager::requestVideoFrame);
}

void TimelineController::onPlayingChanged() { m_mediaManager->onPlayingChanged(); }

void TimelineController::onCurrentFrameChanged() {
    [[maybe_unused]] int nextFrame = m_transport->currentFrame();
    m_mediaManager->onCurrentFrameChanged();
    updateActiveClipsList();
}

void TimelineController::setVideoFrameStore(Rina::Core::VideoFrameStore *store) {
    qDebug() << "TimelineController: VideoFrameStore set. Updating decoders...";
    m_mediaManager->setVideoFrameStore(store);
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

    const ClipData *clip = m_timeline->findClipById(id);
    if (!clip)
        return;

    // 暫定対応：プロパティ名に応じて適切なエフェクトのパラメータを更新する
    int targetEffectIndex = -1;

    for (int i = 0; i < clip->effects.size(); ++i) {
        if (clip->effects[i]->params().contains(name)) {
            targetEffectIndex = i;
            break;
        }
    }

    if (targetEffectIndex == -1 && !clip->effects.isEmpty()) {
        targetEffectIndex = 0;
        // transform に属するキーかどうかで対象エフェクトを振り分け
        static const QStringList transformKeys = {"x", "y", "z", "scale", "aspect", "rotationX", "rotationY", "rotationZ", "opacity"};
        if (!transformKeys.contains(name) && clip->effects.size() > 1) {
            targetEffectIndex = 1;
        }
    }

    if (targetEffectIndex != -1 && targetEffectIndex < clip->effects.size()) {
        updateClipEffectParam(id, targetEffectIndex, name, value);
        updateActiveClipsList();
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
        map["name"] = !meta.name.isEmpty() ? meta.name : clip.type;
        if (!meta.qmlSource.isEmpty()) {
            map["qmlSource"] = meta.qmlSource;
        }

        // params を構築して追加
        QVariantMap params;
        // 基本情報もparamsに入れておく（QML側での利便性とBaseObjectでの参照用）
        params["layer"] = clip.layer;
        params["startFrame"] = clip.startFrame;
        params["durationFrames"] = clip.durationFrames;
        params["id"] = clip.id;

        for (auto *eff : clip.effects) {
            QVariantMap p = eff->params();
            for (auto it = p.begin(); it != p.end(); ++it) {
                params.insert(it.key(), it.value());
            }
        }
        map["params"] = params;

        list.append(map);
    }
    return list;
}

void TimelineController::updateActiveClipsList() { m_engineSync->updateActiveClipsList(); }

void TimelineController::log(const QString &msg) { qDebug() << "[TimelineBridge] " << msg; }

void TimelineController::moveSelectedClips(int deltaLayer, int deltaFrame) {
    if (m_timeline)
        m_timeline->moveSelectedClips(deltaLayer, deltaFrame);
}

void TimelineController::applyClipBatchMove(const QVariantList &moves) {
    if (m_timeline)
        m_timeline->applyClipBatchMove(moves);
}

void TimelineController::resizeSelectedClips(int deltaStartFrame, int deltaDuration) {
    if (m_timeline)
        m_timeline->resizeSelectedClips(deltaStartFrame, deltaDuration);
}

void TimelineController::updateClip(int id, int layer, int startFrame, int duration) {
    const auto *clip = m_timeline->findClipById(id);

    if (clip) {
        const int projectFps = project()->fps();

        if (clip->type == "video") {
            auto *vid = qobject_cast<Rina::Core::VideoDecoder *>(m_mediaManager->decoderForClip(id));
            if (vid && vid->isReady()) {
                int startVideoFrame = 0;
                double speed = 100.0;
                bool isDirectMode = false;

                for (const auto *eff : clip->effects) {
                    if (eff->id() != "video")
                        continue;
                    const QString playMode = eff->params().value("playMode", "開始フレーム＋再生速度").toString();
                    if (playMode == "フレーム直接指定") {
                        isDirectMode = true;
                        break;
                    }
                    startVideoFrame = eff->params().value("startFrame", 0).toInt();
                    speed = eff->params().value("speed", 100.0).toDouble();
                    break;
                }

                double srcFps = vid->sourceFps();
                if (srcFps <= 0.0)
                    srcFps = projectFps;
                int maxDuration = duration;

                if (isDirectMode) {
                    // フレーム直接指定: 単純に「素材の総フレーム数 / srcFps * projectFps」を限界とする
                    const double totalSec = static_cast<double>(vid->totalFrameCount()) / srcFps;
                    maxDuration = static_cast<int>(totalSec * projectFps);
                } else if (speed > 0.0) {
                    const double startSec = static_cast<double>(startVideoFrame) / srcFps;
                    const double remainingSec = static_cast<double>(vid->totalFrameCount()) / srcFps - startSec;
                    if (remainingSec > 0.0) {
                        maxDuration = static_cast<int>(remainingSec / (speed / 100.0) * projectFps);
                    }
                }

                if (maxDuration > 0 && duration > maxDuration) {
                    duration = maxDuration;
                }
            }
        } else if (clip->type == "audio") {
            auto *aud = qobject_cast<Rina::Core::AudioDecoder *>(m_mediaManager->decoderForClip(id));
            if (aud && aud->isReady()) {
                double startTime = 0.0;
                double speed = 100.0;
                bool isDirectMode = false;

                for (const auto *eff : clip->effects) {
                    if (eff->id() != "audio")
                        continue;
                    const QString playMode = eff->params().value("playMode", "開始時間＋再生速度").toString();
                    if (playMode == "時間直接指定") {
                        isDirectMode = true;
                        break;
                    }
                    startTime = eff->params().value("startTime", 0.0).toDouble();
                    speed = eff->params().value("speed", 100.0).toDouble();
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
        } else if (clip->type == "scene") {
            int targetSceneId = 0;
            double speed = 1.0;
            int offset = 0;
            [[maybe_unused]] bool isDirectMode = false;

            for (const auto *eff : clip->effects) {
                if (eff->id() != "scene")
                    continue;
                // ※ 現在Sceneには playMode パラメータは無いが、将来のために概念として考慮。
                // 現状の仕様では「speed と offset」による計算。
                targetSceneId = eff->params().value("targetSceneId", 0).toInt();
                speed = eff->params().value("speed", 1.0).toDouble();
                offset = eff->params().value("offset", 0).toInt();
                break;
            }

            const int sceneDur = getSceneDuration(targetSceneId);
            if (sceneDur > 0 && speed > 0.0) {
                const double rhs = (static_cast<double>(sceneDur - 1 - offset)) / speed;
                int maxDuration = static_cast<int>(rhs) + 1;
                if (maxDuration < 1)
                    maxDuration = 1;
                if (duration > maxDuration) {
                    duration = maxDuration;
                }
            }
        }
    }

    m_timeline->updateClip(id, layer, startFrame, duration);
}

void TimelineController::moveClipWithCollisionCheck(int clipId, int layer, int startFrame) {
    const ClipData *clip = m_timeline->findClipById(clipId);
    if (!clip)
        return;

    int duration = clip->durationFrames;
    int fixedStart = m_timeline->findVacantFrame(layer, startFrame, duration, clipId);
    updateClip(clipId, layer, fixedStart, duration);
}

bool TimelineController::saveProject(const QString &fileUrl) {
    // 渡されたパスが空の場合は内部で保持しているパスを割り当てる
    QString targetUrl = fileUrl.isEmpty() ? m_currentProjectUrl : fileUrl;

    // パスが空の場合は新規作成直後なのでエラーで返す
    if (targetUrl.isEmpty()) {
        emit errorOccurred("保存先のファイルパスが不明です");
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

bool TimelineController::loadProject(const QString &fileUrl) {
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

bool TimelineController::exportMedia(const QString &fileUrl, const QString &format, int quality) { return m_exportManager->exportMedia(fileUrl, format, quality); }

void TimelineController::exportVideoAsync(const QVariantMap &cfg) {
    Rina::Core::VideoEncoder::Config c;
    c.width = cfg.value("width", 1920).toInt();
    c.height = cfg.value("height", 1080).toInt();
    c.fps_num = cfg.value("fps_num", 60000).toInt();
    c.fps_den = cfg.value("fps_den", 1000).toInt();
    c.bitrate = cfg.value("bitrate", 15'000'000).toLongLong();
    c.crf = cfg.value("crf", -1).toInt();
    c.codecName = cfg.value("codecName", "h264_vaapi").toString();
    c.audioCodecName = cfg.value("audioCodecName", "aac").toString();
    c.audioBitrate = cfg.value("audioBitrate", 192'000).toLongLong();
    c.outputUrl = cfg.value("outputUrl").toString();
    c.startFrame = cfg.value("startFrame", 0).toInt();
    c.endFrame = cfg.value("endFrame", -1).toInt();
    m_exportManager->exportVideoAsync(c);
}

void TimelineController::cancelExport() { m_exportManager->cancelExport(); }
bool TimelineController::isExporting() const { return m_exportManager->isExporting(); }

void TimelineController::selectClip(int id) {
    if (m_timeline)
        m_timeline->applySelectionIds(QVariantList{id});
}

void TimelineController::selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive) {
    if (m_timeline)
        m_timeline->selectClipsInRange(frameA, frameB, layerA, layerB, additive);
}

void TimelineController::applySelectionIds(const QVariantList &ids) {
    if (m_timeline)
        m_timeline->applySelectionIds(ids);
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

QString TimelineController::getClipTypeColor(const QString &type) const { return Rina::Core::EffectRegistry::instance().getEffect(type).color; }

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

QVariantList TimelineController::getWaveformPeaks(int clipId, int pixelWidth, int displayDurationFrames) const {
    if (pixelWidth <= 0 || displayDurationFrames <= 0)
        return {};

    const auto *clip = m_timeline->findClipById(clipId);
    if (!clip || clip->type != "audio")
        return {};

    auto *decoder = qobject_cast<Rina::Core::AudioDecoder *>(m_mediaManager ? m_mediaManager->decoderForClip(clipId) : nullptr);
    if (!decoder || !decoder->isReady())
        return QVariantList(pixelWidth, 0.0);

    int fps = m_project->fps();
    if (fps <= 0)
        fps = 60;
    int sampleRate = m_project->sampleRate();
    if (sampleRate <= 0)
        sampleRate = 48000;

    // 渡された displayDurationFrames で秒数を計算 (ドラフト値が来たらそれを使う)
    double displaySec = static_cast<double>(displayDurationFrames) / fps;
    int totalSamples = static_cast<int>(displaySec * sampleRate);
    int samplesPerPixel = std::max(1, totalSamples / pixelWidth);

    QVariantList peaks;
    peaks.reserve(pixelWidth);

    for (int px = 0; px < pixelWidth; px++) {
        double startSec = static_cast<double>(px * samplesPerPixel) / sampleRate;
        auto samples = decoder->getSamples(startSec, samplesPerPixel * 2);

        float peak = 0.0f;
        for (float s : samples)
            peak = std::max(peak, std::abs(s));

        peaks.append(static_cast<double>(std::min(peak, 1.0f)));
    }

    return peaks;
}

QVariantList TimelineController::getClipEffectStack(int clipId) const {
    QVariantList list;
    if (clipId < 0)
        return list;

    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
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

    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
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
    auto &chain = m_mediaManager->audioMixer()->getChain(clipId);
    auto *plugin = chain.get(effectIndex);
    if (plugin) {
        plugin->setParam(paramIndex, value);
    }
}

void TimelineController::setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) { m_timeline->setKeyframe(clipId, effectIndex, paramName, frame, value, options); }

void TimelineController::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) { m_timeline->removeKeyframe(clipId, effectIndex, paramName, frame); }

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

void TimelineController::copySelectedClips() {
    if (m_timeline)
        m_timeline->copySelectedClips();
}

void TimelineController::cutSelectedClips() {
    if (m_timeline)
        m_timeline->cutSelectedClips();
}

void TimelineController::deleteSelectedClips() {
    if (m_timeline)
        m_timeline->deleteSelectedClips();
}

QVariantList TimelineController::scenes() const { return m_timeline->scenes(); }

int TimelineController::currentSceneId() const { return m_timeline->currentSceneId(); }

void TimelineController::createScene(const QString &name) { m_timeline->createScene(name); }

void TimelineController::removeScene(int sceneId) { m_timeline->removeScene(sceneId); }

void TimelineController::switchScene(int sceneId) { m_timeline->switchScene(sceneId); }

void TimelineController::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision, bool enableSnap,
                                             int magneticSnapRange) {
    m_timeline->updateSceneSettings(sceneId, name, width, height, fps, totalFrames, gridMode, gridBpm, gridOffset, gridInterval, gridSubdivision, enableSnap, magneticSnapRange);
}

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
        }

        // パラメータの収集 (エフェクトからフラット化)
        QVariantMap params;
        // 基本情報もparamsに入れておく
        params["layer"] = clip.layer;
        params["startFrame"] = clip.startFrame;
        params["durationFrames"] = clip.durationFrames;
        params["id"] = clip.id;

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

QVariantMap TimelineController::getSceneInfo(int sceneId) const {
    for (const auto &s : m_timeline->getAllScenes()) {
        if (s.id == sceneId)
            return {{"id", s.id}, {"name", s.name}, {"width", s.width}, {"height", s.height}, {"fps", s.fps}, {"totalFrames", s.totalFrames}};
    }
    return {};
}

int TimelineController::getSceneDuration(int sceneId) const {
    for (const auto &s : m_timeline->getAllScenes())
        if (s.id == sceneId)
            return s.totalFrames;
    return 0;
}

void TimelineController::updateViewport(double x, double y) {
    // このメソッドは、QMLのレンダリングタイマーから呼び出され、現在の表示範囲をC++側に通知します。
    // 将来的に、描画範囲外のクリップのレンダリング計算をスキップする等の最適化に使用できます。
    Q_UNUSED(x)
    Q_UNUSED(y)
}

QPoint TimelineController::resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame, const QVariantList &batchIds) { return m_timeline->resolveDragPosition(clipId, targetLayer, proposedStartFrame, batchIds); }

QString TimelineController::debugRunLua(const QString &script) {
    // テスト用に time=currentFrame/fps, index=0, value=0 で実行
    double time = m_transport ? m_transport->currentFrame() / m_project->fps() : 0.0;
    double result = Rina::Scripting::LuaHost::instance().evaluate(script.toStdString(), time, 0, 0.0);
    return QString::number(result);
}

void TimelineController::requestVideoFrame(int clipId, int relFrame) {
    // MediaManagerは直接触れないので、TimelineService側にイベントを発火させる等するか、
    // MediaManagerに直接シグナルで飛ばす。
    // ここでは一番手っ取り早い「シグナル」を追加してMediaManagerに拾わせる。
    emit videoFrameRequested(clipId, relFrame);
}
} // namespace Rina::UI
