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
void UpdateEffectParamCommand::undo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_oldValue, true); }
void UpdateEffectParamCommand::redo() { m_service->updateEffectParamInternal(m_clipId, m_effectIndex, m_paramName, m_newValue, true); }
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

DeleteClipCommand::DeleteClipCommand(TimelineService *service, int clipId, const QString &clipName) : m_service(service), m_clipId(clipId) {
    const auto *clip = service->findClipById(clipId);
    if (clip)
        m_snapshot = service->deepCopyClip(*clip);
    setText(QString("クリップ削除: %1").arg(clipName));
}
void DeleteClipCommand::redo() { m_service->deleteClipInternal(m_clipId); }
void DeleteClipCommand::undo() {
    m_snapshot.id = m_clipId;
    m_service->addClipDirectInternal(m_snapshot);
}

CutClipCommand::CutClipCommand(TimelineService *service, int clipId, const QString &clipName) : m_service(service), m_clipId(clipId) {
    const auto *clip = service->findClipById(clipId);
    if (clip)
        m_snapshot = service->deepCopyClip(*clip);
    setText(QString("切り取り: %1").arg(clipName));
}
void CutClipCommand::redo() {
    m_service->setClipboard(m_snapshot);
    m_service->deleteClipInternal(m_clipId);
}
void CutClipCommand::undo() {
    m_snapshot.id = m_clipId;
    m_service->addClipDirectInternal(m_snapshot);
}

PasteClipCommand::PasteClipCommand(TimelineService *service, int newClipId, const ClipData &clipData) : m_service(service), m_newClipId(newClipId), m_clipData(clipData) { setText(QString("貼り付け: %1").arg(clipData.type)); }
void PasteClipCommand::redo() {
    ClipData c = m_clipData;
    c.id = m_newClipId;
    m_service->addClipDirectInternal(c);
}
void PasteClipCommand::undo() { m_service->deleteClipInternal(m_newClipId); }

SetKeyframeCommand::SetKeyframeCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &newValue, const QVariantMap &options, const QVariant &oldValue, const QVariantMap &oldOptions,
                                       bool wasExisting)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_frame(frame), m_paramName(paramName), m_newValue(newValue), m_oldValue(oldValue), m_newOptions(options), m_oldOptions(oldOptions), m_wasExisting(wasExisting) {
    setText(QString("キーフレーム設定: %1").arg(paramName));
}
void SetKeyframeCommand::redo() { m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_newValue, m_newOptions); }
void SetKeyframeCommand::undo() {
    if (m_wasExisting)
        m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_oldValue, m_oldOptions);
    else
        m_service->removeKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame);
}
int SetKeyframeCommand::id() const { return 1002; }
bool SetKeyframeCommand::mergeWith(const QUndoCommand *other) {
    if (other->id() != id())
        return false;
    const auto *cmd = static_cast<const SetKeyframeCommand *>(other);
    if (cmd->m_clipId != m_clipId || cmd->m_effectIndex != m_effectIndex || cmd->m_paramName != m_paramName || cmd->m_frame != m_frame)
        return false;
    m_newValue = cmd->m_newValue;
    m_newOptions = cmd->m_newOptions;
    return true;
}

RemoveKeyframeCommand::RemoveKeyframeCommand(TimelineService *service, int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &savedValue, const QVariantMap &savedOptions)
    : m_service(service), m_clipId(clipId), m_effectIndex(effectIndex), m_frame(frame), m_paramName(paramName), m_savedValue(savedValue), m_savedOptions(savedOptions) {
    setText(QString("キーフレーム削除: %1 [%2]").arg(paramName).arg(frame));
}
void RemoveKeyframeCommand::redo() { m_service->removeKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame); }
void RemoveKeyframeCommand::undo() { m_service->setKeyframeInternal(m_clipId, m_effectIndex, m_paramName, m_frame, m_savedValue, m_savedOptions); }

AddSceneCommand::AddSceneCommand(TimelineService *service, int sceneId, const QString &name) : m_service(service), m_sceneId(sceneId), m_name(name) { setText(QString("シーン追加: %1").arg(name)); }
void AddSceneCommand::redo() { m_service->createSceneInternal(m_sceneId, m_name); }
void AddSceneCommand::undo() { m_service->removeSceneInternal(m_sceneId); }

