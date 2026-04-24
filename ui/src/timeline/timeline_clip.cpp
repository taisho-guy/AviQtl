#include "clip_effect_system.hpp"
#include "clip_lifecycle_system.hpp"
#include "clip_snapshot.hpp"
#include "commands.hpp"
#include "effect_registry.hpp"
#include "engine/timeline/clip_effect_system.hpp"
#include "engine/timeline/clip_lifecycle_system.hpp"
#include "engine/timeline/clip_transform_system.hpp"
#include "engine/timeline/ecs.hpp"
#include "selection_service.hpp"
#include "settings_manager.hpp"
#include "timeline_service.hpp"
#include <QDebug>
#include <QPoint>
#include <algorithm>
#include <utility>

namespace Rina::UI {

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
    // 衝突チェック: ECS スナップショットを正本として走査
    const auto *createSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const auto &ct : createSnap->transforms) {
        if (ct.layer == layer && overlaps(startFrame, defaultDuration, ct.startFrame, ct.durationFrames)) {
            qWarning() << "クリップ作成を拒否: レイヤー" << layer << "の" << startFrame << "フレームで衝突が発生";
            return;
        }
    }

    // 全書き込みを同じ editState に対して行い、最後に1回だけ commit する
    // 途中で commit すると editState が新バッファに進み effectStacks[clipId] が
    // 次バッファに伝搬しないため addEffect が find(clipId)==nullptr で空振りする
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    auto &state = ecs.editState();

    Rina::Engine::Timeline::ClipLifecycleSystem::createClip(state, clipId, type, layer, startFrame, defaultDuration);
    ecs.updateMetadata(clipId, type, QStringLiteral(""), type, QStringLiteral(""));

    // addEffectInternal は内部で commit を挟むため使用不可
    // 同じ state に直接書き込むラムダで代替する
    const auto addEffectToState = [&](const QString &effectId) {
        const auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
        if (meta.id.isEmpty())
            return;
        Rina::UI::EffectData data;
        data.id = meta.id;
        data.name = meta.name;
        data.kind = meta.kind;
        data.categories = meta.categories;
        data.qmlSource = meta.qmlSource;
        data.uiDefinition = meta.uiDefinition;
        data.enabled = true;
        data.params = meta.defaultParams;
        Rina::Engine::Timeline::ClipEffectSystem::addEffect(state, clipId, data);
    };

    addEffectToState(QStringLiteral("transform"));
    addEffectToState(type);

    // 全書き込み完了後に1回だけ commit して activeIndex に公開する
    ecs.commit();

    if (emitSignal) {
        emit clipsChanged();
        emit clipEffectsChanged(clipId);
        emit clipCreated(clipId, layer, startFrame, defaultDuration, type);
    }
}

void TimelineService::addClipsDirectInternal(const QList<ClipData> &clips) {
    for (const auto &clip : std::as_const(clips)) {
        addClipDirectInternal(clip, false);
    }
    emit clipsChanged();
}

void TimelineService::updateClip(int id, int layer, int startFrame, int duration) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *t = snap->transforms.find(id);
    const auto *m = snap->metadataStates.find(id);
    if (t == nullptr || m == nullptr) {
        return;
    }
    const auto *fx = snap->effectStacks.find(id);
    QString clipName = m->type;
    if (fx != nullptr && !fx->effects.isEmpty()) {
        clipName = fx->effects.first().name;
    }
    m_undoStack->push(new MoveClipCommand(this, id, t->layer, t->startFrame, t->durationFrames, layer, startFrame, duration, clipName));
}

