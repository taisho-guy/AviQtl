// タイムラインオブジェクトのためのエンティティ・コンポーネント・システム
#include "ecs.hpp"
#include <QDebug>
#include <QString>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace Rina::Engine::Timeline {

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
    // Lua FFIからのアクセスを容易にするため、POD型メンバを先頭に配置
    bool renderGraphDirty = false;
    std::unordered_map<int, std::unique_ptr<TransformComponent>> transforms;
    std::unordered_map<int, std::unique_ptr<RenderComponent>> renderStates;
    std::unordered_map<int, std::unique_ptr<AudioComponent>> audioStates;
};

static ECSImpl g_ecsState;

ECS &ECS::instance() {
    static ECS inst;
    return inst;
}

void ECS::syncClipIds(const QSet<int> &aliveIds) {
    auto erase_dead = [&aliveIds](auto &map) {
        for (auto it = map.begin(); it != map.end();) {
            if (!aliveIds.contains(it->first)) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };

    erase_dead(g_ecsState.transforms);
    erase_dead(g_ecsState.renderStates);
    erase_dead(g_ecsState.audioStates);
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

        // qDebug() << "[ECS] クリップ" << clipId << "を更新 -> レイヤー:" << layer << "時間:" << time;
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) {
    auto &audio = g_ecsState.audioStates[clipId];
    if (!audio) {
        audio = std::make_unique<AudioComponent>();
        audio->clipId = clipId;
    }
    // パラメータの更新
    audio->startFrame = startFrame;
    audio->durationFrames = durationFrames;
    audio->volume = volume;
    audio->pan = pan;
    audio->mute = mute;
}

const std::unordered_map<int, std::unique_ptr<AudioComponent>> &ECS::getAudioComponents() const { return g_ecsState.audioStates; }

bool ECS::isRenderGraphDirty() const { return g_ecsState.renderGraphDirty; }

void ECS::markRenderGraphClean() { g_ecsState.renderGraphDirty = false; }

void *ECS::getInternalStatePtr() { return &g_ecsState; }
} // namespace Rina::Engine::Timeline

// --- Lua FFI Helper Functions ---
// 名前マングリングを回避し、Luaから直接シンボル検索できるようにする
extern "C" {
// AudioComponentへの生ポインタを取得
// 注意: 戻り値は Component 継承クラスなので、先頭に vptr があることに注意
Rina::Engine::Timeline::AudioComponent *get_audio_component_raw(int clipId) {
    auto &map = Rina::Engine::Timeline::g_ecsState.audioStates;
    auto it = map.find(clipId);
    if (it != map.end()) {
        return it->second.get();
    }
    return nullptr;
}

// 必要に応じて TransformComponent 等も同様に追加可能
}