RemoveSceneCommand::RemoveSceneCommand(TimelineService *service, int sceneId, const QString &name) : m_service(service), m_sceneId(sceneId) {
    for (const auto &s : service->getAllScenes())
        if (s.id == sceneId) {
            m_snapshot = s;
            break;
        }
    setText(QString("シーン削除: %1").arg(name));
}
void RemoveSceneCommand::redo() { m_service->removeSceneInternal(m_sceneId); }
void RemoveSceneCommand::undo() { m_service->restoreSceneInternal(m_snapshot); }

UpdateSceneSettingsCommand::UpdateSceneSettingsCommand(TimelineService *service, int sceneId, const SceneData &oldData, const SceneData &newData) : m_service(service), m_sceneId(sceneId), m_oldData(oldData), m_newData(newData) {
    setText(QString("シーン設定変更: %1").arg(newData.name));
}
void UpdateSceneSettingsCommand::redo() { m_service->applySceneSettingsInternal(m_sceneId, m_newData); }
void UpdateSceneSettingsCommand::undo() { m_service->applySceneSettingsInternal(m_sceneId, m_oldData); }

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

void TimelineService::applyClipBatchMove(const QVariantList &moves) {
    if (moves.isEmpty())
        return;

    m_batchExcludes.clear();
    for (const QVariant &vMove : moves) {
        m_batchExcludes.insert(vMove.toMap().value("id").toInt());
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

    for (const QVariant &vMove : moves) {
        QVariantMap move = vMove.toMap();
        int id = move.value("id").toInt();
        const auto *clip = findClipById(id);
        if (clip) {
            pending.push_back(PendingOp{id, clip->layer, clip->startFrame, move.value("layer").toInt(), move.value("startFrame").toInt(), move.value("duration").toInt(), clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
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

        for (const auto &op : pending) {
            int testStart = op.targetStart + maxPush;
            int safeStart = findVacantFrame(op.targetLayer, testStart, op.duration, op.id);
            if (safeStart > testStart) {
                int push = safeStart - testStart;
                if (push > currentPush) {
                    currentPush = push;
                }
            }
        }

        if (currentPush > 0) {
            maxPush += currentPush;
            needsPush = true;
        }
        loopCount++;
    }

    m_undoStack->beginMacro(QString("複数クリップ絶対移動: %1").arg(pending.size()));
    for (const auto &op : pending) {
        int finalStart = op.targetStart + maxPush;
        m_undoStack->push(new MoveClipCommand(this, op.id, op.oldLayer, op.oldStart, op.duration, op.targetLayer, finalStart, op.duration, op.name));
    }
    m_undoStack->endMacro();
    m_batchExcludes.clear();
}

void TimelineService::moveSelectedClips(int deltaLayer, int deltaFrame) {
    if (!m_selection || (deltaLayer == 0 && deltaFrame == 0))
        return;

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty())
        return;

    struct PendingOp {
        int id;
        int oldLayer;
        int oldStart;
        int duration;
        QString name;
    };

    QVector<PendingOp> pending;
    pending.reserve(ids.size());

    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (!clip)
            continue;

        pending.push_back(PendingOp{id, clip->layer, clip->startFrame, clip->durationFrames, clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
    }

    if (deltaFrame > 0 || (deltaFrame == 0 && deltaLayer > 0)) {
        std::sort(pending.begin(), pending.end(), [](const PendingOp &a, const PendingOp &b) {
            if (a.oldStart != b.oldStart)
                return a.oldStart > b.oldStart;
            return a.oldLayer > b.oldLayer;
        });
    } else {
        std::sort(pending.begin(), pending.end(), [](const PendingOp &a, const PendingOp &b) {
            if (a.oldStart != b.oldStart)
                return a.oldStart < b.oldStart;
            return a.oldLayer < b.oldLayer;
        });
    }

    m_undoStack->beginMacro(QString("複数クリップ移動: %1").arg(pending.size()));
    for (const PendingOp &clip : pending) {
        const int newLayer = std::max(0, clip.oldLayer + deltaLayer);
        const int newStart = std::max(0, clip.oldStart + deltaFrame);
        m_undoStack->push(new MoveClipCommand(this, clip.id, clip.oldLayer, clip.oldStart, clip.duration, newLayer, newStart, clip.duration, clip.name));
    }
    m_undoStack->endMacro();
}

void TimelineService::resizeSelectedClips(int deltaStartFrame, int deltaDuration) {
    if (!m_selection || (deltaStartFrame == 0 && deltaDuration == 0))
        return;

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty())
        return;

    struct PendingOp {
        int id;
        int oldLayer;
        int oldStart;
        int duration;
        QString name;
    };

    QVector<PendingOp> pending;
    pending.reserve(ids.size());

    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (!clip)
            continue;

        pending.push_back(PendingOp{id, clip->layer, clip->startFrame, clip->durationFrames, clip->effects.isEmpty() ? clip->type : clip->effects.first()->name()});
    }

    // Resize left side -> deltaStartFrame != 0. If deltaStartFrame > 0, left edge moves right.
    // Resize right side -> deltaStartFrame == 0, deltaDuration != 0.
    // In any case, order matters if they push each other.
    if (deltaStartFrame > 0 || deltaDuration > 0) {
        std::sort(pending.begin(), pending.end(), [](const PendingOp &a, const PendingOp &b) {
            if (a.oldStart != b.oldStart)
                return a.oldStart > b.oldStart;
            return a.oldLayer > b.oldLayer;
        });
    } else {
        std::sort(pending.begin(), pending.end(), [](const PendingOp &a, const PendingOp &b) {
            if (a.oldStart != b.oldStart)
                return a.oldStart < b.oldStart;
            return a.oldLayer < b.oldLayer;
        });
    }

    m_undoStack->beginMacro(QString("複数クリップ変形: %1").arg(pending.size()));
    for (const PendingOp &clip : pending) {
        const int newStart = std::max(0, clip.oldStart + deltaStartFrame);
        const int newDuration = std::max(1, clip.duration + deltaDuration);
        m_undoStack->push(new MoveClipCommand(this, clip.id, clip.oldLayer, clip.oldStart, clip.duration, clip.oldLayer, newStart, newDuration, clip.name));
    }
    m_undoStack->endMacro();
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

QPoint TimelineService::resolveDragPosition(int clipId, int targetLayer, int proposedStartFrame, const QVariantList &batchIds) {
    const auto *movingClip = findClipById(clipId);
    if (!movingClip) {
        return QPoint(proposedStartFrame, targetLayer);
    }

    int deltaLayer = targetLayer - movingClip->layer;
    int deltaFrame = proposedStartFrame - movingClip->startFrame;

    QSet<int> movingIds;
    if (!batchIds.isEmpty()) {
        for (const QVariant &v : batchIds) {
            movingIds.insert(v.toInt());
        }
    } else if (m_selection && m_selection->isSelected(clipId)) {
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

        for (int id : movingIds) {
            const auto *c = findClipById(id);
            if (!c)
                continue;

            int tLayer = c->layer + deltaLayer;
            if (tLayer < 0)
                tLayer = 0;
            if (tLayer > 127)
                tLayer = 127;

            int testStart = c->startFrame + deltaFrame + maxPush;
            if (testStart < 0)
                testStart = 0;

            m_batchExcludes = movingIds;
            int safeStart = findVacantFrame(tLayer, testStart, c->durationFrames, id);
            m_batchExcludes = backupExcludes;

            if (safeStart > testStart) {
                int push = safeStart - testStart;
                if (push > currentPush) {
                    currentPush = push;
                }
            }
        }

        if (currentPush > 0) {
            maxPush += currentPush;
            needsPush = true;
        }
        loopCount++;
    }

    int finalFrame = proposedStartFrame + maxPush;
    return QPoint(finalFrame, targetLayer);
}

void TimelineService::updateClipInternal(int id, int layer, int startFrame, int duration, bool notifySelection) {
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
                if (notifySelection && m_selection->selectedClipId() == id) {
                    QVariantMap data = m_selection->selectedClipData();
                    data["layer"] = layer;
                    data["startFrame"] = startFrame;
                    data["durationFrames"] = duration;
                    // refreshSelectionData は古い。直接 replaceSelection を呼ぶ
                    // m_selection->refreshSelectionData(id, data);
                    m_selection->replaceSelection(m_selection->selectedClipIds(), id, data);
                }
            }
            break;
        }
    }
}

