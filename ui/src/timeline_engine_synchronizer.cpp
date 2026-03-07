#include "timeline_engine_synchronizer.hpp"
#include "../../engine/timeline/ecs.hpp"
#include "clip_model.hpp"
#include "timeline_controller.hpp"
#include <algorithm>

namespace Rina::UI {

TimelineEngineSynchronizer::TimelineEngineSynchronizer(TimelineController *controller, QObject *parent) : QObject(parent), m_controller(controller) { m_clipModel = new ClipModel(controller->transport(), this); }

void TimelineEngineSynchronizer::updateActiveClipsList() {
    int current = m_controller->transport()->currentFrame();
    QList<ClipData *> active = m_controller->timeline()->resolvedActiveClipsAt(current);

    std::sort(active.begin(), active.end(), [](const ClipData *a, const ClipData *b) { return a->layer < b->layer; });

    m_clipModel->updateClips(active);
    updateECSState(active, current);
    Rina::Engine::Timeline::ECS::instance().commit();
}

void TimelineEngineSynchronizer::updateECSState(const QList<ClipData *> &activeClips, int currentFrame) {
    for (const auto *clip : activeClips) {
        Rina::Engine::Timeline::ECS::instance().updateClipState(clip->id, clip->layer, currentFrame - clip->startFrame);

        if (clip->type == "audio" || clip->type == "video") {
            float vol = 1.0f;
            float pan = 0.0f;
            bool mute = false;
            const QString paramSourceId = clip->type;
            for (auto *eff : clip->effects) {
                if (eff->id() == paramSourceId) {
                    int relFrame = currentFrame - clip->startFrame;
                    QVariant vVol = eff->evaluatedParam("volume", relFrame);
                    vol = vVol.isValid() ? vVol.toFloat() : 1.0f;
                    pan = eff->evaluatedParam("pan", relFrame).toFloat();
                    mute = eff->evaluatedParam("mute", relFrame).toBool();
                    break;
                }
            }
            Rina::Engine::Timeline::ECS::instance().updateAudioClipState(clip->id, clip->startFrame, clip->durationFrames, vol, pan, mute);
        }
    }
}

void TimelineEngineSynchronizer::rebuildClipIndex() {
    m_sortedClips.clear();
    m_maxDuration = 0;
    m_timelineDuration = 0;
    auto &clips = m_controller->timeline()->clipsMutable();
    m_sortedClips.reserve(clips.size());

    QSet<int> aliveIds;

    for (auto &clip : clips) {
        aliveIds.insert(clip.id);
        m_sortedClips.push_back(&clip);
        if (clip.durationFrames > m_maxDuration)
            m_maxDuration = clip.durationFrames;

        int end = clip.startFrame + clip.durationFrames;
        if (end > m_timelineDuration)
            m_timelineDuration = end;
    }
    std::sort(m_sortedClips.begin(), m_sortedClips.end(), [](const ClipData *a, const ClipData *b) { return a->startFrame < b->startFrame; });

    // ECSの不要になったコンポーネントを掃除する
    Rina::Engine::Timeline::ECS::instance().syncClipIds(aliveIds);
}
} // namespace Rina::UI