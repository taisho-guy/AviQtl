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
    int interpolationType; // 0: 線形
};

struct AudioPluginState {
    QString id;
    bool enabled = true;
    QVariantMap params;
};

struct ClipData {
    int id;
    int sceneId = 0;
    QString type;
    int startFrame;
    int durationFrames;
    int layer;

    // ハイブリッド設計: EffectModelは振る舞いを持つためポインタで保持する
    QList<EffectModel *> effects;
    QList<AudioPluginState> audioPlugins;
};

struct SceneData {
    int id;
    QString name;
    QList<ClipData> clips;

    // シーンのコンテキスト（自己完結化）
    int width = 1920;
    int height = 1080;
    double fps = 60.0;
    int totalFrames = 300;

    // ネスト利用のためのメタデータ
    int startFrame = 0;
    int durationFrames = 0;
};
} // namespace Rina::UI