void TimelineService::selectClip(int id) { applySelectionIds(QVariantList{id}); }

void TimelineService::applySelectionIds(const QVariantList &ids) {
    int primaryId = -1;
    QVariantMap primaryData;

    // Find a valid primary clip from the given ids
    for (const QVariant &vId : ids) {
        int id = vId.toInt();
        const auto *clip = findClipById(id);
        if (clip) {
            primaryId = clip->id;
            for (auto *eff : clip->effects) {
                QVariantMap params = eff->params();
                for (auto it = params.begin(); it != params.end(); ++it)
                    primaryData.insert(it.key(), it.value());
            }
            primaryData["startFrame"] = clip->startFrame;
            primaryData["durationFrames"] = clip->durationFrames;
            primaryData["layer"] = clip->layer;
            primaryData["type"] = clip->type;
            break; // take the first valid clip as primary
        }
    }

    m_selection->replaceSelection(ids, primaryId, primaryData);
}

void TimelineService::selectClipsInRange(int frameA, int frameB, int layerA, int layerB, bool additive) {
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
        if (!frameOverlap || !layerMatch)
            continue;

        ids.append(clip.id);

        if (primaryId == -1) {
            primaryId = clip.id;
            for (auto *eff : clip.effects) {
                QVariantMap params = eff->params();
                for (auto it = params.begin(); it != params.end(); ++it)
                    primaryData.insert(it.key(), it.value());
            }
            primaryData["startFrame"] = clip.startFrame;
            primaryData["durationFrames"] = clip.durationFrames;
            primaryData["layer"] = clip.layer;
            primaryData["type"] = clip.type;
        }
    }

    if (additive) {
        QVariantList merged = m_selection->selectedClipIds();
        for (const QVariant &id : ids) {
            if (!merged.contains(id))
                merged.append(id);
        }
        m_selection->replaceSelection(merged, primaryId, primaryData);
        return;
    }

    m_selection->replaceSelection(ids, primaryId, primaryData);
}

