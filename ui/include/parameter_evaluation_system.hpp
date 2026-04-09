#pragma once
#include "../../engine/timeline/ecs.hpp"
#include "timeline_types.hpp"
#include <QList>

namespace Rina::UI {

class ParameterEvaluationSystem {
  public:
    void evaluate(const QList<ClipData *> &active, int currentFrame, double fps);
};

} // namespace Rina::UI