void TimelineService::applyClipBatchMove(const QVariantList &moves) {
    if (moves.isEmpty()) {
        return;
    }

    m_batchExcludes.clear();
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
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
        const auto *t2 = snap->transforms.find(id);
        const auto *m2 = snap->metadataStates.find(id);
        if (t2 != nullptr && m2 != nullptr) {
            const auto *fx2 = snap->effectStacks.find(id);
            pending.push_back(PendingOp{.id = id,
                                        .oldLayer = t2->layer,
                                        .oldStart = t2->startFrame,
                                        .targetLayer = move.value(QStringLiteral("layer")).toInt(),
                                        .targetStart = move.value(QStringLiteral("startFrame")).toInt(),
                                        .duration = move.value(QStringLiteral("duration")).toInt(),
                                        .name = (fx2 == nullptr || fx2->effects.isEmpty()) ? m2->type : fx2->effects.first().name});
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

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        const auto *tR = snap->transforms.find(id);
        const auto *mR = snap->metadataStates.find(id);
        if (tR == nullptr || mR == nullptr) {
            continue;
        }
        const auto *fxR = snap->effectStacks.find(id);
        pending.push_back(PendingOp{.id = id, .oldLayer = tR->layer, .oldStart = tR->startFrame, .duration = tR->durationFrames, .name = (fxR == nullptr || fxR->effects.isEmpty()) ? mR->type : fxR->effects.first().name});
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

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    QVector<PendingOp> pending;
    pending.reserve(ids.size());

    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        const auto *tR = snap->transforms.find(id);
        const auto *mR = snap->metadataStates.find(id);
        if (tR == nullptr || mR == nullptr) {
            continue;
        }
        const auto *fxR = snap->effectStacks.find(id);
        pending.push_back(PendingOp{.id = id, .oldLayer = tR->layer, .oldStart = tR->startFrame, .duration = tR->durationFrames, .name = (fxR == nullptr || fxR->effects.isEmpty()) ? mR->type : fxR->effects.first().name});
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
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *movingT = snap->transforms.find(clipId);
    if (movingT == nullptr) {
        return proposedStartFrame; // 対象クリップが見つからなければ何もしない
    }
    const int clipDuration = movingT->durationFrames;

    // 1. 対象レイヤーのクリップを抽出 (現在のシーンのみ)
    QList<const Rina::Engine::Timeline::TransformComponent *> layerClips;
    for (const auto &ct : snap->transforms) {
        if (ct.layer == targetLayer && ct.clipId != clipId) {
            layerClips.append(&ct);
        }
    }

    int finalStart = std::max(0, proposedStartFrame);

    // 2. レイヤーに他のクリップがなければ、0未満にならないように補正してそのまま配置
    if (layerClips.isEmpty()) {
        updateClipInternal(clipId, targetLayer, finalStart, clipDuration);
        return finalStart;
    }

    // 3. startFrame昇順ソート
    std::ranges::sort(layerClips, [](const auto *a, const auto *b) -> bool { return a->startFrame < b->startFrame; });

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
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *movingT = snap->transforms.find(clipId);
    if (movingT == nullptr) {
        return {proposedStartFrame, targetLayer};
    }

    int deltaLayer = targetLayer - movingT->layer;
    int deltaFrame = proposedStartFrame - movingT->startFrame;

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
            const auto *c = snap->transforms.find(id);
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
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *existingT = snap->transforms.find(id);
    if (existingT == nullptr) {
        return;
    }

    // 移動元または移動先がロックされている場合は拒否
    if (isLayerLocked(layer) || isLayerLocked(existingT->layer)) {
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

    // m_scenes ループを廃止: ECS を正本として更新し、選択キャッシュのみ同期する
    {
        const auto *updSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
        const auto *updTr = updSnap->transforms.find(id);
        if (updTr != nullptr) {
            if (updTr->layer != layer || updTr->startFrame != startFrame || updTr->durationFrames != duration) {
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
        }
    }

    // ECS の TransformComponent を更新（フェーズ2: ECS を正本として同時更新する）
    // safeStartFrame が衝突回避済みの値なので ECS にもその値を書く
    Rina::Engine::Timeline::ECS::instance().updateClipState(id, layer, startFrame, duration);
    Rina::Engine::Timeline::ECS::instance().commit();
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
        const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
        const auto *tS = snap->transforms.find(id);
        const auto *mS = snap->metadataStates.find(id);
        const auto *fxS = snap->effectStacks.find(id);
        if (tS != nullptr && mS != nullptr) {
            primaryId = id;
            if (fxS != nullptr) {
                for (const auto &eff : std::as_const(fxS->effects)) {
                    for (auto it = eff.params.begin(); it != eff.params.end(); ++it) {
                        primaryData.insert(it.key(), it.value());
                    }
                }
            }
            primaryData.insert(QStringLiteral("startFrame"), tS->startFrame);
            primaryData.insert(QStringLiteral("durationFrames"), tS->durationFrames);
            primaryData.insert(QStringLiteral("layer"), tS->layer);
            primaryData.insert(QStringLiteral("type"), mS->type);
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

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const auto &ct : snap->transforms) {
        const auto *mSel = snap->metadataStates.find(ct.clipId);
        if (mSel == nullptr) {
            continue;
        }
        const int clipEnd = ct.startFrame + ct.durationFrames;
        const bool frameOverlap = ct.startFrame < maxFrame && minFrame < clipEnd;
        const bool layerMatch = ct.layer >= minLayer && ct.layer <= maxLayer;
        if (!frameOverlap || !layerMatch) {
            continue;
        }

        ids.append(ct.clipId);

        if (primaryId == -1) {
            primaryId = ct.clipId;
            const auto *fxSel = snap->effectStacks.find(ct.clipId);
            if (fxSel != nullptr) {
                for (const auto &eff : std::as_const(fxSel->effects)) {
                    for (auto it = eff.params.begin(); it != eff.params.end(); ++it) {
                        primaryData.insert(it.key(), it.value());
                    }
                }
            }
            primaryData.insert(QStringLiteral("startFrame"), ct.startFrame);
            primaryData.insert(QStringLiteral("durationFrames"), ct.durationFrames);
            primaryData.insert(QStringLiteral("layer"), ct.layer);
            primaryData.insert(QStringLiteral("type"), mSel->type);
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
    // m_scenes ループを廃止: ECS を正本として削除する
    auto &delEcs = Rina::Engine::Timeline::ECS::instance();
    if (delEcs.getSnapshot()->transforms.find(clipId) != nullptr) {
        Rina::Engine::Timeline::ClipLifecycleSystem::destroyClip(delEcs.editState(), clipId);
        delEcs.commit();
        if (emitSignal) {
            emit clipsChanged();
        }
    }
}

void TimelineService::addClipDirectInternal(const ClipData &clip, bool emitSignal) {
    // m_scenes.append 廃止: ECS へ直接復元する
    auto &addEcs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipLifecycleSystem::restoreClipFromDTO(addEcs.editState(), clip);
    addEcs.commit();
    if (emitSignal) {
        emit clipsChanged();
        emit clipCreated(clip.id, clip.layer, clip.startFrame, clip.durationFrames, clip.type);
    }
}
void TimelineService::restoreClipFromSnapshotInternal(const Rina::UI::ClipSnapshot &snap, bool emitSignal) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipLifecycleSystem::restoreClipFromSnapshot(ecs.editState(), snap);
    ecs.commit();
    if (emitSignal) {
        emit clipsChanged();
        emit clipCreated(snap.transform.clipId, snap.transform.layer, snap.transform.startFrame, snap.transform.durationFrames, snap.metadata.type);
    }
}

void TimelineService::restoreClipsFromSnapshotInternal(const QList<Rina::UI::ClipSnapshot> &snaps) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    for (const auto &snap : snaps)
        Rina::Engine::Timeline::ClipLifecycleSystem::restoreClipFromSnapshot(ecs.editState(), snap);
    ecs.commit();
    emit clipsChanged();
}

void TimelineService::setClipboardFromSnapshot(const Rina::UI::ClipSnapshot &snap) {
    m_clipboardSnapshots.clear();
    m_clipboardSnapshots.append(snap);
}

void TimelineService::copyClip(int clipId) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    Rina::UI::ClipSnapshot cs;
    if (const auto *t = snap->transforms.find(clipId)) {
        cs.transform = *t;
    }
    if (const auto *m = snap->metadataStates.find(clipId)) {
        cs.metadata = *m;
    }
    if (const auto *fx = snap->effectStacks.find(clipId)) {
        cs.effectStack = *fx;
    }
    if (const auto *au = snap->audioStacks.find(clipId)) {
        cs.audioStack = *au;
    }
    if (cs.isValid()) {
        m_clipboardSnapshots.clear();
        m_clipboardSnapshots.append(cs);
    }
}

void TimelineService::copySelectedClips() {
    const QVariantList ids = m_selection->selectedClipIds();
    QList<Rina::UI::ClipSnapshot> copied;
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const QVariant &value : std::as_const(ids)) {
        const int id = value.toInt();
        Rina::UI::ClipSnapshot cs;
        if (const auto *t = snap->transforms.find(id)) {
            cs.transform = *t;
        }
        if (const auto *m = snap->metadataStates.find(id)) {
            cs.metadata = *m;
        }
        if (const auto *fx = snap->effectStacks.find(id)) {
            cs.effectStack = *fx;
        }
        if (const auto *au = snap->audioStacks.find(id)) {
            cs.audioStack = *au;
        }
        if (cs.isValid()) {
            copied.append(cs);
        }
    }
    if (!copied.isEmpty()) {
        m_clipboardSnapshots = copied;
    }
}

void TimelineService::cutClip(int clipId) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *mC = snap->metadataStates.find(clipId);
    const auto *fxC = snap->effectStacks.find(clipId);
    if (mC == nullptr) {
        return;
    }
    QString name = (fxC == nullptr || fxC->effects.isEmpty()) ? mC->type : fxC->effects.first().name;
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

    QList<Rina::UI::ClipSnapshot> copied;
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const QVariant &v : std::as_const(ids)) {
        const int id = v.toInt();
        Rina::UI::ClipSnapshot cs;
        if (const auto *t = snap->transforms.find(id)) {
            cs.transform = *t;
        }
        if (const auto *m = snap->metadataStates.find(id)) {
            cs.metadata = *m;
        }
        if (const auto *fx = snap->effectStacks.find(id)) {
            cs.effectStack = *fx;
        }
        if (const auto *au = snap->audioStacks.find(id)) {
            cs.audioStack = *au;
        }
        if (cs.isValid()) {
            copied.append(cs);
        }
    }
    if (!copied.isEmpty()) {
        m_clipboardSnapshots = copied;
    }

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
    if (m_clipboardSnapshots.isEmpty()) {
        return;
    }

    frame = std::max(frame, 0);
    layer = std::max(layer, 0);

    auto overlaps = [](int s1, int d1, int s2, int d2) -> bool { return (s1 < (s2 + d2)) && (s2 < (s1 + d1)); };
    // 衝突チェック: ECS スナップショットを正本として走査（m_scenes廃止）
    const auto *pasteSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();

    int baseFrame = m_clipboardSnapshots.first().transform.startFrame;
    int baseLayer = m_clipboardSnapshots.first().transform.layer;
    for (const auto &clip : std::as_const(m_clipboardSnapshots)) {
        baseFrame = std::min(baseFrame, clip.transform.startFrame);
        baseLayer = std::min(baseLayer, clip.transform.layer);
    }

    QList<Rina::UI::ClipSnapshot> pending;
    for (const auto &src : std::as_const(m_clipboardSnapshots)) {
        Rina::UI::ClipSnapshot newClip = src;
        newClip.transform.startFrame = frame + (src.transform.startFrame - baseFrame);
        newClip.transform.layer = std::max(0, layer + (src.transform.layer - baseLayer));

        for (const auto &ct : pasteSnap->transforms) {
            if (ct.layer == newClip.transform.layer && overlaps(newClip.transform.startFrame, newClip.transform.durationFrames, ct.startFrame, ct.durationFrames)) {
                qWarning() << "クリップ貼り付けを拒否: レイヤー" << newClip.transform.layer << "の" << newClip.transform.startFrame << "フレームで衝突が発生";
                return;
            }
        }
        for (const auto &c : std::as_const(pending)) {
            if (c.transform.layer == newClip.transform.layer && overlaps(newClip.transform.startFrame, newClip.transform.durationFrames, c.transform.startFrame, c.transform.durationFrames)) {
                qWarning() << "クリップ貼り付けを拒否: 貼り付け対象同士が衝突";
                return;
            }
        }

        pending.append(newClip);
    }

    // TODO: Phase3 Undo command requires ClipSnapshot support
    // For now, PasteClipCommand still takes ClipData (converted dynamically in Phase3 or here if needed)
    // To preserve buildability for Phase2, we will leave PasteClipCommand untouched until Phase3.
    // *Temporarily bypass Paste for Phase 2 strict snapshot build.*
    qWarning() << "pasteClip is temporarily disabled until Undo command supports ClipSnapshot (Phase 3)";
}