void TimelineService::deleteClip(int clipId) {
    const auto *clip = findClipById(clipId);
    if (!clip)
        return;
    QString name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name();
    m_undoStack->push(new DeleteClipCommand(this, clipId, name));
}

void TimelineService::deleteClipInternal(int clipId) {
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

void TimelineService::previewEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    const auto *clip = findClipById(clipId);
    if (!clip || effectIndex < 0 || effectIndex >= clip->effects.size())
        return;

    const QVariant oldValue = clip->effects[effectIndex]->params().value(paramName);
    if (oldValue.isValid() && oldValue == value)
        return;

    updateEffectParamInternal(clipId, effectIndex, paramName, value, true);
}

void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    QVariant oldValue;
    const auto *clip = findClipById(clipId);
    if (!clip || effectIndex >= clip->effects.size())
        return;

    const QVariant currentValue = clip->effects[effectIndex]->params().value(paramName);
    if (currentValue.isValid() && currentValue == value)
        return;

    const auto *eff = clip->effects[effectIndex];
    oldValue = eff->params().value(paramName);

    m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue, eff->name()));
}

void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value, bool notifySelection) {
    for (auto &clip : clipsMutable()) {
        if (clip.id == clipId) {
            if (effectIndex >= 0 && effectIndex < clip.effects.size()) {
                clip.effects[effectIndex]->setParam(paramName, value);
                emit effectParamChanged(clipId, effectIndex, paramName, value);
                if (notifySelection && m_selection->selectedClipId() == clipId)
                    m_selection->patchSelectedClipParam(paramName, value);
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
    auto &currentClips = clipsMutable();
    auto it = std::find_if(currentClips.begin(), currentClips.end(), [clipId](const ClipData &c) { return c.id == clipId; });
    if (it == currentClips.end())
        return;

    m_clipboard.clear();
    m_clipboard.append(deepCopyClip(*it));
}

void TimelineService::copySelectedClips() {
    QList<ClipData> copied;
    const QVariantList ids = m_selection ? m_selection->selectedClipIds() : QVariantList();
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (clip)
            copied.append(deepCopyClip(*clip));
    }
    if (!copied.isEmpty())
        setClipboard(copied);
}

void TimelineService::cutClip(int clipId) {
    const auto *clip = findClipById(clipId);
    if (!clip)
        return;
    QString name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name();
    m_undoStack->push(new CutClipCommand(this, clipId, name));
}

void TimelineService::cutSelectedClips() {
    if (!m_selection)
        return;

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty())
        return;

    QList<ClipData> copied;
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (clip)
            copied.append(deepCopyClip(*clip));
    }
    if (copied.isEmpty())
        return;

    setClipboard(copied);

    m_undoStack->beginMacro(QString("複数クリップ切り取り: %1").arg(copied.size()));
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (!clip)
            continue;
        QString name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name();
        m_undoStack->push(new DeleteClipCommand(this, id, name));
    }
    m_undoStack->endMacro();
    m_selection->clearSelection();
}

