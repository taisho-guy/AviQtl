#include "timeline_service.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include <QDebug>
#include <QPoint>
#include <algorithm>
#include <utility>

namespace Rina::UI {

AddClipCommand::AddClipCommand(TimelineService *service, int clipId, QString type, int startFrame, int layer, const QString &clipName) // NOLINT(bugprone-easily-swappable-parameters)
    : m_service(service), m_clipId(clipId), m_type(std::move(type)), m_startFrame(startFrame), m_layer(layer), m_clipName(clipName) {
    setText(QObject::tr("クリップ追加: %1").arg(clipName));
}
void AddClipCommand::undo() { m_service->deleteClipInternal(m_clipId); }
void AddClipCommand::redo() { m_service->createClipInternal(m_clipId, m_type, m_startFrame, m_layer); }

MoveClipCommand::MoveClipCommand(TimelineService *service, int clipId, int oldLayer, int oldStart, int oldDuration, int newLayer, int newStart, int newDuration, const QString &clipName) // NOLINT(bugprone-easily-swappable-parameters)
    : m_service(service), m_clipId(clipId), m_oldLayer(oldLayer), m_oldStart(oldStart), m_oldDuration(oldDuration), m_newLayer(newLayer), m_newStart(newStart), m_newDuration(newDuration), m_clipName(clipName) {
    setText(QObject::tr("クリップ移動: %1").arg(clipName));
}
void MoveClipCommand::undo() { m_service->updateClipInternal(m_clipId, m_oldLayer, m_oldStart, m_oldDuration); }
void MoveClipCommand::redo() { m_service->updateClipInternal(m_clipId, m_newLayer, m_newStart, m_newDuration); }

UpdateEffectParamCommand::UpdateEffectParamCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, QVariant newValue, QVariant oldValue, const QString &effectName) // NOLINT(bugprone-easily-swappable-parameters)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_paramName(paramName), m_newValue(std::move(newValue)), m_oldValue(std::move(oldValue)), m_effectName(effectName) {
    setText(QObject::tr("パラメータ変更: %1 - %2").arg(effectName).arg(paramName));
}
void UpdateEffectParamCommand::undo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_oldValue); }
void UpdateEffectParamCommand::redo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_newValue); }
auto UpdateEffectParamCommand::id() const -> int { return 1001; } // パラメータ変更コマンドのID
auto UpdateEffectParamCommand::mergeWith(const QUndoCommand *other) -> bool {
    if (other->id() != id()) {
        return false;
    }
    const auto *cmd = dynamic_cast<const UpdateEffectParamCommand *>(other);
    if (cmd->m_clipId != m_clipId || cmd->m_effectIndex != m_effectIndex || cmd->m_paramName != m_paramName) {
        return false;
    }
    m_newValue = cmd->m_newValue; // 連続する同じパラメータの変更はマージする
    return true;
}

AddEffectCommand::AddEffectCommand(TimelineService *service, int clipId, QString effectId, const QString &effectName)
    : m_service(service), m_clipId(clipId), m_effectId(std::move(effectId)), m_effectName(effectName) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("エフェクト追加: %1").arg(effectName));
}
void AddEffectCommand::undo() { m_service->removeEffectInternal(m_clipId, -1); }
void AddEffectCommand::redo() { m_service->addEffectInternal(m_clipId, m_effectId); }

RemoveEffectCommand::RemoveEffectCommand(TimelineService *service, int clipId, int effectIndex, const QString &effectName)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_effectName(effectName) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("エフェクト削除: %1").arg(effectName));
}
void RemoveEffectCommand::redo() { m_service->removeEffectInternal(m_clipId, m_effectIndex); }
void RemoveEffectCommand::undo() { m_service->restoreEffectInternal(m_clipId, m_removedEffectData); }

ReorderEffectCommand::ReorderEffectCommand(TimelineService *service, int clipId, int oldIndex, int newIndex) : m_service(service), m_clipId(clipId), m_oldIndex(oldIndex), m_newIndex(newIndex) {
    setText(QObject::tr("エフェクト順序変更"));
} // NOLINT(bugprone-easily-swappable-parameters)
void ReorderEffectCommand::undo() { m_service->reorderEffectsInternal(m_clipId, m_newIndex, m_oldIndex); }
void ReorderEffectCommand::redo() { m_service->reorderEffectsInternal(m_clipId, m_oldIndex, m_newIndex); }

ReorderAudioPluginCommand::ReorderAudioPluginCommand(TimelineService *service, int clipId, int oldIndex, int newIndex) : m_service(service), m_clipId(clipId), m_oldIndex(oldIndex), m_newIndex(newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("オーディオプラグイン順序変更"));
}
void ReorderAudioPluginCommand::undo() { m_service->reorderAudioPluginsInternal(m_clipId, m_newIndex, m_oldIndex); }
void ReorderAudioPluginCommand::redo() { m_service->reorderAudioPluginsInternal(m_clipId, m_oldIndex, m_newIndex); }

SetEffectEnabledCommand::SetEffectEnabledCommand(TimelineService *service, int clipId, int effectIndex, bool enabled) : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_enabled(enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("エフェクト有効/無効切り替え"));
}
void SetEffectEnabledCommand::undo() { m_service->setEffectEnabledInternal(m_clipId, m_effectIndex, !m_enabled); }
void SetEffectEnabledCommand::redo() { m_service->setEffectEnabledInternal(m_clipId, m_effectIndex, m_enabled); }

SetAudioPluginEnabledCommand::SetAudioPluginEnabledCommand(TimelineService *service, int clipId, int index, bool enabled) : m_service(service), m_clipId(clipId), m_index(index), m_enabled(enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("オーディオプラグイン有効/無効切り替え"));
}
void SetAudioPluginEnabledCommand::undo() { m_service->setAudioPluginEnabledInternal(m_clipId, m_index, !m_enabled); }
void SetAudioPluginEnabledCommand::redo() { m_service->setAudioPluginEnabledInternal(m_clipId, m_index, m_enabled); }

PasteEffectCommand::PasteEffectCommand(TimelineService *service, int clipId, int targetIndex, EffectModel *templateEffect)
    : m_service(service), m_clipId(clipId), m_targetIndex(targetIndex), m_effect(templateEffect->clone()) { // NOLINT(bugprone-easily-swappable-parameters)

    setText(QObject::tr("エフェクト貼り付け"));
}
void PasteEffectCommand::undo() { m_service->removeEffectInternal(m_clipId, m_targetIndex); }
void PasteEffectCommand::redo() { m_service->pasteEffectInternal(m_clipId, m_targetIndex, m_effect); }

UpdateLayerStateCommand::UpdateLayerStateCommand(TimelineService *service, int sceneId, int layer, bool value, StateType type)
    : m_service(service), m_sceneId(sceneId), m_layer(layer), m_value(value), m_type(type) { // NOLINT(bugprone-easily-swappable-parameters)
    QString actionName = (type == Lock) ? (value ? QObject::tr("レイヤーロック") : QObject::tr("ロック解除")) : (value ? QObject::tr("レイヤー非表示") : QObject::tr("レイヤー表示"));
    setText(QObject::tr("%1: レイヤー %2").arg(actionName).arg(m_layer));
}
void UpdateLayerStateCommand::undo() { m_service->setLayerStateInternal(m_sceneId, m_layer, !m_value, m_type); }
void UpdateLayerStateCommand::redo() { m_service->setLayerStateInternal(m_sceneId, m_layer, m_value, m_type); }

SplitClipCommand::SplitClipCommand(TimelineService *service, int clipId, int frame, const QString &clipName)
    : m_service(service), m_originalClipId(clipId), m_newClipId(-1), m_splitFrame(frame), m_originalDuration(0), m_clipName(clipName) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("クリップ分割: %1").arg(clipName));
}

