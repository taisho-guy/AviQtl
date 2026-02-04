#include "timeline_service.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include <QDebug>

namespace Rina::UI {

// === Commands Implementation ===
AddClipCommand::AddClipCommand(TimelineService *service, int clipId, const QString &type, int startFrame, int layer, const QString &clipName)
    : m_service(service), m_clipId(clipId), m_type(type), m_startFrame(startFrame), m_layer(layer), m_clipName(clipName) {
    setText(QString("クリップ追加: %1").arg(clipName));
}
void AddClipCommand::undo() { m_service->deleteClip(m_clipId); }
void AddClipCommand::redo() { m_service->createClipInternal(m_clipId, m_type, m_startFrame, m_layer); }

MoveClipCommand::MoveClipCommand(TimelineService *service, int clipId, int oldLayer, int oldStart, int oldDuration, int newLayer, int newStart, int newDuration, const QString &clipName)
    : m_service(service), m_clipId(clipId), m_oldLayer(oldLayer), m_oldStart(oldStart), m_oldDuration(oldDuration), m_newLayer(newLayer), m_newStart(newStart), m_newDuration(newDuration), m_clipName(clipName) {
    setText(QString("クリップ移動: %1").arg(clipName));
}
void MoveClipCommand::undo() { m_service->updateClipInternal(m_clipId, m_oldLayer, m_oldStart, m_oldDuration); }
void MoveClipCommand::redo() { m_service->updateClipInternal(m_clipId, m_newLayer, m_newStart, m_newDuration); }

UpdateEffectParamCommand::UpdateEffectParamCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, const QVariant &newValue, const QVariant &oldValue, const QString &effectName)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_paramName(paramName), m_newValue(newValue), m_oldValue(oldValue), m_effectName(effectName) {
    setText(QString("パラメータ変更: %1 - %2").arg(effectName).arg(paramName));
}
void UpdateEffectParamCommand::undo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_oldValue); }
void UpdateEffectParamCommand::redo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_newValue); }
int UpdateEffectParamCommand::id() const { return 1001; } // パラメータ変更コマンドのID
bool UpdateEffectParamCommand::mergeWith(const QUndoCommand *other) {
    if (other->id() != id())
        return false;
    const auto *cmd = static_cast<const UpdateEffectParamCommand *>(other);
    if (cmd->m_clipId != m_clipId || cmd->m_effectIndex != m_effectIndex || cmd->m_paramName != m_paramName)
        return false;
    m_newValue = cmd->m_newValue; // 連続する同じパラメータの変更はマージする
    return true;
}

AddEffectCommand::AddEffectCommand(TimelineService *service, int clipId, const QString &effectId, const QString &effectName) : m_service(service), m_clipId(clipId), m_effectId(effectId), m_effectName(effectName) {
    setText(QString("エフェクト追加: %1").arg(effectName));
}
void AddEffectCommand::undo() { m_service->removeEffectInternal(m_clipId, -1); }
void AddEffectCommand::redo() { m_service->addEffectInternal(m_clipId, m_effectId); }

RemoveEffectCommand::RemoveEffectCommand(TimelineService *service, int clipId, int effectIndex, const QString &effectName) : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_effectName(effectName) {
    setText(QString("エフェクト削除: %1").arg(effectName));
}
void RemoveEffectCommand::redo() { m_service->removeEffectInternal(m_clipId, m_effectIndex); }
void RemoveEffectCommand::undo() { m_service->restoreEffectInternal(m_clipId, m_removedEffectData); }

SplitClipCommand::SplitClipCommand(TimelineService *service, int clipId, int frame, const QString &clipName) : m_service(service), m_originalClipId(clipId), m_newClipId(-1), m_splitFrame(frame), m_originalDuration(0), m_clipName(clipName) {
    setText(QString("クリップ分割: %1").arg(clipName));
}