void TimelineService::deleteSelectedClips() {
    if (!m_selection)
        return;

    const QVariantList ids = m_selection->selectedClipIds();
    if (ids.isEmpty())
        return;

    m_undoStack->beginMacro(QString("複数クリップ削除: %1").arg(ids.size()));
    for (const QVariant &value : ids) {
        const int id = value.toInt();
        const auto *clip = findClipById(id);
        if (!clip)
            continue;
        QString name = clip->effects.isEmpty() ? clip->type : clip->effects.first()->name();
        m_undoStack->push(new DeleteClipCommand(this, id, name));
    }
    m_undoStack->endMacro();
    m_selection->clearSelection();
}

void TimelineService::pasteClip(int frame, int layer) {
    if (m_clipboard.isEmpty())
        return;

    if (frame < 0)
        frame = 0;
    if (layer < 0)
        layer = 0;

    auto overlaps = [](int s1, int d1, int s2, int d2) { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    auto &currentClips = clipsMutable();

    int baseFrame = m_clipboard.first().startFrame;
    int baseLayer = m_clipboard.first().layer;
    for (const auto &clip : m_clipboard) {
        baseFrame = std::min(baseFrame, clip.startFrame);
        baseLayer = std::min(baseLayer, clip.layer);
    }

    QList<ClipData> pending;
    for (const auto &src : m_clipboard) {
        ClipData newClip = deepCopyClip(src);
        newClip.startFrame = frame + (src.startFrame - baseFrame);
        newClip.layer = std::max(0, layer + (src.layer - baseLayer));

        for (const auto &c : currentClips) {
            if (c.layer == newClip.layer && overlaps(newClip.startFrame, newClip.durationFrames, c.startFrame, c.durationFrames)) {
                qWarning() << "クリップ貼り付けを拒否: レイヤー" << newClip.layer << "の" << newClip.startFrame << "フレームで衝突が発生";
                return;
            }
        }
        for (const auto &c : pending) {
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

    m_undoStack->beginMacro(QString("複数クリップ貼り付け: %1").arg(pending.size()));
    for (const auto &clip : pending) {
        int newId = m_nextClipId++;
        m_undoStack->push(new PasteClipCommand(this, newId, clip));
    }
    m_undoStack->endMacro();
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
        map["gridMode"] = scene.gridMode;
        map["gridBpm"] = scene.gridBpm;
        map["gridOffset"] = scene.gridOffset;
        map["gridInterval"] = scene.gridInterval;
        map["gridSubdivision"] = scene.gridSubdivision;
        map["enableSnap"] = scene.enableSnap;
        map["magneticSnapRange"] = scene.magneticSnapRange;
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
    int id = m_nextSceneId++;
    m_undoStack->push(new AddSceneCommand(this, id, name));
}

void TimelineService::removeScene(int sceneId) {
    for (const auto &s : getAllScenes())
        if (s.id == sceneId) {
            m_undoStack->push(new RemoveSceneCommand(this, sceneId, s.name));
            return;
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

void TimelineService::updateSceneSettings(int sceneId, const QString &name, int width, int height, double fps, int totalFrames, const QString &gridMode, double gridBpm, double gridOffset, int gridInterval, int gridSubdivision, bool enableSnap,
                                          int magneticSnapRange) {
    SceneData newData, oldData;
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

    // バッチ移動中は明示的に指定された集合を使い、そうでない場合は選択情報を使う
    bool isBatchMode = !m_batchExcludes.isEmpty();
    bool isSelected = m_selection && m_selection->isSelected(excludeClipId);
    QVariantList selectedIds = isSelected ? m_selection->selectedClipIds() : QVariantList();

    for (const auto &clip : clips()) {
        if (clip.id == excludeClipId)
            continue;

        if (isBatchMode) {
            if (m_batchExcludes.contains(clip.id))
                continue;
        } else if (isSelected) {
            bool isPeer = false;
            for (const QVariant &v : selectedIds) {
                if (v.toInt() == clip.id) {
                    isPeer = true;
                    break;
                }
            }
            if (isPeer)
                continue;
        }

        if (clip.layer == layer) {
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

void TimelineService::setClipboard(const ClipData &clip) {
    m_clipboard.clear();
    m_clipboard.append(deepCopyClip(clip));
}

void TimelineService::setClipboard(const QList<ClipData> &clips) {
    m_clipboard.clear();
    for (const auto &clip : clips)
        m_clipboard.append(deepCopyClip(clip));
}

void TimelineService::createSceneInternal(int sceneId, const QString &name) {
    SceneData newScene;
    newScene.id = sceneId;
    newScene.name = name;
    const auto &settings = Rina::Core::SettingsManager::instance().settings();
    newScene.width = settings.value("defaultProjectWidth", 1920).toInt();
    newScene.height = settings.value("defaultProjectHeight", 1080).toInt();
    newScene.fps = settings.value("defaultProjectFps", 60.0).toDouble();
    m_scenes.append(newScene);
    emit scenesChanged();
    switchScene(newScene.id);
}

void TimelineService::removeSceneInternal(int sceneId) {
    if (sceneId == 0)
        return;
    auto it = std::find_if(m_scenes.begin(), m_scenes.end(), [sceneId](const SceneData &s) { return s.id == sceneId; });
    if (it != m_scenes.end()) {
        for (auto &clip : it->clips)
            for (auto *eff : clip.effects)
                eff->deleteLater();
        if (m_currentSceneId == sceneId)
            switchScene(0);
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
    if (!clip || effectIndex >= clip->effects.size())
        return;
    const auto *eff = clip->effects[effectIndex];

    bool wasExisting = false;
    QVariant oldValue;
    QVariantMap oldOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : track) {
        const auto m = v.toMap();
        if (m.value("frame").toInt() == frame) {
            wasExisting = true;
            oldValue = m.value("value");
            oldOptions = m;
            break;
        }
    }
    m_undoStack->push(new SetKeyframeCommand(this, clipId, effectIndex, paramName, frame, value, options, oldValue, oldOptions, wasExisting));
}

void TimelineService::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) {
    const auto *clip = findClipById(clipId);
    if (!clip || effectIndex >= clip->effects.size())
        return;
    const auto *eff = clip->effects[effectIndex];

    QVariant savedValue;
    QVariantMap savedOptions;
    const auto track = eff->keyframeTracks().value(paramName).toList();
    for (const auto &v : track) {
        const auto m = v.toMap();
        if (m.value("frame").toInt() == frame) {
            savedValue = m.value("value");
            savedOptions = m;
            break;
        }
    }
    m_undoStack->push(new RemoveKeyframeCommand(this, clipId, effectIndex, paramName, frame, savedValue, savedOptions));
}

void TimelineService::setKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    const auto *clip = findClipById(clipId);
    if (clip && effectIndex < clip->effects.size())
        clip->effects[effectIndex]->setKeyframe(paramName, frame, value, options);
}

void TimelineService::removeKeyframeInternal(int clipId, int effectIndex, const QString &paramName, int frame) {
    const auto *clip = findClipById(clipId);
    if (clip && effectIndex < clip->effects.size())
        clip->effects[effectIndex]->removeKeyframe(paramName, frame);
}

void TimelineService::setEffectEnabled(int clipId, int effectIndex, bool enabled) {
    auto *clip = findClipById(clipId);
    if (clip && effectIndex >= 0 && effectIndex < clip->effects.size()) {
        clip->effects[effectIndex]->setEnabled(enabled);
        emit clipEffectsChanged(clipId);
        emit clipsChanged();
    }
}

void TimelineService::reorderEffects(int clipId, int fromIndex, int toIndex) {
    auto *clip = findClipById(clipId);
    if (!clip)
        return;
    if (fromIndex < 0 || fromIndex >= clip->effects.size() || toIndex < 0 || toIndex >= clip->effects.size() || fromIndex == toIndex) {
        return;
    }

    clip->effects.move(fromIndex, toIndex);
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

} // namespace Rina::UI