void SplitClipCommand::undo() {
    m_service->deleteClipInternal(m_newClipId);
    // 元のクリップの長さを復元
    const auto &clips = m_service->clips();
    auto it = std::ranges::find_if(clips, [this](const ClipData &c) -> bool { return c.id == m_originalClipId; });
    if (it != clips.end()) {
        m_service->updateClipInternal(m_originalClipId, it->layer, it->startFrame, m_originalDuration);
    }
}

void SplitClipCommand::redo() {
    const auto &clips = m_service->clips();
    auto it = std::ranges::find_if(clips, [this](const ClipData &c) -> bool { return c.id == m_originalClipId; });
    if (it == clips.end()) {
        return;
    }

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

    for (int i = 0; i < it->effects.size() && i < newClip.effects.size(); ++i) {
        auto *originalEffect = it->effects.value(i);
        auto *newEffect = newClip.effects.value(i);
        if ((originalEffect == nullptr) || (newEffect == nullptr)) {
            continue;
        }

        QVariantMap secondHalfTracks = originalEffect->splitTracks(firstHalfDuration, m_originalDuration);
        originalEffect->syncTrackEndpoints(firstHalfDuration);
        newEffect->setKeyframeTracks(secondHalfTracks);
        newEffect->syncTrackEndpoints(secondHalfDuration);
    }

    m_service->updateClipInternal(m_originalClipId, it->layer, it->startFrame, firstHalfDuration);
    m_service->addClipDirectInternal(newClip);
}

DeleteClipCommand::DeleteClipCommand(TimelineService *service, int clipId, const QString &clipName) : m_service(service), m_clipId(clipId) {
    const auto *clip = service->findClipById(clipId);
    if (clip != nullptr) {
        m_snapshot = service->deepCopyClip(*clip);
    }
    setText(QObject::tr("クリップ削除: %1").arg(clipName));
}
void DeleteClipCommand::redo() { m_service->deleteClipInternal(m_clipId); }
void DeleteClipCommand::undo() {
    m_snapshot.id = m_clipId;
    m_service->addClipDirectInternal(m_snapshot);
}

DeleteClipsCommand::DeleteClipsCommand(TimelineService *service, const QList<int> &clipIds, const QString &macroText) : m_service(service), m_clipIds(clipIds) {
    setText(macroText);
    for (int id : std::as_const(clipIds)) {
        const auto *clip = service->findClipById(id);
        if (clip != nullptr) {
            ClipData snap = service->deepCopyClip(*clip);
            snap.id = id; // 重要: 削除前の元のIDをスナップショットに保存
            m_snapshots.append(snap);
        }
    }
}
void DeleteClipsCommand::redo() {
    for (int id : std::as_const(m_clipIds)) {
        m_service->deleteClipInternal(id, false);
    }
    emit m_service->clipsChanged();
}
void DeleteClipsCommand::undo() { m_service->addClipsDirectInternal(m_snapshots); }

CutClipCommand::CutClipCommand(TimelineService *service, int clipId, const QString &clipName) : m_service(service), m_clipId(clipId) {
    const auto *clip = service->findClipById(clipId);
    if (clip != nullptr) {
        m_snapshot = service->deepCopyClip(*clip);
    }
    setText(QObject::tr("切り取り: %1").arg(clipName));
}
void CutClipCommand::redo() {
    m_service->setClipboard(m_snapshot);
    m_service->deleteClipInternal(m_clipId);
}
void CutClipCommand::undo() {
    m_snapshot.id = m_clipId;
    m_service->addClipDirectInternal(m_snapshot);
}

PasteClipCommand::PasteClipCommand(TimelineService *service, int newClipId, const ClipData &clipData) : m_service(service), m_newClipId(newClipId), m_clipData(clipData) { setText(QObject::tr("貼り付け: %1").arg(clipData.type)); }
void PasteClipCommand::redo() {
    ClipData c = m_clipData;
    c.id = m_newClipId;
    m_service->addClipDirectInternal(c);
}
void PasteClipCommand::undo() { m_service->deleteClipInternal(m_newClipId); }

SetKeyframeCommand::SetKeyframeCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, int frame, QVariant newValue, QVariantMap options, QVariant oldValue, QVariantMap oldOptions,
                                       bool wasExisting) // NOLINT(bugprone-easily-swappable-parameters)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_frame(frame), m_paramName(paramName), m_newValue(std::move(newValue)), m_oldValue(std::move(oldValue)), m_newOptions(std::move(options)), m_oldOptions(std::move(oldOptions)),
      m_wasExisting(wasExisting) {
    setText(QObject::tr("キーフレーム設定: %1").arg(paramName));
}
void SetKeyframeCommand::redo() { m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_newValue, m_newOptions); }
void SetKeyframeCommand::undo() {
    if (m_wasExisting) {
        m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_oldValue, m_oldOptions);
    } else {
        m_service->removeKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame);
    }
}
auto SetKeyframeCommand::id() const -> int { return 1002; }
auto SetKeyframeCommand::mergeWith(const QUndoCommand *other) -> bool {
    if (other->id() != id()) {
        return false;
    }
    const auto *cmd = dynamic_cast<const SetKeyframeCommand *>(other);
    if (cmd->m_clipId != m_clipId || cmd->m_effectIndex != m_effectIndex || cmd->m_paramName != m_paramName || cmd->m_frame != m_frame) {
        return false;
    }
    m_newValue = cmd->m_newValue;
    m_newOptions = cmd->m_newOptions;
    return true;
}

RemoveKeyframeCommand::RemoveKeyframeCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, int frame, QVariant savedValue, QVariantMap savedOptions) // NOLINT(bugprone-easily-swappable-parameters)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_frame(frame), m_paramName(paramName), m_savedValue(std::move(savedValue)), m_savedOptions(std::move(savedOptions)) {
    setText(QObject::tr("キーフレーム削除: %1 [%2]").arg(paramName).arg(frame));
}
void RemoveKeyframeCommand::redo() { m_service->removeKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame); }
void RemoveKeyframeCommand::undo() { m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_savedValue, m_savedOptions); }

AddSceneCommand::AddSceneCommand(TimelineService *service, int sceneId, const QString &name) : m_service(service), m_sceneId(sceneId), m_name(name) { setText(QObject::tr("シーン追加: %1").arg(name)); }
void AddSceneCommand::redo() { m_service->createSceneInternal(m_sceneId, m_name); }
void AddSceneCommand::undo() { m_service->removeSceneInternal(m_sceneId); }

RemoveSceneCommand::RemoveSceneCommand(TimelineService *service, int sceneId, const QString &name) : m_service(service), m_sceneId(sceneId) {
    for (const auto &s : service->getAllScenes()) {
        if (s.id == sceneId) {
            m_snapshot = s;
            break;
        }
    }
    setText(QObject::tr("シーン削除: %1").arg(name));
}
void RemoveSceneCommand::redo() { m_service->removeSceneInternal(m_sceneId); }
void RemoveSceneCommand::undo() { m_service->restoreSceneInternal(m_snapshot); }

UpdateSceneSettingsCommand::UpdateSceneSettingsCommand(TimelineService *service, int sceneId, SceneData oldData, const SceneData &newData)
    : m_service(service), m_sceneId(sceneId), m_oldData(std::move(oldData)), m_newData(newData) { // NOLINT(bugprone-easily-swappable-parameters)
    setText(QObject::tr("シーン設定変更: %1").arg(newData.name));
}
void UpdateSceneSettingsCommand::redo() { m_service->applySceneSettingsInternal(m_sceneId, m_newData); }
void UpdateSceneSettingsCommand::undo() { m_service->applySceneSettingsInternal(m_sceneId, m_oldData); }

