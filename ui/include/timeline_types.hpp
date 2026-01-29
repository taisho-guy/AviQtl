#pragma once
#include <QString>
#include <QVariant>
#include <QList>
#include "effect_model.hpp"

namespace Rina::UI {

    struct EffectData {
        QString id;
        QString name;
        QString qmlSource;
        bool enabled = true;
        QVariantMap params;
    };

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
        
        // エフェクトスタック
        QList<EffectModel*> effects;
    };
}