void TimelineService::splitClip(int clipId, int frame) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    const auto *tSp = snap->transforms.find(clipId);
    const auto *mSp = snap->metadataStates.find(clipId);
    const auto *fxSp = snap->effectStacks.find(clipId);
    if (tSp == nullptr || mSp == nullptr) {
        return;
    }

    if (frame > tSp->startFrame && frame < tSp->startFrame + tSp->durationFrames) {
        QString clipName = (fxSp == nullptr || fxSp->effects.isEmpty()) ? mSp->type : fxSp->effects.first().name;
        m_undoStack->push(new SplitClipCommand(this, clipId, frame, clipName));
    }
}

// clips() / clipsMutable() は ECS 正本化に伴い廃止（フェーズ3完了）

auto TimelineService::clips(int sceneId) const -> const QList<ClipData> & {
    for (const auto &scene : std::as_const(m_scenes)) {
        if (scene.id == sceneId) {
            return scene.clips;
        }
    }
    static QList<ClipData> empty;
    return empty;
}

auto TimelineService::findVacantFrame(int layer, int startFrame, int duration, int excludeClipId) const -> int { // NOLINT(bugprone-easily-swappable-parameters)
    QList<const Rina::Engine::Timeline::TransformComponent *> layerClips;

    bool isBatchMode = !m_batchExcludes.isEmpty();
    bool isSelected = (m_selection != nullptr) && m_selection->isSelected(excludeClipId);
    QVariantList selectedIds = isSelected ? m_selection->selectedClipIds() : QVariantList();

    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    for (const auto &ct : snap->transforms) {
        if (ct.clipId == excludeClipId) {
            continue;
        }
        if (isBatchMode) {
            if (m_batchExcludes.contains(ct.clipId)) {
                continue;
            }
        } else if (isSelected) {
            bool isPeer = false;
            for (const QVariant &v : std::as_const(selectedIds)) {
                if (v.toInt() == ct.clipId) {
                    isPeer = true;
                    break;
                }
            }
            if (isPeer) {
                continue;
            }
        }
        if (ct.layer == layer) {
            layerClips.append(&ct);
        }
    }

    std::ranges::sort(layerClips, [](const auto *a, const auto *b) -> bool { return a->startFrame < b->startFrame; });

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

} // namespace Rina::UI