TimelineService::TimelineService(SelectionService *selection, QObject *parent) : QObject(parent), m_undoStack(new QUndoStack(this)), m_selection(selection) {
    // 初期シーンを作成
    SceneData rootScene;
    rootScene.id = 0;
    rootScene.name = QObject::tr("ルート");
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    rootScene.width = settings.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    rootScene.height = settings.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    rootScene.fps = settings.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    m_scenes.append(rootScene);
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

void TimelineService::createClipInternal(int clipId, const QString &type, int startFrame, int layer, bool emitSignal) {
    startFrame = std::max(startFrame, 0);
    layer = std::max(layer, 0);

    if (isLayerLocked(layer)) {
        qWarning() << "createClipInternal: レイヤー" << layer << "はロックされています。";
        return;
    }

    const int defaultDuration = Rina::Core::SettingsManager::instance().settings().value(QStringLiteral("defaultClipDuration"), 100).toInt();
    auto overlaps = [](int s1, int d1, int s2, int d2) -> bool { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();
    for (const auto &c : std::as_const(currentClips)) {
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
    addEffectInternal(clipId, QStringLiteral("transform"));
    addEffectInternal(clipId, type);

    if (emitSignal) {
        emit clipsChanged();
        emit clipCreated(newClip.id, newClip.layer, newClip.startFrame, newClip.durationFrames, newClip.type);
    }
}

void TimelineService::addClipsDirectInternal(const QList<ClipData> &clips) {
    for (const auto &clip : std::as_const(clips)) {
        addClipDirectInternal(clip, false);
    }
    emit clipsChanged();
}

void TimelineService::updateClip(int id, int layer, int startFrame, int duration) {
    const auto *clip = findClipById(id);
    if (clip == nullptr) {
        return;
    }

    QString clipName = clip->type;
    if (!clip->effects.isEmpty()) {
        clipName = clip->effects.first()->name();
    }

    m_undoStack->push(new MoveClipCommand(this, id, clip->layer, clip->startFrame, clip->durationFrames, layer, startFrame, duration, clipName));
}

void TimelineService::applyClipBatchMove(const QVariantList &moves) {
    if (moves.isEmpty()) {
        return;
    }

    m_batchExcludes.clear();
    for (const QVariant &vMove : std::as_const(moves)) {
        m_batchExcludes.insert(vMove.toMap().value(QStringLiteral("id")).toInt());
    }

    struct PendingOp {
        int id;
        int oldLayer;
        int oldStart;
        int targetLayer;
        int targetStart;
        int duration;
        QString name;
    };

    QVector<PendingOp> pending;
    pending.reserve(moves.size());

    for (const QVariant &vMove : std::as_const(moves)) {
        QVariantMap move = vMove.toMap();
        int id = move.value(QStringLiteral("id")).toInt();
        const auto *clip = findClipById(id);
        if (clip != nullptr) {
            pending.push_back(PendingOp{.id = id,
                                        .oldLayer = clip->layer,
                                        .oldStart = clip->startFrame,
                                        .targetLayer = move.value(QStringLiteral("layer")).toInt(),
                                        .targetStart = move.value(QStringLiteral("startFrame")).toInt(),
                                        .duration = move.value(QStringLiteral("duration")).toInt(),
                                        .name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
        }
    }

    // 衝突回避ロジック:
    // グループ全体の相対位置を崩さないため、どれか1つでも衝突したらグループ全体を右へ押し出す。
    // それを他のクリップに当たらなくなるまで最大100回反復する。
    int maxPush = 0;
    bool needsPush = true;
    int loopCount = 0;

    while (needsPush && loopCount < 100) {
        needsPush = false;
        int currentPush = 0;

        for (const auto &op : std::as_const(pending)) {
            int testStart = op.targetStart + maxPush;
            int safeStart = findVacantFrame(op.targetLayer, testStart, op.duration, op.id);
            if (safeStart > testStart) {
                int push = safeStart - testStart;
                currentPush = std::max(push, currentPush);
            }
        }

        if (currentPush > 0) {
            maxPush += currentPush;
            needsPush = true;
        }
        loopCount++;
    }

    m_undoStack->beginMacro(QObject::tr("複数クリップ絶対移動: %1").arg(pending.size()));
    for (const auto &op : std::as_const(pending)) {
        int finalStart = op.targetStart + maxPush;
        m_undoStack->push(new MoveClipCommand(this, op.id, op.oldLayer, op.oldStart, op.duration, op.targetLayer, finalStart, op.duration, op.name));
    }
    m_undoStack->endMacro();
    m_batchExcludes.clear();
}

void TimelineService::moveSelectedClips(int deltaLayer, int deltaFrame) {
    if ((m_selection == nullptr) || (deltaLayer == 0 && deltaFrame == 0)) {
        return;
    }

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    struct PendingOp {
        int id;
        int oldLayer;
        int oldStart;
        int duration;
        QString name;
    };

    QVector<PendingOp> pending;
    pending.reserve(ids.size());

    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (clip == nullptr) {
            continue;
        }

        pending.push_back(PendingOp{.id = id, .oldLayer = clip->layer, .oldStart = clip->startFrame, .duration = clip->durationFrames, .name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
    }

    if (deltaFrame > 0 || (deltaFrame == 0 && deltaLayer > 0)) {
        std::ranges::sort(pending, [](const PendingOp &a, const PendingOp &b) -> bool {
            if (a.oldStart != b.oldStart) {
                return a.oldStart > b.oldStart;
            }
            return a.oldLayer > b.oldLayer;
        });
    } else {
        std::ranges::sort(pending, [](const PendingOp &a, const PendingOp &b) -> bool {
            if (a.oldStart != b.oldStart) {
                return a.oldStart < b.oldStart;
            }
            return a.oldLayer < b.oldLayer;
        });
    }

    m_undoStack->beginMacro(QObject::tr("複数クリップ移動: %1").arg(pending.size()));
    for (const PendingOp &clip : std::as_const(pending)) {
        const int newLayer = std::max(0, clip.oldLayer + deltaLayer);
        const int newStart = std::max(0, clip.oldStart + deltaFrame);
        m_undoStack->push(new MoveClipCommand(this, clip.id, clip.oldLayer, clip.oldStart, clip.duration, newLayer, newStart, clip.duration, clip.name));
    }
    m_undoStack->endMacro();
}

void TimelineService::resizeSelectedClips(int deltaStartFrame, int deltaDuration) {
    if ((m_selection == nullptr) || (deltaStartFrame == 0 && deltaDuration == 0)) {
        return;
    }

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    struct PendingOp {
        int id;
        int oldLayer;
        int oldStart;
        int duration;
        QString name;
    };

    QVector<PendingOp> pending;
    pending.reserve(ids.size());

    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (clip == nullptr) {
            continue;
        }

        pending.push_back(PendingOp{.id = id, .oldLayer = clip->layer, .oldStart = clip->startFrame, .duration = clip->durationFrames, .name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
    }

    // Resize left side -> deltaStartFrame != 0. If deltaStartFrame > 0, left edge moves right.
    // Resize right side -> deltaStartFrame == 0, deltaDuration != 0.
    // In any case, order matters if they push each other.
    if (deltaStartFrame > 0 || deltaDuration > 0) {
        std::ranges::sort(pending, [](const PendingOp &a, const PendingOp &b) -> bool {
            if (a.oldStart != b.oldStart) {
                return a.oldStart > b.oldStart;
            }
            return a.oldLayer > b.oldLayer;
        });
    } else {
        std::ranges::sort(pending, [](const PendingOp &a, const PendingOp &b) -> bool {
            if (a.oldStart != b.oldStart) {
                return a.oldStart < b.oldStart;
            }
            return a.oldLayer < b.oldLayer;
        });
    }

    m_undoStack->beginMacro(QObject::tr("複数クリップ変形: %1").arg(pending.size()));
    for (const PendingOp &clip : std::as_const(pending)) {
        const int newStart = std::max(0, clip.oldStart + deltaStartFrame);
        const int newDuration = std::max(1, clip.duration + deltaDuration);
        m_undoStack->push(new MoveClipCommand(this, clip.id, clip.oldLayer, clip.oldStart, clip.duration, clip.oldLayer, newStart, newDuration, clip.name));
    }
    m_undoStack->endMacro();
}

auto TimelineService::computeMagneticSnapPosition(int clipId, int targetLayer, int proposedStartFrame) -> int {
    // 0. 移動対象のクリップ情報を取得
    const auto *movingClip = findClipById(clipId);
    if (movingClip == nullptr) {
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
    std::ranges::sort(layerClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    // 4. スナップ候補となる「空き領域の始点」を生成
    QList<int> snapPoints;

    // 4-1. 最初のクリップの前が空いていれば、タイムラインの先頭(0)を候補に
    if (layerClips.first()->startFrame >= clipDuration) {
        snapPoints.append(0);
    }

    // 4-2. クリップ間の隙間を候補に
    for (int i = 0; i < layerClips.size() - 1; ++i) {
        int gapStart = layerClips.value(i)->startFrame + layerClips.value(i)->durationFrames;
        int gapEnd = layerClips.value(i + 1)->startFrame;
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
            int delta = std::abs(proposedStartFrame - snapPoints.value(i));
            if (delta < minDelta) {
                minDelta = delta;
                bestSnapPoint = snapPoints.value(i);
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

auto TimelineService::resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame, const QVariantList &batchIds) -> QPoint { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *movingClip = findClipById(clipId);
    if (movingClip == nullptr) {
        return {proposedStartFrame, targetLayer};
    }

    int deltaLayer = targetLayer - movingClip->layer;
    int deltaFrame = proposedStartFrame - movingClip->startFrame;

    QSet<int> movingIds;
    if (!batchIds.isEmpty()) {
        for (const QVariant &v : std::as_const(batchIds)) {
            movingIds.insert(v.toInt());
        }
    } else if ((m_selection != nullptr) && m_selection->isSelected(clipId)) {
        for (const QVariant &v : m_selection->selectedClipIds()) {
            movingIds.insert(v.toInt());
        }
    } else {
        movingIds.insert(clipId);
    }

    int maxPush = 0;
    bool needsPush = true;
    int loopCount = 0;

    QSet<int> backupExcludes = m_batchExcludes;

    while (needsPush && loopCount < 100) {
        needsPush = false;
        int currentPush = 0;

        for (int id : std::as_const(movingIds)) {
            const auto *c = findClipById(id);
            if (c == nullptr) {
                continue;
            }

            int tLayer = c->layer + deltaLayer;
            tLayer = std::max(tLayer, 0);
            tLayer = std::min(tLayer, 127);

            // ターゲットレイヤーがロックされている場合は、そのクリップの移動を制限
            if (isLayerLocked(tLayer) || isLayerLocked(c->layer)) {
                tLayer = c->layer;
            }

            int testStart = c->startFrame + deltaFrame + maxPush;
            testStart = std::max(testStart, 0);

            m_batchExcludes = movingIds;
            int safeStart = findVacantFrame(tLayer, testStart, c->durationFrames, id);
            m_batchExcludes = backupExcludes;

            if (safeStart > testStart) {
                int push = safeStart - testStart;
                currentPush = std::max(push, currentPush);
            }
        }

        if (currentPush > 0) {
            maxPush += currentPush;
            needsPush = true;
        }
        loopCount++;
    }

    int finalFrame = proposedStartFrame + maxPush;
    finalFrame = std::max(finalFrame, 0);

    int finalLayer = targetLayer;
    finalLayer = std::max(finalLayer, 0);
    finalLayer = std::min(finalLayer, 127);

    return {finalFrame, finalLayer};
}

void TimelineService::updateClipInternal(int id, int layer, int startFrame, int duration, bool emitSignal) {
    const auto *existingClip = findClipById(id);
    if (existingClip == nullptr) {
        return;
    }

    // 移動元または移動先がロックされている場合は拒否
    if (isLayerLocked(layer) || isLayerLocked(existingClip->layer)) {
        qWarning() << "updateClipInternal: ロックされたレイヤーへの/からの操作を拒否しました。";
        return;
    }

    startFrame = std::max(startFrame, 0);
    duration = std::max(duration, 1);
    layer = std::max(layer, 0);

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
                for (auto *effect : std::as_const(clip.effects)) {
                    if (effect != nullptr) {
                        effect->syncTrackEndpoints(duration);
                    }
                }
                if (emitSignal) {
                    emit clipsChanged();
                }
                // 選択中のクリップであればSelectionServiceのキャッシュも更新する
                if (m_selection->selectedClipId() == id) {
                    QVariantMap data = m_selection->selectedClipData();
                    data.insert(QStringLiteral("layer"), layer);
                    data.insert(QStringLiteral("startFrame"), startFrame);
                    data.insert(QStringLiteral("durationFrames"), duration);
                    m_selection->refreshSelectionData(id, data);
                }
            }
            break;
        }
    }
}

void TimelineService::selectClip(int id) { applySelectionIds(QVariantList{id}); }

void TimelineService::toggleSelection(int id, const QVariantMap &data) {
    if (m_selection == nullptr) {
        return;
    }

    QVariantList ids = m_selection->selectedClipIds();
    int idx = -1;
    for (int i = 0; i < ids.size(); ++i) {
        if (ids.value(i).toInt() == id) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        ids.removeAt(idx);
    } else {
        ids.prepend(id);
    }

    applySelectionIds(ids);
}

void TimelineService::applySelectionIds(const QVariantList &ids) {
    int primaryId = -1;
    QVariantMap primaryData;

    // 選択されたクリップのリストを更新
    QVariantList newSelectedIds;
    for (const QVariant &v : std::as_const(ids)) {
        if (!newSelectedIds.contains(v)) {
            newSelectedIds.append(v);
        }
    }

    if (!newSelectedIds.isEmpty()) {
        int id = newSelectedIds.first().toInt(); // 最初のクリップをプライマリとする
        const auto *clip = findClipById(id);
        if (clip != nullptr) { // findClipById は nullptr を返す可能性があるのでチェック
            primaryId = clip->id;
            for (auto *eff : clip->effects) {
                QVariantMap params = eff->params();
                for (auto it = params.begin(); it != params.end(); ++it) {
                    primaryData.insert(it.key(), it.value());
                }
            }
            primaryData.insert(QStringLiteral("startFrame"), clip->startFrame);
            primaryData.insert(QStringLiteral("durationFrames"), clip->durationFrames);
            primaryData.insert(QStringLiteral("layer"), clip->layer);
            primaryData.insert(QStringLiteral("type"), clip->type);
        }
    }

    // SelectionService の replaceSelection を呼び出す
    m_selection->replaceSelection(newSelectedIds, primaryId, primaryData);
}

void TimelineService::selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive) { // NOLINT(bugprone-easily-swappable-parameters)
    const int minFrame = std::min(frameA, frameB);
    const int maxFrame = std::max(frameA, frameB);
    const int minLayer = std::min(layerA, layerB);
    const int maxLayer = std::max(layerA, layerB);

    QVariantList ids;
    int primaryId = -1;
    QVariantMap primaryData;

    for (const auto &clip : clips()) {
        const int clipStart = clip.startFrame;
        const int clipEnd = clip.startFrame + clip.durationFrames;
        const bool frameOverlap = clipStart < maxFrame && minFrame < clipEnd;
        const bool layerMatch = clip.layer >= minLayer && clip.layer <= maxLayer;
        if (!frameOverlap || !layerMatch) {
            continue;
        }

        ids.append(clip.id);

        if (primaryId == -1) {
            primaryId = clip.id;
            for (auto *eff : std::as_const(clip.effects)) {
                QVariantMap params = eff->params();
                for (auto it = params.begin(); it != params.end(); ++it) {
                    primaryData.insert(it.key(), it.value());
                }
            }
            primaryData.insert(QStringLiteral("startFrame"), clip.startFrame);
            primaryData.insert(QStringLiteral("durationFrames"), clip.durationFrames);
            primaryData.insert(QStringLiteral("layer"), clip.layer);
            primaryData.insert(QStringLiteral("type"), clip.type);
        }
    }

    // 既存の選択とマージ
    if (additive) {
        QVariantList merged = m_selection->selectedClipIds();
        for (const QVariant &id : std::as_const(ids)) {
            if (!merged.contains(id)) {
                merged.append(id);
            }
        }
        // プライマリIDがまだ設定されていなければ、マージ後の最初のクリップをプライマリにする
        if (primaryId == -1 && !merged.isEmpty()) {
            primaryId = merged.first().toInt();
        }
        m_selection->replaceSelection(merged, primaryId, primaryData);
        return;
    }
    m_selection->replaceSelection(ids, primaryId, primaryData);
}

void TimelineService::deleteSelectedClips() {
    if (m_selection == nullptr) {
        return;
    }
    deleteClipsByIds(m_selection->selectedClipIds());
}

void TimelineService::deleteClipsByIds(const QVariantList &ids) {
    if (ids.isEmpty()) {
        return;
    }

    QList<int> intIds;
    for (const QVariant &v : std::as_const(ids)) {
        int id = v.toInt();
        if (id >= 0) {
            intIds.append(id);
        }
    }

    if (intIds.isEmpty()) {
        return;
    }

    QString macroText = intIds.size() == 1 ? QObject::tr("クリップ削除") : QObject::tr("複数クリップ削除: %1").arg(intIds.size());

    // 既存の複数削除コマンドを再利用してUndo/Redoスタックに積む
    m_undoStack->push(new DeleteClipsCommand(this, intIds, macroText));

    // 削除後は選択状態をクリアする
    m_selection->clearSelection();
}

void TimelineService::deleteClip(int clipId) { deleteClipsByIds({clipId}); }

void TimelineService::deleteClipInternal(int clipId, bool emitSignal) {
    auto &currentClips = clipsMutable();
    auto it = std::ranges::find_if(currentClips, [clipId](const ClipData &c) -> bool { return c.id == clipId; });
    if (it != currentClips.end()) {
        for (auto *eff : it->effects) {
            eff->deleteLater();
        }
        currentClips.erase(it);
        if (emitSignal) {
            emit clipsChanged();
        }
    }
}

void TimelineService::addEffect(int clipId, const QString &effectId) {
    auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
    if (meta.id.isEmpty()) {
        return;
    }

    m_undoStack->push(new AddEffectCommand(this, clipId, effectId, meta.name));
}

void TimelineService::addEffectInternal(int clipId, const QString &effectId) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
            auto *model = new EffectModel(meta.id, meta.name, meta.category, meta.defaultParams, meta.qmlSource, meta.uiDefinition, this);
            model->syncTrackEndpoints(clip.durationFrames);
            connect(model, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
            clip.effects.append(model);
            emit clipsChanged();
            emit clipEffectsChanged(clipId);
            break;
        }
    }
}

void TimelineService::addClipDirectInternal(const ClipData &clip, bool emitSignal) {
    clipsMutable().append(clip);
    if (emitSignal) {
        emit clipsChanged();
        emit clipCreated(clip.id, clip.layer, clip.startFrame, clip.durationFrames, clip.type);
    }
}

void TimelineService::restoreEffectInternal(int clipId, const QVariantMap &data) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            auto meta = Rina::Core::EffectRegistry::instance().getEffect(data.value(QStringLiteral("id")).toString());
            auto *model = new EffectModel(data.value(QStringLiteral("id")).toString(), data.value(QStringLiteral("name")).toString(), meta.category, data.value(QStringLiteral("params")).toMap(), data.value(QStringLiteral("qmlSource")).toString(),
                                          data.value(QStringLiteral("uiDefinition")).toMap(), this);
            model->setEnabled(data.value(QStringLiteral("enabled")).toBool());
            model->setKeyframeTracks(data.value(QStringLiteral("keyframes")).toMap());
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
    if (clip == nullptr) {
        return;
    }

    int idx = (effectIndex == -1) ? static_cast<int>(clip->effects.size()) - 1 : effectIndex;
    if (idx >= 0 && idx < clip->effects.size()) {
        auto *eff = clip->effects.value(idx);
        removedData.insert(QStringLiteral("id"), eff->id());
        removedData.insert(QStringLiteral("name"), eff->name());
        removedData.insert(QStringLiteral("enabled"), eff->isEnabled());
        removedData.insert(QStringLiteral("params"), eff->params());
        removedData.insert(QStringLiteral("qmlSource"), eff->qmlSource());
        removedData.insert(QStringLiteral("uiDefinition"), eff->uiDefinition());
        removedData.insert(QStringLiteral("keyframes"), eff->keyframeTracks());

        auto *cmd = new RemoveEffectCommand(this, clipId, effectIndex, eff->name());
        cmd->setRemovedEffect(removedData);
        m_undoStack->push(cmd);
    }
}

void TimelineService::removeEffectInternal(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex == -1) {
                effectIndex = static_cast<int>(clip.effects.size()) - 1;
            }
            if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                // Transformエフェクトは削除不可
                if (effectIndex == 0 && clip.effects.value(0)->id() == QStringLiteral("transform")) {
                    return;
                }
                auto *eff = clip.effects.takeAt(effectIndex);
                eff->deleteLater();
                emit clipsChanged();
                emit clipEffectsChanged(clipId);
            }
            break;
        }
    }
}

