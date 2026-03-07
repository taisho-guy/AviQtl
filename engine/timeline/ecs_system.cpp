#include "ecs.hpp"
#include <QDebug>
#include <cmath>

namespace Rina::Engine::Timeline {

ECS::ECS() { m_activeState = std::make_shared<ECSState>(); }

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

    erase_dead(m_editState.transforms);
    erase_dead(m_editState.renderStates);
    erase_dead(m_editState.audioStates);
}

void ECS::updateClipState(int clipId, int layer, double time) {
    auto &transform = m_editState.transforms[clipId];

    bool changed = (transform.layer != layer || std::abs(transform.timePosition - time) > 0.001);

    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;

        auto &render = m_editState.renderStates[clipId];
        render.needsUpdate = true;
        m_editState.renderGraphDirty = true;
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) {
    auto &audio = m_editState.audioStates[clipId];
    audio.clipId = clipId;
    audio.startFrame = startFrame;
    audio.durationFrames = durationFrames;
    audio.volume = volume;
    audio.pan = pan;
    audio.mute = mute;
}

void ECS::commit() {
    auto newState = std::make_shared<ECSState>(m_editState);
    m_activeState.store(newState, std::memory_order_release);
}

std::shared_ptr<const ECSState> ECS::getSnapshot() const { return m_activeState.load(std::memory_order_acquire); }

bool ECS::isRenderGraphDirty() const { return m_editState.renderGraphDirty; }

void ECS::markRenderGraphClean() { m_editState.renderGraphDirty = false; }

} // namespace Rina::Engine::Timeline

extern "C" {
// Lua連携用の安全なヘルパー関数
float rina_lua_get_audio_volume(int clipId) {
    auto state = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    if (!state)
        return 1.0f;

    auto it = state->audioStates.find(clipId);
    if (it != state->audioStates.end()) {
        return it->second.volume;
    }
    return 1.0f;
}
}
