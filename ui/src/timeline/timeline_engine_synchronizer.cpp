#include "timeline_engine_synchronizer.hpp"
#include "clip_model.hpp"
#include "engine/timeline/ecs.hpp"
#include "engine/timeline/ecs_profiler.hpp"
#include "timeline_controller.hpp"
#include <algorithm>
#include <bitset>

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

void TimelineEngineSynchronizer::updateActiveClipsList() {
    ECS_TIMER_SCOPE(updateActiveNs);

    int current = m_controller->transport()->currentFrame();
    QList<ClipData *> active;
    queryIntervalTree(m_treeRoot, current, active);
    std::ranges::sort(active, [](const ClipData *a, const ClipData *b) -> bool { return a->layer > b->layer; });

    double fps = m_controller->project()->fps();

    for (const ClipData *clip : std::as_const(active)) {
        int relFrame = current - clip->startFrame;

        // フェーズ7: startFrame / durationFrames を追加
        // Phase 6 では TransformComponent.startFrame が常に 0 のままだったため
        // GpuTransformSoA.startFrames[] が無効な値を持っていた
        // これを修正し TransformComponent をタイミング情報の正規のソースとする
        AviQtl::Engine::Timeline::ECS::instance().updateClipState(
            clip->id, clip->layer, relFrame, clip->startFrame, clip->durationFrames);

        if (clip->type == QLatin1String("audio") || clip->type == QLatin1String("video")) {
            float vol = 1.0F;
            float pan = 0.0F;
            bool mute = false;
            for (auto *eff : clip->effects) {
                if (eff->id() == clip->type) {
                    vol = eff->evaluatedParamFloat(QStringLiteral("volume"), relFrame, 1.0f, fps);
                    pan = eff->evaluatedParamFloat(QStringLiteral("pan"), relFrame, 0.0f, fps);
                    mute = eff->evaluatedParamBool(QStringLiteral("mute"), relFrame, false, fps);
                    ECS_PROF_ADD(variantEvalCount, 3);
                    break;
                }
            }
            AviQtl::Engine::Timeline::ECS::instance().updateAudioClipState(
                clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }

    AviQtl::Engine::Timeline::ECS::instance().commit();
    m_clipModel->updateClips(active);
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    m_sortedClips.clear();
    m_intervalTree.clear();
    m_treeRoot = -1;
    m_maxDuration = 0;
    m_timelineDuration = 0;

    auto &clips = m_controller->timeline()->clipsMutable();
    m_sortedClips.reserve(clips.size());

    std::bitset<AviQtl::Engine::Timeline::MAX_CLIP_ID> aliveFlags;

    for (auto &clip : clips) {
        assert(clip.id >= 0 && clip.id < AviQtl::Engine::Timeline::MAX_CLIP_ID);
        aliveFlags.set(static_cast<std::size_t>(clip.id));
        m_sortedClips.push_back(&clip);
        m_maxDuration = std::max(clip.durationFrames, m_maxDuration);
        int end = clip.startFrame + clip.durationFrames;
        m_timelineDuration = std::max(end, m_timelineDuration);
    }

    std::ranges::sort(m_sortedClips, [](const ClipData *a, const ClipData *b) -> bool { return a->startFrame < b->startFrame; });

    if (!m_sortedClips.isEmpty()) {
        m_intervalTree.resize(m_sortedClips.size());
        m_treeRoot = buildIntervalTree(0, static_cast<int>(m_sortedClips.size()) - 1);
    }

    AviQtl::Engine::Timeline::ECS::instance().syncClipIds(aliveFlags);
}

} // namespace AviQtl::UI
