#include "clip_effect_system.hpp"

namespace Rina::Engine::Timeline {

QVariantMap ClipEffectSystem::evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame) {
    QVariantMap out;

    if (const auto *stack = effectStacks.find(clipId)) {
        // QObject (EffectModel) を介さず、純粋なデータからパラメータを構築
        for (const auto &effectVariant : stack->effects) {
            // フェーズ4では QVariantMap や構造体としてエフェクトデータが保存されている前提
            QVariantMap effectMap = effectVariant.toMap();
            QString id = effectMap.value(QStringLiteral("id")).toString();
            QVariantMap params = effectMap.value(QStringLiteral("params")).toMap();

            // 実際にはここで relFrame に応じたキーフレーム補間計算（イージング等）を行う
            // 現在は固定パラメータをそのまま返す
            out.insert(id, params);
        }
    }
    return out;
}

void ClipEffectSystem::updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, float value) {
    if (auto *stack = state.effectStacks.find(clipId)) {
        if (effectIndex >= 0 && effectIndex < stack->effects.size()) {
            QVariantMap effectMap = stack->effects[effectIndex].toMap();
            QVariantMap params = effectMap.value(QStringLiteral("params")).toMap();
            params.insert(paramName, value);
            effectMap.insert(QStringLiteral("params"), params);
            stack->effects[effectIndex] = effectMap;
        }
    }
}

} // namespace Rina::Engine::Timeline
