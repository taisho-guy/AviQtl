// Entity Component System for Timeline Objects
#include "ecs.hpp"
#include <iostream>

namespace Rina::Engine::Timeline {
struct Component {
    virtual ~Component() = default;
};

struct Entity {
    int id;
    // List of components (Transform, VideoFile, etc.)
};

ECS &ECS::instance() {
    static ECS inst;
    return inst;
}

void ECS::updateClipState(int clipId, int layer, double time) {
    // ここでコンポーネントデータを更新し、RenderGraphの再構築フラグを立てる
    std::cout << "[ECS] Clip " << clipId << " moved -> Layer: " << layer << ", Time: " << time << std::endl;
}
} // namespace Rina::Engine::Timeline
