#pragma once
#include "../ecs/world.hpp"
#include <vector>

namespace AviQtl {
namespace Engine {

class TimelineEvaluationSystem {
public:
  // Evaluates which clip entities are active at the given frame number,
  // returning their IDs sorted by layer.
  static std::vector<EntityId> getActiveClipsAtFrame(const World &world,
                                                     int64_t frame);
};

} // namespace Engine
} // namespace AviQtl
