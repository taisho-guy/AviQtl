#pragma once
#include "effect_model.hpp"
#include <QList>
#include <QString>
#include <QVariant>

namespace Rina::UI {

// 最適化: ClipIDを導入
using ClipID = int;

class EffectModel; // 前方宣言

struct Keyframe {
    int frame;
    float value;
    int interpolationType; // 0: Linear
};

struct ClipData {
    int id;
    QString type;
    int startFrame;
    int durationFrames;
    int layer;

    // ハイブリッド設計: エフェクトスタックのみポインタで保持 (振る舞いを持つため)
    QList<EffectModel *> effects;
};
} // namespace Rina::UI