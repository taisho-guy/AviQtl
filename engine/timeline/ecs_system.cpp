// Entity Component System for Timeline Objects
#include "ecs.hpp"
#include <QString>
#include <QDebug>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace Rina::Engine::Timeline {
struct Component {
    virtual ~Component() = default;
};

struct TransformComponent : Component {
    int layer;
    double timePosition;
    int startFrame;
    int durationFrames;
};

struct RenderComponent : Component {
    bool needsUpdate = true;
    QString effectChain;
};

struct ECSImpl {
    std::unordered_map<int, std::unique_ptr<TransformComponent>> transforms;
    std::unordered_map<int, std::unique_ptr<RenderComponent>> renderStates;
    bool renderGraphDirty = false;
};

static ECSImpl g_ecsState;

ECS &ECS::instance() {
    static ECS inst;
    return inst;
}

void ECS::updateClipState(int clipId, int layer, double time) {
    // Transform コンポーネントの更新
    auto &transform = g_ecsState.transforms[clipId];
    if (!transform) {
        transform = std::make_unique<TransformComponent>();
    }

    bool changed = (transform->layer != layer || std::abs(transform->timePosition - time) > 0.001);

    if (changed) {
        transform->layer = layer;
        transform->timePosition = time;

        // Render コンポーネントに再計算フラグ
        auto &render = g_ecsState.renderStates[clipId];
        if (!render) {
            render = std::make_unique<RenderComponent>();
        }
        render->needsUpdate = true;
        g_ecsState.renderGraphDirty = true;

        qDebug() << "[ECS] Clip" << clipId << "updated -> Layer:" << layer << "Time:" << time;
    }
}

bool ECS::isRenderGraphDirty() const {
    return g_ecsState.renderGraphDirty;
}

void ECS::markRenderGraphClean() {
    g_ecsState.renderGraphDirty = false;
}
} // namespace Rina::Engine::Timeline
