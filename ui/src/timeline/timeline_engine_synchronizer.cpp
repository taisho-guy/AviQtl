#include "timeline_engine_synchronizer.hpp"
#include "clip_model.hpp"
#include "effect_data.hpp"
#include "engine/timeline/ecs.hpp"
#include "timeline_controller.hpp"
#include <algorithm>

namespace Rina::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller), m_clipModel(new ClipModel(controller->transport(), this)) {}

int TimelineEngineSynchronizer::buildIntervalTree(int left, int right) {
    if (left > right) {
        return -1;
    }
    int mid = left + (right - left) / 2;
    int nodeIdx = mid; // インデックスと配列位置を一致させる（O(1)アクセス）
    auto &node = m_intervalTree[nodeIdx];
    node.clip = m_sortedClips[mid];
    node.maxEnd = node.clip->startFrame + node.clip->durationFrames;
    node.left = buildIntervalTree(left, mid - 1);
    node.right = buildIntervalTree(mid + 1, right);

    if (node.left != -1) {
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.left].maxEnd);
    }
    if (node.right != -1) {
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.right].maxEnd);
    }
    return nodeIdx;
}

void TimelineEngineSynchronizer::queryIntervalTree(int nodeIdx, int frame, QList<ClipData *> &result) {
    if (nodeIdx == -1) {
        return;
    }
    const auto &node = m_intervalTree[nodeIdx];

    // 枝刈り: この部分木の最大終了時間が現在フレーム以下なら交差するクリップは存在しない
    if (node.maxEnd <= frame) {
        return;
    }

    if (frame < node.clip->startFrame) {
        // 現在フレームがこのノードの開始時間より前の場合、
        // 右部分木のクリップは全て開始時間がさらに未来になるため交差しない。左のみ探索する。
        queryIntervalTree(node.left, frame, result);
    } else {
        // 交差判定
        int end = node.clip->startFrame + node.clip->durationFrames;
        if (frame < end) {
            if (!m_controller->timeline()->isLayerHidden(node.clip->layer)) {
                result.append(node.clip);
            }
        }
        // 左部分木にもまだ交差する長いクリップがあるかもしれない
        queryIntervalTree(node.left, frame, result);
        // 右部分木は開始時間が現在フレーム以下なので交差の可能性がある
        queryIntervalTree(node.right, frame, result);
    }
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    qDebug() << "[DOD Sync] updateActiveClipsList: frame=" << m_controller->transport()->currentFrame();
    int current = m_controller->transport()->currentFrame();

    QList<ClipData *> active;
    // 厳密な O(log n + k) 区間検索 (Interval Tree)
    queryIntervalTree(m_treeRoot, current, active);
    qDebug() << "[DOD Sync] IntervalTree query result count:" << active.size();

    // レイヤー番号が小さいほど背面、大きいほど前面になるようソート
    std::ranges::sort(active, [](const ClipData *a, const ClipData *b) -> bool { return a->layer > b->layer; });

    double fps = m_controller->project()->fps();

    // ECS が正本: m_localClips の clip->effects (EffectModel*) は空のため使わない
    // ECS effectStacks から直接 vol/pan/mute を取得し、updateEffectStack は呼ばない
    const auto *ecsSnap = Rina::Engine::Timeline::ECS::instance().getSnapshot();

    for (const ClipData *clip : std::as_const(active)) {
        int relFrame = current - clip->startFrame;

        Rina::Engine::Timeline::ECS::instance().updateClipState(clip->id, clip->layer, clip->startFrame, clip->durationFrames, static_cast<double>(relFrame));

        // ECS effectStacks が正本のため updateEffectStack による上書きは行わない

        if (clip->type == QLatin1String("audio") || clip->type == QLatin1String("video")) {
            float vol = 1.0F;
            float pan = 0.0F;
            bool mute = false;
            // ECS effectStacks から対象エフェクトの params を直接参照する
            if (const auto *fxSnap = ecsSnap->effectStacks.find(clip->id)) {
                for (const auto &eff : std::as_const(fxSnap->effects)) {
                    if (eff.id == clip->type) {
                        const QVariant vVol = eff.params.value(QStringLiteral("volume"));
                        if (vVol.isValid()) {
                            vol = vVol.toFloat();
                        }
                        pan = eff.params.value(QStringLiteral("pan")).toFloat();
                        mute = eff.params.value(QStringLiteral("mute")).toBool();
                        break;
                    }
                }
            }
            Rina::Engine::Timeline::ECS::instance().updateAudioClipState(clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }

    Rina::Engine::Timeline::ECS::instance().commit();
    qDebug() << "[DOD Sync] Sending to ClipModel. Size=" << active.size();
    m_clipModel->updateClips(active);
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    qDebug() << "[DOD Sync] rebuildClipIndex called.";
    m_sortedClips.clear();
    m_localClips.clear();
    m_intervalTree.clear();
    m_treeRoot = -1;
    m_maxDuration = 0;
    m_timelineDuration = 0;

    // フェーズ2: m_scenes の直接参照をやめ、ECS スナップショットを走査して
    // m_localClips（所有バッファ）を構築する。m_sortedClips はそのポインタを持つ。
    const auto *ecsState = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    if (!ecsState) {
        qWarning() << "[DOD Sync] rebuildClipIndex: ECS スナップショットが null です。";
        return;
    }

    QSet<int> aliveIds;

    // ECS の TransformComponent を走査（キャッシュに乗る DOD アクセス）
    for (auto it = ecsState->transforms.begin(); it != ecsState->transforms.end(); ++it) {
        ClipData clip;
        clip.id = it->clipId;
        clip.layer = it->layer;
        clip.startFrame = it->startFrame;
        clip.durationFrames = it->durationFrames;
        if (const auto *meta = ecsState->metadataStates.find(it->clipId)) {
            clip.type = meta->type;
        }
        // フェーズ3完了まで audioPlugins は空、sceneId は -1 で暫定運用
        // effects/audioPlugins の補完は EffectModel DOD 化（フェーズ3）後に実装
        if (const auto *fxComp = ecsState->effectStacks.find(it->clipId)) {
            Q_UNUSED(fxComp)
        }
        clip.sceneId = -1;
        m_localClips.append(clip);
        aliveIds.insert(clip.id);
    }

    m_sortedClips.reserve(m_localClips.size());
    for (auto &clip : m_localClips) {
        m_sortedClips.push_back(&clip);
        m_maxDuration = std::max(clip.durationFrames, m_maxDuration);
        int end = clip.startFrame + clip.durationFrames;
        m_timelineDuration = std::max(end, m_timelineDuration);
    }

    // startFrame でソート
    std::ranges::sort(m_sortedClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    // Interval Tree の構築 (O(n))
    if (!m_sortedClips.isEmpty()) {
        qDebug() << "[DOD Sync] m_sortedClips size:" << m_sortedClips.size();
        m_intervalTree.resize(m_sortedClips.size());
        m_treeRoot = buildIntervalTree(0, static_cast<int>(m_sortedClips.size()) - 1);
    }

    // ECS の不要になったコンポーネントを掃除する
    Rina::Engine::Timeline::ECS::instance().syncClipIds(aliveIds);
}
} // namespace Rina::UI
