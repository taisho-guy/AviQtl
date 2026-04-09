#include "parameter_evaluation_system.hpp"
#include "effect_model.hpp"
#include "engine/timeline/ecs.hpp"

namespace Rina::UI {

void ParameterEvaluationSystem::evaluate(const QList<ClipData *> &active, int currentFrame, double fps) {
    auto &editState = Rina::Engine::Timeline::ECS::instance().editState();

    for (const ClipData *clip : active) {
        const int relFrame = currentFrame - clip->startFrame;
        auto &ep = editState.evaluatedParams[clip->id];
        ep.effects.clear();

        for (auto *eff : clip->effects) {
            QVariantMap evaluated;
            const QVariantMap &raw = eff->params();
            for (auto it = raw.cbegin(); it != raw.cend(); ++it) {
                evaluated[it.key()] = eff->evaluatedParam(it.key(), relFrame, fps);
            }
            ep.effects[eff->id()] = std::move(evaluated);
        }

        Rina::Engine::Timeline::ECS::instance().markEvaluatedParamsDirty(clip->id);
    }
}

} // namespace Rina::UI
