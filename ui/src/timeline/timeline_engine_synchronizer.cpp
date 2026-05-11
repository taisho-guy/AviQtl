#include "timeline_engine_synchronizer.hpp"
#include "bridge/core_bridge.hpp"
#include "clip_model.hpp"
#include "engine/timeline/ecs.hpp"
#include "engine/timeline/ecs_profiler.hpp"
#include "timeline_controller.hpp"
#include <algorithm>
#include <bitset>

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2.1: 旧同期機構の除去
//
// 変更点:
//   - layerCount() / clipsInLayer() への依存を除去。
//     TimelineService は SceneData::clips をフラット QList<ClipData> で保持するため、
//     clips(sceneId) を直接反復してクリップインデックスを構築する (ECS DOD 正道)。
//   - ECS::updateClipState / updateAudioClipState への直接呼び出しを維持しつつ、
//     フレーム変化通知を CoreBridge::notifyFrameAdvanced() 経由に切り替える。
// ─────────────────────────────────────────────────────────────────────────────

namespace AviQtl::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller), m_clipModel(new ClipModel(controller->transport(), this)) {}

int TimelineEngineSynchronizer::buildIntervalTree(int left, int right) {
    if (left > right)
        return -1;
    int mid = left + (right - left) / 2;
    int nodeIdx = mid;
    auto &node = m_intervalTree[nodeIdx];
    node.clip = m_sortedClips[mid];
    node.maxEnd = node.clip->startFrame + node.clip->durationFrames;
    node.left = buildIntervalTree(left, mid - 1);
    node.right = buildIntervalTree(mid + 1, right);
    if (node.left != -1)
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.left].maxEnd);
    if (node.right != -1)
        node.maxEnd = std::max(node.maxEnd, m_intervalTree[node.right].maxEnd);
    return nodeIdx;
}

void TimelineEngineSynchronizer::queryIntervalTree(int nodeIdx, int frame, QList<ClipData *> &result) {
    if (nodeIdx == -1)
        return;
    const auto &node = m_intervalTree[nodeIdx];
    if (node.maxEnd <= frame)
        return;
    if (frame < node.clip->startFrame) {
        queryIntervalTree(node.left, frame, result);
    } else {
        int end = node.clip->startFrame + node.clip->durationFrames;
        if (frame < end) {
            if (!m_controller->timeline()->isLayerHidden(node.clip->layer))
                result.append(node.clip);
        }
        queryIntervalTree(node.left, frame, result);
        queryIntervalTree(node.right, frame, result);
    }
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    const auto *timeline = m_controller->timeline();
    if (!timeline)
        return;

    m_sortedClips.clear();

    // TimelineService は SceneData::clips をフラット QList<ClipData> で保持する。
    // clips(sceneId) で現在シーンのフラット配列を取得し直接反復する。
    // (旧: layerCount() / clipsInLayer() への依存を除去)
    const int currentSceneId = timeline->currentSceneId();
    const QList<ClipData> &allClips = timeline->clips(currentSceneId);
    m_sortedClips.reserve(allClips.size());
    for (const ClipData &clip : allClips) {
        // QList<ClipData> の要素アドレスを取得する。
        // Qt6 の QList<T> は連続メモリを保証するため、
        // const参照が有効な間はアドレスが安定する。
        m_sortedClips.append(const_cast<ClipData *>(&clip));
    }

    std::ranges::sort(m_sortedClips, [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    m_intervalTree.resize(m_sortedClips.size());
    m_treeRoot = buildIntervalTree(0, static_cast<int>(m_sortedClips.size()) - 1);

    // alive フラグを再構築して ECS から不要エンティティを除去
    std::bitset<AviQtl::Engine::Timeline::MAX_CLIP_ID> aliveFlags;
    for (const auto *clip : std::as_const(m_sortedClips)) {
        if (clip->id >= 0 && clip->id < AviQtl::Engine::Timeline::MAX_CLIP_ID)
            aliveFlags.set(static_cast<std::size_t>(clip->id));
    }
    AviQtl::Engine::Timeline::ECS::instance().syncClipIds(aliveFlags);

    // タイムライン総フレーム数を再計算
    m_maxDuration = 0;
    for (const auto *clip : std::as_const(m_sortedClips)) {
        m_maxDuration = std::max(m_maxDuration, clip->startFrame + clip->durationFrames);
    }
    m_timelineDuration = m_maxDuration;
}

void TimelineEngineSynchronizer::updateActiveClipsList() {
    ECS_TIMER_SCOPE(updateActiveNs);

    int current = m_controller->transport()->currentFrame();
    QList<ClipData *> active;
    queryIntervalTree(m_treeRoot, current, active);
    std::ranges::sort(active, [](const ClipData *a, const ClipData *b) { return a->layer > b->layer; });

    for (const ClipData *clip : std::as_const(active)) {
        int relFrame = current - clip->startFrame;

        AviQtl::Engine::Timeline::ECS::instance().updateClipState(clip->id, clip->layer, relFrame, clip->startFrame, clip->durationFrames);

        if (clip->type == QLatin1String("audio") || clip->type == QLatin1String("video")) {
            float vol = 1.0F;
            float pan = 0.0F;
            bool mute = false;
            for (const auto *eff : clip->effects) {
                if (eff->id() == clip->type) {
                    vol = static_cast<float>(eff->evaluatedParam(QStringLiteral("volume"), relFrame).toDouble());
                    pan = static_cast<float>(eff->evaluatedParam(QStringLiteral("pan"), relFrame).toDouble());
                    mute = eff->evaluatedParam(QStringLiteral("mute"), relFrame).toBool();
                    break;
                }
            }
            AviQtl::Engine::Timeline::ECS::instance().updateAudioClipState(clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }

    AviQtl::Engine::Timeline::ECS::instance().commit();

    // Phase 2.1: フレーム更新を CoreBridge 経由で通知する
    CoreBridge::instance().notifyFrameAdvanced(current);
}

} // namespace AviQtl::UI