void TimelineService::setEffectEnabled(int clipId, int effectIndex, bool enabled) { m_undoStack->push(new SetEffectEnabledCommand(this, clipId, effectIndex, enabled)); }

void TimelineService::setAudioPluginEnabled(int clipId, int index, bool enabled) { m_undoStack->push(new SetAudioPluginEnabledCommand(this, clipId, index, enabled)); }

void TimelineService::reorderEffects(int clipId, int oldIndex, int newIndex) {
    if (oldIndex == newIndex) {
        return;
    }
    m_undoStack->push(new ReorderEffectCommand(this, clipId, oldIndex, newIndex));
}

void TimelineService::reorderAudioPlugins(int clipId, int oldIndex, int newIndex) {
    if (oldIndex == newIndex) {
        return;
    }
    m_undoStack->push(new ReorderAudioPluginCommand(this, clipId, oldIndex, newIndex));
}

void TimelineService::reorderEffectsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || oldIndex < 0 || oldIndex >= static_cast<int>(clip->effects.size()) || newIndex < 0 || newIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    clip->effects.move(oldIndex, newIndex);

    // UI更新通知
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::setEffectEnabledInternal(int clipId, int effectIndex, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex < 0 || effectIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    clip->effects.value(effectIndex)->setEnabled(enabled);
    emit clipEffectsChanged(clipId);
}