void SplitClipCommand::undo() {
    m_service->deleteClip(m_newClipId);
    // 元のクリップの長さを復元
    const auto &clips = m_service->clips();
    auto it = std::find_if(clips.begin(), clips.end(), [this](const ClipData &c) { return c.id == m_originalClipId; });
    if (it != clips.end()) {
        m_service->updateClipInternal(m_originalClipId, it->layer, it->startFrame, m_originalDuration);
    }
}

void SplitClipCommand::redo() {
    const auto &clips = m_service->clips();
    auto it = std::find_if(clips.begin(), clips.end(), [this](const ClipData &c) { return c.id == m_originalClipId; });
    if (it == clips.end())
        return;

    // 分割前の状態を保存・計算
    if (m_newClipId == -1) {
        m_newClipId = m_service->nextClipId();
        m_service->setNextClipId(m_newClipId + 1);
        m_originalDuration = it->durationFrames;
    }

    int firstHalfDuration = m_splitFrame - it->startFrame;
    int secondHalfDuration = m_originalDuration - firstHalfDuration;

    // 後半部分のクリップを作成（エフェクトなどをディープコピー）
    ClipData newClip = m_service->deepCopyClip(*it);
    newClip.id = m_newClipId;
    newClip.startFrame = m_splitFrame;
    newClip.durationFrames = secondHalfDuration;

    m_service->updateClipInternal(m_originalClipId, it->layer, it->startFrame, firstHalfDuration);
    m_service->addClipDirectInternal(newClip);
}

// === TimelineService Implementation ===

TimelineService::TimelineService(SelectionService *selection, QObject *parent) : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {
    // 初期シーンを作成
    SceneData rootScene;
    rootScene.id = 0;
    rootScene.name = "Root";
    rootScene.width = 1920; // プロジェクト設定と同期推奨
    rootScene.height = 1080;
    rootScene.fps = 60.0;
    rootScene.totalFrames = 3600;
    m_scenes.append(rootScene);
    m_currentSceneId = 0;
}

void TimelineService::undo() { m_undoStack->undo(); }
void TimelineService::redo() { m_undoStack->redo(); }

void TimelineService::createClip(const QString &type, int startFrame, int layer) {
    int id = m_nextClipId++;
    QString clipName = type;
    auto meta = Rina::Core::EffectRegistry::instance().getEffect(type);
    if (!meta.name.isEmpty()) {
        clipName = meta.name;
    }
    m_undoStack->push(new AddClipCommand(this, id, type, startFrame, layer, clipName));
}

