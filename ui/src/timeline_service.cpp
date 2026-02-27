#include "timeline_service.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include <QDebug>
#include <QPoint>
#include <algorithm>

namespace Rina::UI {

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

    // 後半部分のクリップを作成
    ClipData newClip = m_service->deepCopyClip(*it);
    newClip.id = m_newClipId;
    newClip.startFrame = m_splitFrame;
    newClip.durationFrames = secondHalfDuration;

    m_service->updateClipInternal(m_originalClipId, it->layer, it->startFrame, firstHalfDuration);
    m_service->addClipDirectInternal(newClip);
}

TimelineService::TimelineService(SelectionService *selection, QObject *parent) : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {
    // 初期シーンを作成
    SceneData rootScene;
    rootScene.id = 0;
    rootScene.name = "Root";
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    rootScene.width = settings.value("defaultProjectWidth", 1920).toInt();
    rootScene.height = settings.value("defaultProjectHeight", 1080).toInt();
    rootScene.fps = settings.value("defaultProjectFps", 60.0).toDouble();
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
    newClip.sceneId = m_currentSceneId;
    newClip.type = type;
    newClip.startFrame = startFrame;
    newClip.durationFrames = defaultDuration;
    newClip.layer = layer;

    currentClips.append(newClip);

    // デフォルトで transform エフェクトを追加
    addEffectInternal(clipId, "transform");
    addEffectInternal(clipId, type);

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

int TimelineService::computeMagneticSnapPosition(int clipId, int targetLayer, int proposedStartFrame) {
    // 0. 移動対象のクリップ情報を取得
    const auto *movingClip = findClipById(clipId);
    if (!movingClip) {
        return proposedStartFrame; // 対象クリップが見つからなければ何もしない
    }
    const int clipDuration = movingClip->durationFrames;

    // 1. 対象レイヤーのクリップを抽出 (現在のシーンのみ)
    QList<const ClipData *> layerClips;
    for (const auto &c : clips()) { // 修正: 全シーン走査を現在のシーンに限定
        if (c.layer == targetLayer && c.id != clipId) {
            layerClips.append(&c);
        }
    }

    int finalStart = std::max(0, proposedStartFrame);

    // 2. レイヤーに他のクリップがなければ、0未満にならないように補正してそのまま配置
    if (layerClips.isEmpty()) {
        updateClipInternal(clipId, targetLayer, finalStart, clipDuration);
        return finalStart;
    }

    // 3. startFrame昇順ソート
    std::sort(layerClips.begin(), layerClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    // 4. スナップ候補となる「空き領域の始点」を生成
    QList<int> snapPoints;

    // 4-1. 最初のクリップの前が空いていれば、タイムラインの先頭(0)を候補に
    if (layerClips.first()->startFrame >= clipDuration) {
        snapPoints.append(0);
    }

    // 4-2. クリップ間の隙間を候補に
    for (int i = 0; i < layerClips.size() - 1; ++i) {
        int gapStart = layerClips[i]->startFrame + layerClips[i]->durationFrames;
        int gapEnd = layerClips[i + 1]->startFrame;
        if (gapEnd - gapStart >= clipDuration) {
            snapPoints.append(gapStart);
        }
    }

    // 4-3. 最後のクリップの後を候補に
    snapPoints.append(layerClips.last()->startFrame + layerClips.last()->durationFrames);

    // 5. 提案位置に最も近いスナップ候補を見つける
    if (!snapPoints.isEmpty()) {
        int bestSnapPoint = snapPoints.first();
        int minDelta = std::abs(proposedStartFrame - bestSnapPoint);

        for (int i = 1; i < snapPoints.size(); ++i) {
            int delta = std::abs(proposedStartFrame - snapPoints[i]);
            if (delta < minDelta) {
                minDelta = delta;
                bestSnapPoint = snapPoints[i];
            }
        }
        finalStart = bestSnapPoint;
    } else {
        // どの隙間にも収まらない場合 (クリップが密集している場合など) は、既存の findVacantFrame で安全な位置を探す
        finalStart = findVacantFrame(targetLayer, proposedStartFrame, clipDuration, clipId);
    }

    // 6. 即時適用 (Undoスタックを汚さないように internal を使用)
    // 注意: ドラッグ完了時に QUndoCommand を発行するアーキテクチャが望ましいです。
    // 修正: この関数は位置を計算して返すだけに責務を限定します。
    // 実際の更新は呼び出し元（QMLのドラッグハンドラなど）が担当べきです。
    // updateClipInternal(clipId, targetLayer, finalStart, clipDuration);
    return finalStart;
}

QPoint TimelineService::resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame) {
    const auto *movingClip = findClipById(clipId);
    if (!movingClip) {
        return QPoint(proposedStartFrame, targetLayer);
    }

    const int duration = movingClip->durationFrames;
    int candidateFrame = std::max(0, proposedStartFrame);

    // 1. 対象レイヤーの他のクリップをすべて取得
    QList<const ClipData *> otherClips;
    for (const auto &c : clips()) {
        if (c.id != clipId && c.layer == targetLayer) {
            otherClips.append(&c);
        }
    }

    // クリップがなければ衝突はありえない
    if (otherClips.isEmpty()) {
        return QPoint(candidateFrame, targetLayer);
    }

    // 2. 衝突解決ロジックのために、クリップを開始フレームでソートする
    // これが欠けていたことが、クリップが重なってしまうバグの根本原因でした。
    std::sort(otherClips.begin(), otherClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    // 3. 衝突解決 (Push-Right)
    // QML側でグリッドスナップされた位置(`proposedStartFrame`)を尊重し、
    // 衝突する場合のみ、重ならなくなるまで右に押し出す。
    // このループは、押し出した結果、さらに別のクリップと衝突するケースを解決するために必要。
    bool collisionFound;
    do {
        collisionFound = false;
        for (const auto *c : otherClips) {
            // Overlap check: (s1 < e2) && (s2 < e1)
            if (candidateFrame < (c->startFrame + c->durationFrames) && c->startFrame < (candidateFrame + duration)) {
                // 衝突した場合、そのクリップの右端に候補位置を移動
                candidateFrame = c->startFrame + c->durationFrames;
                // 位置を動かしたため、再度全クリップとの衝突を確認する必要がある
                collisionFound = true;
                break;
            }
        }
    } while (collisionFound);

    return QPoint(candidateFrame, targetLayer);
}

void TimelineService::updateClipInternal(int id, int layer, int startFrame, int duration) {
    if (startFrame < 0)
        startFrame = 0;
    if (duration < 1)
        duration = 1;
    if (layer < 0)
        layer = 0;

    // [FINAL LOGIC] The ultimate gatekeeper for collision.
    // All position updates, whether from drag, undo, or other operations, must pass this check.
    int safeStartFrame = findVacantFrame(layer, startFrame, duration, id);
    if (safeStartFrame != startFrame) {
        qWarning() << "updateClipInternal: Collision detected. Position adjusted from" << startFrame << "to" << safeStartFrame;
        startFrame = safeStartFrame;
    }

    for (auto &clip : clipsMutable()) {
        if (clip.id == id) {
            if (clip.layer != layer || clip.startFrame != startFrame || clip.durationFrames != duration) {
                clip.layer = layer;
                clip.startFrame = startFrame;
                clip.durationFrames = duration;
                emit clipsChanged();
                // 選択中のクリップであればSelectionServiceのキャッシュも更新する
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
            auto *model = new EffectModel(meta.id, meta.name, meta.category, meta.defaultParams, meta.qmlSource, meta.uiDefinition, this);
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
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(data["id"].toString());
            auto *model = new EffectModel(data["id"].toString(), data["name"].toString(), meta.category, data["params"].toMap(), data["qmlSource"].toString(), data["uiDefinition"].toMap(), this);
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
        auto *newEffect = new EffectModel(oldEffect->id(), oldEffect->name(), oldEffect->category(), oldEffect->params(), oldEffect->qmlSource(), oldEffect->uiDefinition(), this);
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
    newClip.sceneId = m_currentSceneId;
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

const QList<ClipData> &TimelineService::clips(int sceneId) const {
    for (const auto &scene : m_scenes) {
        if (scene.id == sceneId)
            return scene.clips;
    }
    static QList<ClipData> empty;
    return empty;
}

QVariantList TimelineService::scenes() const {
    QVariantList list;
    for (const auto &scene : m_scenes) {
        QVariantMap map;
        map["id"] = scene.id;
        map["name"] = scene.name;
        map["width"] = scene.width;
        map["height"] = scene.height;
        map["fps"] = scene.fps;
        map["totalFrames"] = scene.totalFrames;
        list.append(map);
    }
    return list;
}

void TimelineService::setScenes(const QList<SceneData> &scenes) {
    m_scenes = scenes;
    if (m_scenes.isEmpty()) {
        createScene("Root");
    }
    // 現在のシーンIDが有効か確認
    bool found = false;
    for (const auto &s : m_scenes) {
        if (s.id == m_currentSceneId) {
            found = true;
            break;
        }
    }
    if (!found && !m_scenes.isEmpty())
        m_currentSceneId = m_scenes.first().id;
    emit scenesChanged();
    emit currentSceneIdChanged();
    emit clipsChanged();
}

void TimelineService::createScene(const QString &name) {
    SceneData newScene;
    newScene.id = m_nextSceneId++;
    newScene.name = name;

    // プロジェクト設定と同期
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    newScene.width = settings.value("defaultProjectWidth", 1920).toInt();
    newScene.height = settings.value("defaultProjectHeight", 1080).toInt();
    newScene.fps = settings.value("defaultProjectFps", 60.0).toDouble();

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

void TimelineService::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames) {
    for (auto &scene : m_scenes) {
        if (scene.id == sceneId) {
            scene.name = name;
            scene.width = width;
            scene.height = height;
            scene.fps = fps;
            scene.totalFrames = totalFrames;
            emit scenesChanged();
            return;
        }
    }
}

QList<ClipData *> TimelineService::resolvedActiveClipsAt(int frame) const {
    QList<ClipData *> result;

    // 1. 現在シーンを取得
    const SceneData *currentScenePtr = nullptr;
    for (const auto &s : m_scenes) {
        if (s.id == m_currentSceneId) {
            currentScenePtr = &s;
            break;
        }
    }
    if (!currentScenePtr)
        return result;

    // 現在シーン内のクリップを走査
    for (auto &clip : currentScenePtr->clips) {
        // 通常クリップ
        if (clip.type != "scene") {
            if (frame >= clip.startFrame && frame < clip.startFrame + clip.durationFrames) {
                result.append(const_cast<ClipData *>(&clip));
            }
            continue;
        }

        // シーンオブジェクトの場合
        int parentLocal = frame - clip.startFrame;
        if (parentLocal < 0 || parentLocal >= clip.durationFrames)
            continue;

        // パラメータから sceneId / speed / offset を取得
        int targetSceneId = 0;
        double speed = 1.0;
        int offset = 0;
        if (!clip.effects.isEmpty()) {
            // 先頭エフェクト(Transform)の次、あるいはIDで検索すべきだけど
            // 確実にエフェクトリストからパラメータを取得
            for (auto *eff : clip.effects) {
                if (eff->id() == "scene") {
                    QVariantMap p = eff->params();
                    targetSceneId = p.value("targetSceneId", 0).toInt();
                    speed = p.value("speed", 1.0).toDouble();
                    offset = p.value("offset", 0).toInt();
                    break;
                }
            }
        }
        if (targetSceneId < 0)
            continue;

        int childLocal = static_cast<int>(parentLocal * speed) + offset;

        // 対象シーンを探す
        const SceneData *targetScene = nullptr;
        for (const auto &s : m_scenes) {
            if (s.id == targetSceneId) {
                targetScene = &s;
                break;
            }
        }
        if (!targetScene)
            continue;

        // 対象シーン内のクリップを、親シーンの座標系に射影して追加
        for (auto &child : targetScene->clips) {
            if (childLocal >= child.startFrame && childLocal < child.startFrame + child.durationFrames) {
                // グローバル時間で見た start は「親のstart + 子のstart」
                // （ここではClipData自体は書き換えず、判定だけに使う）
                result.append(const_cast<ClipData *>(&child));
            }
        }
    }

    return result;
}

int TimelineService::findVacantFrame(int layer, int startFrame, int duration, int excludeClipId) const {
    QList<const ClipData *> layerClips;
    for (const auto &clip : clips()) {
        if (clip.id != excludeClipId && clip.layer == layer) {
            layerClips.append(&clip);
        }
    }

    std::sort(layerClips.begin(), layerClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    int candidateStart = std::max(0, startFrame);
    for (const auto &clip : layerClips) {
        int clipEnd = clip->startFrame + clip->durationFrames;
        int candidateEnd = candidateStart + duration;
        if (candidateStart < clipEnd && candidateEnd > clip->startFrame) {
            candidateStart = clipEnd;
        }
    }
    return candidateStart;
}

} // namespace Rina::UI