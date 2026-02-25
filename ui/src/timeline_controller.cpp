#include "timeline_controller.hpp"
#include "../../core/include/project_serializer.hpp"
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

    // FPSが変更されたら再生タイマーの間隔を更新
    connect(m_project, &ProjectService::fpsChanged, [this]() { m_transport->updateTimerInterval(m_project->fps()); });
    m_transport->updateTimerInterval(m_project->fps());

    // 再生状態の変化をデコーダーに伝播
    connect(m_transport, &TransportService::isPlayingChanged, this, &TimelineController::onPlayingChanged);

    // 再生位置が変わったらプレビュー更新
    connect(m_transport, &TransportService::currentFrameChanged, this, &TimelineController::onCurrentFrameChanged);
}

void TimelineController::onPlayingChanged() { m_mediaManager->onPlayingChanged(); }

void TimelineController::onCurrentFrameChanged() {
    int nextFrame = m_transport->currentFrame();
    // ループ再生ロジック
    if (nextFrame > timelineDuration() && timelineDuration() > 0) {
        m_transport->setCurrentFrame(0);
        return; // ループ時はここで処理を切り上げ、0フレーム目に任せる
    }
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

void TimelineController::updateClip(int id, int layer, int startFrame, int duration) { m_timeline->updateClip(id, layer, startFrame, duration); }

void TimelineController::moveClipWithCollisionCheck(int clipId, int layer, int startFrame) {
    const ClipData *clip = m_timeline->findClipById(clipId);
    if (!clip)
        return;

    int duration = clip->durationFrames;
    int fixedStart = m_timeline->findVacantFrame(layer, startFrame, duration, clipId);
    updateClip(clipId, layer, fixedStart, duration);
}

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

bool TimelineController::exportMedia(const QString &fileUrl, const QString &format, int quality) { return m_exportManager->exportMedia(fileUrl, format, quality); }

void TimelineController::exportVideoHW(Rina::Core::VideoEncoder *encoder) { m_exportManager->exportVideoHW(encoder); }

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

QString TimelineController::debugRunLua(const QString &script) {
    // テスト用に time=currentFrame/fps, index=0, value=0 で実行
    double time = m_transport ? m_transport->currentFrame() / m_project->fps() : 0.0;
    double result = Rina::Scripting::LuaHost::instance().evaluate(script.toStdString(), time, 0, 0.0);
    return QString::number(result);
}

} // namespace Rina::UI