void TimelineService::setAudioPluginEnabledInternal(int clipId, int index, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || index < 0 || index >= static_cast<int>(clip->audioPlugins.size())) {
        return;
    }

    clip->audioPlugins[index].enabled = enabled;

    emit clipEffectsChanged(clipId);
    emit clipsChanged(); // エンジン側の同期を促す
}

void TimelineService::reorderAudioPluginsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip == nullptr) || oldIndex < 0 || oldIndex >= static_cast<int>(clip->audioPlugins.size()) || newIndex < 0 || newIndex >= static_cast<int>(clip->audioPlugins.size())) {
        return;
    }

    clip->audioPlugins.move(oldIndex, newIndex);

    // UI更新通知
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::copyEffect(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex >= 0 && effectIndex < static_cast<int>(clip->effects.size())) {
        m_effectClipboard.reset(clip->effects.value(effectIndex)->clone());
    }
}

void TimelineService::pasteEffect(int clipId, int targetIndex) {
    if (!m_effectClipboard) {
        return;
    }
    m_undoStack->push(new PasteEffectCommand(this, clipId, targetIndex, m_effectClipboard.get()));
}

void TimelineService::pasteEffectInternal(int clipId, int targetIndex, EffectModel *effect) { // NOLINT(bugprone-easily-swappable-parameters)
    auto *clip = findClipById(clipId);
    if (clip != nullptr) {
        int idx = std::clamp(targetIndex, 0, static_cast<int>(clip->effects.size()));
        clip->effects.insert(idx, effect->clone());
        emit clipEffectsChanged(clipId);
        emit clipsChanged();
    }
}