void TimelineService::createClipInternal(int clipId, const QString &type, int startFrame, int layer) {
    if (startFrame < 0)
        startFrame = 0;
    if (layer < 0)
        layer = 0;

    const int defaultDuration = Rina::Core::SettingsManager::instance().settings().value("defaultClipDuration", 100).toInt();
    auto overlaps = [](int s1, int d1, int s2, int d2) { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();
    for (const auto &c : currentClips) {
        if (c.layer == layer && overlaps(startFrame, defaultDuration, c.startFrame, c.durationFrames)) {
            qWarning() << "クリップ作成を拒否: レイヤー" << layer << "の" << startFrame << "フレームで衝突が発生";
            return;
        }
    }

    ClipData newClip;
    newClip.id = clipId;
    newClip.type = type;
    newClip.startFrame = startFrame;
    newClip.durationFrames = defaultDuration;
    newClip.layer = layer;

    // Transformエフェクトは全クリップに必須
    auto *tm = new EffectModel("transform", "座標", {{"x", 0}, {"y", 0}, {"z", 0}, {"scale", 100.0}, {"aspect", 0.0}, {"rotationX", 0.0}, {"rotationY", 0.0}, {"rotationZ", 0.0}, {"opacity", 1.0}}, "", {}, this);
    connect(tm, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
    newClip.effects.append(tm);

    // オブジェクト固有のエフェクト (例: テキスト、図形)
    auto meta = Rina::Core::EffectRegistry::instance().getEffect(type);
    auto *cm = new EffectModel(meta.id.isEmpty() ? "unknown" : meta.id, meta.name.isEmpty() ? "Unknown" : meta.name, meta.defaultParams, meta.qmlSource, meta.uiDefinition, this);
    connect(cm, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
    newClip.effects.append(cm);

    currentClips.append(newClip);
    emit clipsChanged();
    emit clipCreated(newClip.id, newClip.layer, newClip.startFrame, newClip.durationFrames, newClip.type);
}

void TimelineService::updateClip(int id, int layer, int startFrame, int duration) {
    const auto *clip = findClipById(id);
    if (!clip)
        return;

    QString clipName = clip->type;
    if (!clip->effects.isEmpty()) {
        clipName = clip->effects.first()->name();
    }

    m_undoStack->push(new MoveClipCommand(this, id, clip->layer, clip->startFrame, clip->durationFrames, layer, startFrame, duration, clipName));
}

void TimelineService::updateClipInternal(int id, int layer, int startFrame, int duration) {
    if (startFrame < 0)
        startFrame = 0;
    if (duration < 1)
        duration = 1;
    if (layer < 0)
        layer = 0;

    auto overlaps = [](int s1, int d1, int s2, int d2) { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();
    for (const auto &c : currentClips) {
        if (c.id == id)
            continue;
        if (c.layer == layer && overlaps(startFrame, duration, c.startFrame, c.durationFrames)) {
            qWarning() << "クリップ更新を拒否: クリップID" << id << "で衝突が発生";
            return;
        }
    }

    for (auto &clip : currentClips) {
        if (clip.id == id) {
            if (clip.layer != layer || clip.startFrame != startFrame || clip.durationFrames != duration) {
                clip.layer = layer;
                clip.startFrame = startFrame;
                clip.durationFrames = duration;
                emit clipsChanged();
                // 選択中のクリップであれば、SelectionServiceのキャッシュも更新する
                if (m_selection->selectedClipId() == id) {
                    QVariantMap data = m_selection->selectedClipData();
                    data["layer"] = layer;
                    data["startFrame"] = startFrame;
                    data["durationFrames"] = duration;
                    m_selection->select(id, data);
                }
            }
            break;
        }
    }
}

void TimelineService::selectClip(int id) {
    if (m_selection->selectedClipId() == id)
        return;

    for (const auto &clip : clips()) {
        if (clip.id == id) {
            QVariantMap cache;
            for (auto *eff : clip.effects) {
                QVariantMap params = eff->params();
                for (auto it = params.begin(); it != params.end(); ++it)
                    cache.insert(it.key(), it.value());
            }

            cache["startFrame"] = clip.startFrame;
            cache["durationFrames"] = clip.durationFrames;
            cache["layer"] = clip.layer;
            cache["type"] = clip.type;

            m_selection->select(id, cache);
            return;
        }
    }
    m_selection->select(-1, {});
}

void TimelineService::deleteClip(int clipId) {
    auto &currentClips = clipsMutable();
    auto it = std::find_if(currentClips.begin(), currentClips.end(), [clipId](const ClipData &c) { return c.id == clipId; });
    if (it != currentClips.end()) {
        for (auto *eff : it->effects)
            eff->deleteLater();
        currentClips.erase(it);
        emit clipsChanged();
    }
}

void TimelineService::addEffect(int clipId, const QString &effectId) {
    auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
    if (meta.id.isEmpty())
        return;

    m_undoStack->push(new AddEffectCommand(this, clipId, effectId, meta.name));
}

void TimelineService::addEffectInternal(int clipId, const QString &effectId) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
            auto *model = new EffectModel(meta.id, meta.name, meta.defaultParams, meta.qmlSource, meta.uiDefinition, this);
            connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            clip.effects.append(model);
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::addClipDirectInternal(const ClipData &clip) {
    clipsMutable().append(clip);
    emit clipsChanged();
    emit clipCreated(clip.id, clip.layer, clip.startFrame, clip.durationFrames, clip.type);
}

void TimelineService::restoreEffectInternal(int clipId, const QVariantMap &data) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto *model = new EffectModel(data["id"].toString(), data["name"].toString(), data["params"].toMap(), data["qmlSource"].toString(), data["uiDefinition"].toMap(), this);
            model->setEnabled(data["enabled"].toBool());
            model->setKeyframeTracks(data["keyframes"].toMap());
            connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            clip.effects.append(model);
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::removeEffect(int clipId, int effectIndex) {
    QVariantMap removedData;
    const auto *clip = findClipById(clipId);
    if (!clip)
        return;

    int idx = (effectIndex == -1) ? clip->effects.size() - 1 : effectIndex;
    if (idx >= 0 && idx < clip->effects.size()) {
        auto *eff = clip->effects[idx];
        removedData["id"] = eff->id();
        removedData["name"] = eff->name();
        removedData["enabled"] = eff->isEnabled();
        removedData["params"] = eff->params();
        removedData["qmlSource"] = eff->qmlSource();
        removedData["uiDefinition"] = eff->uiDefinition();
        removedData["keyframes"] = eff->keyframeTracks();

        auto *cmd = new RemoveEffectCommand(this, clipId, effectIndex, eff->name());
        cmd->setRemovedEffect(removedData);
        m_undoStack->push(cmd);
    }
}

void TimelineService::removeEffectInternal(int clipId, int effectIndex) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex == -1)
                effectIndex = clip.effects.size() - 1;
            if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                // Transformエフェクトは削除不可
                if (effectIndex == 0 && clip.effects[0]->id() == "transform")
                    return;
                auto *eff = clip.effects.takeAt(effectIndex);
                eff->deleteLater();
                emit clipsChanged();
                emit clipEffectsChanged(clipId);
            }
            break;
        }
    }
}

