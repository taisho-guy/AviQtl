#pragma once
#include "engine/timeline/ecs.hpp"
#include <QString>
#include <QVariantMap>
#include <vector>

namespace Rina::Engine::Timeline {

// DOD向けのエフェクト内部データ（QObjectを含まない）
struct EffectParamState {
    QString paramName;
    float value;
    // TODO: キーフレーム情報などもここにフラット化して持たせる
};

struct DODEffectData {
    QString effectId;
    std::vector<EffectParamState> parameters;
};

class ClipEffectSystem {
  public:
    // ECSのコンポーネントから直接エフェクトパラメータを評価する（旧: TimelineController::evaluateClipParams）
    static QVariantMap evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame);

    // UIからのパラメータ変更をECSに書き込む
    static void updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, float value);
};

} // namespace Rina::Engine::Timeline