void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    QVariant oldValue;
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= static_cast<int>(clip->effects.size())) {
        return;
    }

    const auto *eff = clip->effects.value(effectIndex);
    oldValue = eff->params().value(paramName);

    m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue, eff->name()));
}

void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex >= 0 && effectIndex < static_cast<int>(clip.effects.size())) {
                clip.effects.value(effectIndex)->setParam(paramName, value);

                // メディアの再読み込みが必要なパラメータ（ファイルパスや参照シーン）が変更された場合、
                // clipsChanged シグナルを発火させて MediaManager 等に通知する
                if (paramName == QLatin1String("path") || paramName == QLatin1String("source") || paramName == QStringLiteral("targetSceneId")) {
                    emit clipsChanged();
                }

                emit effectParamChanged(clipId, effectIndex, paramName, value);
                if (m_selection->selectedClipId() == clipId) {
                    QVariantMap data = m_selection->selectedClipData();
                    data.insert(paramName, value);
                    // select() は単一選択にリセットしてしまうため、
                    // 既存の複数選択リストを破壊しない refreshSelectionData を使用する。
                    // これにより UI からの意図しない書き戻しによる選択解除を防ぐ。
                    m_selection->refreshSelectionData(clipId, data);
                }
            }
            break;
        }
    }
}

auto TimelineService::findClipById(int clipId) -> ClipData * {
    auto &currentClips = clipsMutable();
    auto it = std::ranges::find_if(currentClips, [clipId](const ClipData &c) -> bool { return c.id == clipId; });
    return (it != currentClips.end()) ? &(*it) : nullptr;
}

auto TimelineService::findClipById(int clipId) const -> const ClipData * {
    const auto &currentClips = clips();
    auto it = std::ranges::find_if(currentClips, [clipId](const ClipData &c) -> bool { return c.id == clipId; });
    return (it != currentClips.end()) ? &(*it) : nullptr;
}

auto TimelineService::deepCopyClip(const ClipData &source) -> ClipData {
    ClipData newClip;
    newClip.id = -1;
    newClip.type = source.type;
    newClip.startFrame = source.startFrame;
    newClip.durationFrames = source.durationFrames;
    newClip.layer = source.layer;

    for (const auto *oldEffect : std::as_const(source.effects)) {
        auto *newEffect = new EffectModel(oldEffect->id(), oldEffect->name(), oldEffect->category(), oldEffect->params(), oldEffect->qmlSource(), oldEffect->uiDefinition(), this);
        newEffect->setEnabled(oldEffect->isEnabled());
        newEffect->setKeyframeTracks(oldEffect->keyframeTracks());
        newEffect->syncTrackEndpoints(source.durationFrames);
        connect(newEffect, &EffectModel::keyframeTracksChanged, this, &TimelineService::clipsChanged);
        newClip.effects.append(newEffect);
    }
    return newClip;
}

void TimelineService::copyClip(int clipId) {
    auto &currentClips = clipsMutable();
    auto it = std::ranges::find_if(currentClips, [clipId](const ClipData &c) -> bool { return c.id == clipId; });
    if (it == currentClips.end()) {
        return;
    }

    m_clipboard.clear();
    m_clipboard.append(deepCopyClip(*it));
}

void TimelineService::copySelectedClips() {
    QList<ClipData> copied;
    const QVariantList ids = m_selection->selectedClipIds();
    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (clip != nullptr) {
            copied.append(deepCopyClip(*clip));
        }
    }
    if (!copied.isEmpty()) {
        setClipboard(copied);
    }
}

void TimelineService::cutClip(int clipId) {
    const auto *clip = findClipById(clipId); // findClipById は const なので、ここでコピー
    if (clip == nullptr) {
        return;
    }
    QString name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name();
    m_undoStack->push(new CutClipCommand(this, clipId, name));
}

void TimelineService::cutSelectedClips() {
    if ((m_selection == nullptr) || m_selection->selectedClipIds().isEmpty()) {
        return;
    }

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    QList<ClipData> copied;
    for (const QVariant &v : std::as_const(ids)) {
        const auto *clip = findClipById(v.toInt());
        if (clip != nullptr) {
            copied.append(deepCopyClip(*clip));
        }
    }
    setClipboard(copied); // クリップボードにコピー

    QList<int> intIds;
    for (const QVariant &v : std::as_const(ids)) {
        intIds.append(v.toInt());
    }
    m_undoStack->push(new DeleteClipsCommand(this, intIds, QString(QStringLiteral("複数クリップ切り取り: %1")).arg(ids.size())));
    m_selection->clearSelection();
}

void TimelineService::splitSelectedClips(int frame) {
    if (m_selection == nullptr) {
        return;
    }
    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty()) {
        return;
    }

    m_undoStack->beginMacro(QObject::tr("複数クリップ分割: %1").arg(ids.size()));
    for (const QVariant &v : std::as_const(ids)) {
        splitClip(v.toInt(), frame);
    }
    m_undoStack->endMacro();
}

