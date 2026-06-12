#include "timeline_evaluation_system.hpp"
#include "../components/time_range_component.hpp"
#include <algorithm>

namespace AviQtl {
namespace Engine {

std::vector<EntityId>
TimelineEvaluationSystem::getActiveClipsAtFrame(const World &world,
                                                int64_t frame) {
  std::vector<EntityId> activeClips;

  const auto *storage = world.getStorage<TimeRangeComponent>();
  if (!storage) {
    return activeClips;
  }

  for (const auto &[entity, range] : storage->all()) {
    if (frame >= range.startFrame &&
        frame < (range.startFrame + range.durationFrames)) {
      activeClips.push_back(entity);
    }
  }

  // Sort by layer so that they can be rendered in the correct order (e.g.
  // higher layer rendered on top, or bottom-up)
  std::sort(activeClips.begin(), activeClips.end(),
            [&](EntityId a, EntityId b) {
              const auto *rangeA = storage->get(a);
              const auto *rangeB = storage->get(b);
              if (rangeA && rangeB) {
                return rangeA->layer < rangeB->layer;
              }
              return a < b;
            });

  return activeClips;
}

} // namespace Engine
} // namespace AviQtl