void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    QVariant oldValue;
    const auto *clip = findClipById(clipId);
    if (!clip || effectIndex >= clip->effects.size())
        return;

    const auto *eff = clip->effects[effectIndex];
    oldValue = eff->params().value(paramName);

    m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue, eff->name()));
}

void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                clip.effects[effectIndex]->setParam(paramName, value);
                emit clipsChanged();
                if (m_selection->selectedClipId() == clipId) {
                    QVariantMap data = m_selection->selectedClipData();
                    data[paramName] = value;
                    m_selection->select(clipId, data);
                }
            }
            break;
        }
    }
}

ClipData *TimelineService::findClipById(int clipId) {
    auto &currentClips = clipsMutable();
    auto it = std::find_if(currentClips.begin(), currentClips.end(), [clipId](const ClipData &c) { return c.id == clipId; });
    return (it != currentClips.end()) ? &(*it) : nullptr;
}

const ClipData *TimelineService::findClipById(int clipId) const {
    const auto &currentClips = clips();
    auto it = std::find_if(currentClips.begin(), currentClips.end(), [clipId](const ClipData &c) { return c.id == clipId; });
    return (it != currentClips.end()) ? &(*it) : nullptr;
}

ClipData TimelineService::deepCopyClip(const ClipData &source) {
    ClipData newClip;
    newClip.id = -1;
    newClip.type = source.type;
    newClip.startFrame = source.startFrame;
    newClip.durationFrames = source.durationFrames;
    newClip.layer = source.layer;

    for (const auto *oldEffect : source.effects) {
        auto *newEffect = new EffectModel(oldEffect->id(), oldEffect->name(), oldEffect->params(), oldEffect->qmlSource(), oldEffect->uiDefinition(), this);
        newEffect->setEnabled(oldEffect->isEnabled());
        newEffect->setKeyframeTracks(oldEffect->keyframeTracks());
        connect(newEffect, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
        newClip.effects.append(newEffect);
    }
    return newClip;
}

void TimelineService::copyClip(int clipId) {
    const auto &currentClips = clips();
    auto it = std::find_if(currentClips.begin(), currentClips.end(), [clipId](const ClipData &c) { return c.id == clipId; });
    if (it != currentClips.end())
        m_clipboard = std::make_unique<ClipData>(deepCopyClip(*it));
}

void TimelineService::cutClip(int clipId) {
    copyClip(clipId);
    deleteClip(clipId);
}

void TimelineService::pasteClip(int frame, int layer) {
    if (!m_clipboard)
        return;

    if (frame < 0)
        frame = 0;
    if (layer < 0)
        layer = 0;

    auto overlaps = [](int s1, int d1, int s2, int d2) { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();
    for (const auto &c : currentClips) {
        if (c.layer == layer && overlaps(frame, m_clipboard->durationFrames, c.startFrame, c.durationFrames)) {
            qWarning() << "クリップ貼り付けを拒否: レイヤー" << layer << "の" << frame << "フレームで衝突が発生";
            return;
        }
    }

    ClipData newClip = deepCopyClip(*m_clipboard);
    newClip.id = m_nextClipId++;
    newClip.startFrame = frame;
    newClip.layer = layer;
    currentClips.append(newClip);
    emit clipsChanged();
    emit clipCreated(newClip.id, newClip.layer, newClip.startFrame, newClip.durationFrames, newClip.type);
}

void TimelineService::splitClip(int clipId, int frame) {
    const auto *clip = findClipById(clipId);
    if (!clip)
        return;

    if (frame > clip->startFrame && frame < clip->startFrame + clip->durationFrames) {
        QString clipName = clip->type;
        if (!clip->effects.isEmpty())
            clipName = clip->effects.first()->name();
        m_undoStack->push(new SplitClipCommand(this, clipId, frame, clipName));
    }
}

SceneData *TimelineService::currentScene() {
    for (auto &scene : m_scenes) {
        if (scene.id == m_currentSceneId)
            return &scene;
    }
    if (m_scenes.isEmpty()) {
        static SceneData dummy;
        return &dummy;
    }
    return &m_scenes[0];
}

const SceneData *TimelineService::currentScene() const {
    for (const auto &scene : m_scenes) {
        if (scene.id == m_currentSceneId)
            return &scene;
    }
    if (m_scenes.isEmpty()) {
        static SceneData dummy;
        return &dummy;
    }
    return &m_scenes[0];
}

const QList<ClipData> &TimelineService::clips() const { return currentScene()->clips; }

QList<ClipData> &TimelineService::clipsMutable() { return currentScene()->clips; }

QVariantList TimelineService::scenes() const {
    QVariantList list;
    for (const auto &scene : m_scenes) {
        QVariantMap map;
        map["id"] = scene.id;
        map["name"] = scene.name;
        list.append(map);
    }
    return list;
}

void TimelineService::createScene(const QString &name) {
    SceneData newScene;
    int maxId = 0;
    for (const auto &s : m_scenes)
        maxId = std::max(maxId, s.id);
    newScene.id = maxId + 1;
    newScene.name = name;

    // 現在のプロジェクト設定（あるいはRootシーン）の設定を継承
    // ※本来はSettingsManagerやProjectServiceから取得すべきだが、
    //   ここでは既存のシーン0の値をデフォルトとして採用する
    const SceneData &base = m_scenes.first();
    newScene.width = base.width;
    newScene.height = base.height;
    newScene.fps = base.fps;
    newScene.totalFrames = base.totalFrames;

    m_scenes.append(newScene);
    emit scenesChanged();
    switchScene(newScene.id);
}

void TimelineService::removeScene(int sceneId) {
    if (sceneId == 0)
        return;

    auto it = std::find_if(m_scenes.begin(), m_scenes.end(), [sceneId](const SceneData &s) { return s.id == sceneId; });
    if (it != m_scenes.end()) {
        for (auto &clip : it->clips) {
            qDeleteAll(clip.effects);
        }
        if (m_currentSceneId == sceneId) {
            switchScene(0);
        }
        m_scenes.erase(it);
        emit scenesChanged();
    }
}

void TimelineService::switchScene(int sceneId) {
    if (m_currentSceneId == sceneId)
        return;

    bool exists = false;
    for (const auto &s : m_scenes) {
        if (s.id == sceneId) {
            exists = true;
            break;
        }
    }
    if (!exists)
        return;

    m_currentSceneId = sceneId;
    emit currentSceneIdChanged();
    emit clipsChanged();

    if (m_selection)
        m_selection->select(-1, QVariantMap());
}

} // namespace Rina::UI