void TimelineService::pasteClip(int frame, int layer) {
    if (m_clipboard.isEmpty()) {
        return;
    }

    frame = std::max(frame, 0);
    layer = std::max(layer, 0);

    auto overlaps = [](int s1, int d1, int s2, int d2) -> bool { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();

    int baseFrame = m_clipboard.first().startFrame;
    int baseLayer = m_clipboard.first().layer;
    for (const auto &clip : std::as_const(m_clipboard)) {
        baseFrame = std::min(baseFrame, clip.startFrame);
        baseLayer = std::min(baseLayer, clip.layer);
    }

    QList<ClipData> pending;
    for (const auto &src : std::as_const(m_clipboard)) {
        ClipData newClip = deepCopyClip(src);
        newClip.startFrame = frame + (src.startFrame - baseFrame);
        newClip.layer = std::max(0, layer + (src.layer - baseLayer));

        for (const auto &c : std::as_const(currentClips)) {
            if (c.layer == newClip.layer && overlaps(newClip.startFrame, newClip.durationFrames, c.startFrame, c.durationFrames)) {
                qWarning() << "クリップ貼り付けを拒否: レイヤー" << newClip.layer << "の" << newClip.startFrame << "フレームで衝突が発生";
                return;
            }
        }
        for (const auto &c : std::as_const(pending)) {
            if (c.layer == newClip.layer && overlaps(newClip.startFrame, newClip.durationFrames, c.startFrame, c.durationFrames)) {
                qWarning() << "クリップ貼り付けを拒否: 貼り付け対象同士が衝突";
                return;
            }
        }

        pending.append(newClip);
    }

    if (pending.size() == 1) {
        int newId = m_nextClipId++;
        m_undoStack->push(new PasteClipCommand(this, newId, pending.first()));
        return;
    }

    m_undoStack->beginMacro(QObject::tr("複数クリップ貼り付け: %1").arg(pending.size()));
    for (const auto &clip : std::as_const(pending)) {
        int newId = m_nextClipId++;
        m_undoStack->push(new PasteClipCommand(this, newId, clip));
    }
    m_undoStack->endMacro();
}

void TimelineService::splitClip(int clipId, int frame) {
    const auto *clip = findClipById(clipId);
    if (clip == nullptr) {
        return;
    }

    if (frame > clip->startFrame && frame < clip->startFrame + clip->durationFrames) {
        QString clipName = clip->type;
        if (!clip->effects.isEmpty()) {
            clipName = clip->effects.first()->name();
        }
        m_undoStack->push(new SplitClipCommand(this, clipId, frame, clipName));
    }
}

auto TimelineService::currentScene() -> SceneData * {
    for (auto &scene : m_scenes) {
        if (scene.id == m_currentSceneId) {
            return &scene;
        }
    }
    if (m_scenes.isEmpty()) {
        static SceneData dummy;
        return &dummy;
    }
    return m_scenes.data();
}

auto TimelineService::currentScene() const -> const SceneData * {
    for (const auto &scene : std::as_const(m_scenes)) {
        if (scene.id == m_currentSceneId) {
            return &scene;
        }
    }
    if (m_scenes.isEmpty()) {
        static SceneData dummy;
        return &dummy;
    }
    return m_scenes.data();
}

auto TimelineService::clips() const -> const QList<ClipData> & { return currentScene()->clips; }

auto TimelineService::clipsMutable() -> QList<ClipData> & { return currentScene()->clips; }

auto TimelineService::clips(int sceneId) const -> const QList<ClipData> & {
    for (const auto &scene : std::as_const(m_scenes)) {
        if (scene.id == sceneId) {
            return scene.clips;
        }
    }
    static QList<ClipData> empty;
    return empty;
}

auto TimelineService::scenes() const -> QVariantList {
    QVariantList list;
    for (const auto &scene : std::as_const(m_scenes)) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), scene.id);
        map.insert(QStringLiteral("name"), scene.name);
        map.insert(QStringLiteral("width"), scene.width);
        map.insert(QStringLiteral("height"), scene.height);
        map.insert(QStringLiteral("fps"), scene.fps);
        map.insert(QStringLiteral("totalFrames"), scene.totalFrames);
        map.insert(QStringLiteral("gridMode"), scene.gridMode);
        map.insert(QStringLiteral("gridBpm"), scene.gridBpm);
        map.insert(QStringLiteral("gridOffset"), scene.gridOffset);
        map.insert(QStringLiteral("gridInterval"), scene.gridInterval);
        map.insert(QStringLiteral("gridSubdivision"), scene.gridSubdivision);
        map.insert(QStringLiteral("enableSnap"), scene.enableSnap);
        map.insert(QStringLiteral("magneticSnapRange"), scene.magneticSnapRange);
        list.append(map);
    }
    return list;
}

void TimelineService::setScenes(const QList<SceneData> &scenes) {
    m_scenes = scenes;
    if (m_scenes.isEmpty()) {
        createScene(QObject::tr("ルート"));
    }
    // 現在のシーンIDが有効か確認
    bool found = false;
    for (const auto &s : std::as_const(m_scenes)) {
        if (s.id == m_currentSceneId) {
            found = true;
            break;
        }
    }
    if (!found && !m_scenes.isEmpty()) {
        m_currentSceneId = m_scenes.first().id;
    }
    emit scenesChanged();
    emit currentSceneIdChanged();
    emit clipsChanged();
}

void TimelineService::createScene(const QString &name) {
    int id = m_nextSceneId++;
    m_undoStack->push(new AddSceneCommand(this, id, name));
}

void TimelineService::removeScene(int sceneId) {
    for (const auto &s : getAllScenes()) {
        if (s.id == sceneId) {
            m_undoStack->push(new RemoveSceneCommand(this, sceneId, s.name));
            return;
        }
    }
}

void TimelineService::switchScene(int sceneId) {
    if (m_currentSceneId == sceneId) {
        return;
    }

    bool exists = false;
    for (const auto &s : std::as_const(m_scenes)) {
        if (s.id == sceneId) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        return;
    }

    m_currentSceneId = sceneId;
    emit currentSceneIdChanged();
    emit clipsChanged();

    if (m_selection != nullptr) {
        m_selection->select(-1, QVariantMap());
    }
}

void TimelineService::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision,
                                          bool enableSnap, // NOLINT(bugprone-easily-swappable-parameters)
                                          int magneticSnapRange) {
    SceneData newData;
    SceneData oldData;
    for (const auto &s : getAllScenes()) {
        if (s.id == sceneId) {
            oldData = s;
            break;
        }
    }
    newData = oldData;
    newData.name = name;
    newData.width = width;
    newData.height = height;
    newData.fps = fps;
    newData.totalFrames = totalFrames;
    newData.gridMode = gridMode;
    newData.gridBpm = gridBpm;
    newData.gridOffset = gridOffset;
    newData.gridInterval = gridInterval;
    newData.gridSubdivision = gridSubdivision;
    newData.enableSnap = enableSnap;
    newData.magneticSnapRange = magneticSnapRange;
    m_undoStack->push(new UpdateSceneSettingsCommand(this, sceneId, oldData, newData));
}

auto TimelineService::isLayerLocked(int layer) const -> bool { return currentScene()->lockedLayers.contains(layer); }

auto TimelineService::isLayerHidden(int layer) const -> bool { return currentScene()->hiddenLayers.contains(layer); }

void TimelineService::setLayerState(int layer, bool value, int type) { m_undoStack->push(new UpdateLayerStateCommand(this, m_currentSceneId, layer, value, static_cast<UpdateLayerStateCommand::StateType>(type))); }

void TimelineService::setLayerStateInternal(int sceneId, int layer, bool value, int type) { // NOLINT(bugprone-easily-swappable-parameters)
    auto it = std::ranges::find_if(m_scenes, [sceneId](const SceneData &s) -> bool { return s.id == sceneId; });
    if (it == m_scenes.end()) {
        return;
    }
    if (type == UpdateLayerStateCommand::Lock) {
        if (value) {
            it->lockedLayers.insert(layer);
        } else {
            it->lockedLayers.remove(layer);
        }
    } else {
        if (value) {
            it->hiddenLayers.insert(layer);
        } else {
            it->hiddenLayers.remove(layer);
        }
    }
    emit clipsChanged();
    emit layerStateChanged(layer);
}

