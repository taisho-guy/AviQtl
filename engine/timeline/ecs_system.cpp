#include "ecs.hpp"
#include <QDebug>
#include <cmath>

namespace Rina::Engine::Timeline {

ECS::ECS() {
    m_activeIndex.store(0, std::memory_order_relaxed);
    m_editIndex = 1;
}

ECS &ECS::instance() {
    static ECS inst;
    return inst;
}

void ECS::syncClipIds(const QSet<int> &aliveIds) {
    auto &editState = m_buffers[m_editIndex];
    editState.transforms.syncAlive(aliveIds);
    editState.renderStates.syncAlive(aliveIds);
    editState.audioStates.syncAlive(aliveIds);
}

void ECS::updateClipState(int clipId, int layer, double time) {
    auto &editState = m_buffers[m_editIndex];
    auto &transform = editState.transforms[clipId];

    bool changed = (transform.layer != layer || std::abs(transform.timePosition - time) > 0.001);

    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;

        auto &render = editState.renderStates[clipId];
        render.needsUpdate = true;
        editState.renderGraphDirty = true;
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) {
    auto &editState = m_buffers[m_editIndex];
    auto &audio = editState.audioStates[clipId];
    audio.clipId = clipId;
    audio.startFrame = startFrame;
    audio.durationFrames = durationFrames;
    audio.volume = volume;
    audio.pan = pan;
    audio.mute = mute;
}

void ECS::commit() {
    // 現在の edit バッファのインデックスを新しい active バッファにする
    int nextActive = m_editIndex;

    // 次の edit バッファを選ぶ (0, 1, 2 を循環)
    m_editIndex = (m_editIndex + 1) % 3;

    // 次の edit バッファが現在 active として使われているなら、もう一つ進める
    if (m_editIndex == m_activeIndex.load(std::memory_order_acquire)) {
        m_editIndex = (m_editIndex + 1) % 3;
    }

    // 次の edit バッファに、今の確定済み状態をコピーしておく(差分更新のベースとして)
    m_buffers[m_editIndex] = m_buffers[nextActive];

    // 最新の active インデックスを公開
    m_activeIndex.store(nextActive, std::memory_order_release);
}

const ECSState *ECS::getSnapshot() const { return &m_buffers[m_activeIndex.load(std::memory_order_acquire)]; }

bool ECS::isRenderGraphDirty() const { return m_buffers[m_editIndex].renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers[m_editIndex].renderGraphDirty = false; }

} // namespace Rina::Engine::Timeline

extern "C" {
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