auto TimelineService::resolvedActiveClipsAt(int frame) const -> QList<ClipData *> {
    const SceneData *currentScenePtr = currentScene();
    if ((currentScenePtr == nullptr) || currentScenePtr->id != m_currentSceneId) {
        return {};
    }

    // 最適化: QSet::contains を回避するためビットセット（128レイヤー分）を作成
    std::bitset<128> hiddenMask;
    for (int layer : currentScenePtr->hiddenLayers) {
        if (static_cast<size_t>(layer) < hiddenMask.size()) {
            hiddenMask.set(layer);
        }
    }

    QList<ClipData *> result;
    result.reserve(currentScenePtr->clips.size());

    for (const auto &clip : currentScenePtr->clips) {
        // 高速ビットチェック
        if (clip.layer < 128 && hiddenMask.test(clip.layer)) {
            continue;
        }

        // クリップ種別のキャッシュ更新
        if (!clip.isSceneIdCached) {
            clip.isSceneObject = (clip.type == QStringLiteral("scene"));
            clip.isSceneIdCached = true;
        }

        // 通常クリップ
        if (!clip.isSceneObject) {
            if (frame >= clip.startFrame && frame < clip.startFrame + clip.durationFrames) {
                result.append(const_cast<ClipData *>(&clip));
            }
            continue;
        }

        // シーンオブジェクトの場合
        int parentLocal = frame - clip.startFrame;
        if (parentLocal < 0 || parentLocal >= clip.durationFrames) {
            continue;
        }

        // パラメータから sceneId / speed / offset を取得
        int targetSceneId = 0;
        double speed = 1.0;
        int offset = 0;
        if (!clip.effects.isEmpty()) {
            // 先頭エフェクト(Transform)の次、あるいはIDで検索すべきだけど
            // 確実にエフェクトリストからパラメータを取得
            for (auto *eff : std::as_const(clip.effects)) {
                if (eff->id() == QStringLiteral("scene")) {
                    QVariantMap p = eff->params();
                    targetSceneId = p.value(QStringLiteral("targetSceneId"), 0).toInt();
                    speed = p.value(QStringLiteral("speed"), 1.0).toDouble();
                    offset = p.value(QStringLiteral("offset"), 0).toInt();
                    break;
                }
            }
        }
        if (targetSceneId < 0) {
            continue;
        }

        int childLocal = static_cast<int>(parentLocal * speed) + offset;

        // 対象シーンを探す
        const SceneData *targetScene = nullptr;
        for (const auto &s : std::as_const(m_scenes)) {
            if (s.id == targetSceneId) {
                targetScene = &s;
                break;
            }
        }
        if (targetScene == nullptr) {
            continue;
        }

        // 対象シーン内のクリップを、親シーンの座標系に射影して追加
        for (const auto &child : targetScene->clips) {
            if (childLocal >= child.startFrame && childLocal < child.startFrame + child.durationFrames) {
                if (targetScene->hiddenLayers.contains(child.layer)) {
                    continue;
                }
                // グローバル時間で見た start は「親のstart + 子のstart」
                // （ここではClipData自体は書き換えず、判定だけに使う）
                result.append(const_cast<ClipData *>(&child));
            }
        }
    }

    return result;
}

auto TimelineService::findVacantFrame(int layer, int startFrame, int duration, int excludeClipId) const -> int { // NOLINT(bugprone-easily-swappable-parameters)
    QList<const ClipData *> layerClips;

    // バッチ移動中は明示的に指定された集合を使い、そうでない場合は選択情報を使う
    bool isBatchMode = !m_batchExcludes.isEmpty();
    bool isSelected = (m_selection != nullptr) && m_selection->isSelected(excludeClipId);
    QVariantList selectedIds = isSelected ? m_selection->selectedClipIds() : QVariantList();

    for (const auto &clip : clips()) {
        if (clip.id == excludeClipId) {
            continue;
        }

        if (isBatchMode) {
            if (m_batchExcludes.contains(clip.id)) {
                continue;
            }
        } else if (isSelected) {
            bool isPeer = false;
            for (const QVariant &v : std::as_const(selectedIds)) {
                if (v.toInt() == clip.id) {
                    isPeer = true;
                    break;
                }
            }
            if (isPeer) {
                continue;
            }
        }

        if (clip.layer == layer) {
            layerClips.append(&clip);
        }
    }

    std::ranges::sort(layerClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    int candidateStart = std::max(0, startFrame);
    for (const auto &clip : std::as_const(layerClips)) {
        int clipEnd = clip->startFrame + clip->durationFrames;
        int candidateEnd = candidateStart + duration;
        if (candidateStart < clipEnd && candidateEnd > clip->startFrame) {
            candidateStart = clipEnd;
        }
    }
    return candidateStart;
}

void TimelineService::setClipboard(const ClipData &clip) {
    m_clipboard.clear();
    m_clipboard.append(deepCopyClip(clip));
}

void TimelineService::setClipboard(const QList<ClipData> &clips) {
    m_clipboard.clear();
    for (const auto &clip : std::as_const(clips)) {
        m_clipboard.append(deepCopyClip(clip));
    }
}

void TimelineService::createSceneInternal(int sceneId, const QString &name) {
    SceneData newScene;
    newScene.id = sceneId;
    newScene.name = name;
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    newScene.width = settings.value(QStringLiteral("defaultProjectWidth"), 1920).toInt();
    newScene.height = settings.value(QStringLiteral("defaultProjectHeight"), 1080).toInt();
    newScene.fps = settings.value(QStringLiteral("defaultProjectFps"), 60.0).toDouble();
    m_scenes.append(newScene);
    emit scenesChanged();
    switchScene(newScene.id);
}

void TimelineService::removeSceneInternal(int sceneId) {
    if (sceneId == 0) {
        return;
    }
    auto it = std::ranges::find_if(m_scenes, [sceneId](const SceneData &s) -> bool { return s.id == sceneId; });
    if (it != m_scenes.end()) {
        for (auto &clip : it->clips) {
            for (auto *eff : std::as_const(clip.effects)) {
                eff->deleteLater();
            }
        }
        if (m_currentSceneId == sceneId) {
            switchScene(0);
        }
        m_scenes.erase(it);
        emit scenesChanged();
    }
}

void TimelineService::restoreSceneInternal(const SceneData &scene) {
    m_scenes.append(scene);
    emit scenesChanged();
}

void TimelineService::applySceneSettingsInternal(int sceneId, const SceneData &data) {
    for (auto &scene : m_scenes) {
        if (scene.id == sceneId) {
            scene.name = data.name;
            scene.width = data.width;
            scene.height = data.height;
            scene.fps = data.fps;
            scene.totalFrames = data.totalFrames;
            scene.gridMode = data.gridMode;
            scene.gridBpm = data.gridBpm;
            scene.gridOffset = data.gridOffset;
            scene.gridInterval = data.gridInterval;
            scene.gridSubdivision = data.gridSubdivision;
            scene.enableSnap = data.enableSnap;
            scene.magneticSnapRange = data.magneticSnapRange;
            emit scenesChanged();
            return;
        }
    }
}

void TimelineService::setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= clip->effects.size()) {
        return;
    }
    const auto *eff = clip->effects.value(effectIndex);

    bool wasExisting = false;
    QVariant oldValue;
    QVariantMap oldOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : std::as_const(track)) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("frame")).toInt() == frame) {
            wasExisting = true;
            oldValue = m.value(QStringLiteral("value"));
            oldOptions = m;
            break;
        }
    }
    m_undoStack->push(new SetKeyframeCommand(this, clipId, effectIndex, paramName, frame, value, options, oldValue, oldOptions, wasExisting));
}

void TimelineService::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) {
    const auto *clip = findClipById(clipId);
    if ((clip == nullptr) || effectIndex >= clip->effects.size()) {
        return;
    }
    const auto *eff = clip->effects.value(effectIndex);

    QVariant savedValue;
    QVariantMap savedOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : std::as_const(track)) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("frame")).toInt() == frame) {
            savedValue = m.value(QStringLiteral("value"));
            savedOptions = m;
            break;
        }
    }
    m_undoStack->push(new RemoveKeyframeCommand(this, clipId, effectIndex, paramName, frame, savedValue, savedOptions));
}

void TimelineService::setKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex < clip->effects.size()) {
        clip->effects.value(effectIndex)->setKeyframe(paramName, frame, value, options);
    }
}

void TimelineService::removeKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame) { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *clip = findClipById(clipId);
    if ((clip != nullptr) && effectIndex < clip->effects.size()) {
        clip->effects.value(effectIndex)->removeKeyframe(paramName, frame);
    }
}

} // namespace Rina::UI