#include "ecs.hpp"
#include <QDebug>
#include <cmath>

namespace Rina::Engine::Timeline {

ECS::ECS() : m_editIndex(1) {
    m_activeIndex.store(0, std::memory_order_relaxed);

    m_fullSyncRequired.fill(true);
}

auto ECS::instance() -> ECS & {
    static ECS inst;
    return inst;
}

void ECS::syncClipIds(const QSet<int> &aliveIds) {
    auto &editState = m_buffers.value(m_editIndex);
    bool changed = false;
    changed |= editState.transforms.syncAlive(aliveIds);
    changed |= editState.renderStates.syncAlive(aliveIds);
    changed |= editState.audioStates.syncAlive(aliveIds);
    changed |= editState.metadataStates.syncAlive(aliveIds);

    if (changed) {
        m_fullSyncRequired.insert((m_editIndex + 1) % 3, true);
        m_fullSyncRequired.insert((m_editIndex + 2) % 3, true);
    }
}

void ECS::updateClipState(int clipId, int layer, double time) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &editState = m_buffers.value(m_editIndex);

    if (!editState.transforms.contains(clipId)) {
        m_fullSyncRequired.insert((m_editIndex + 1) % 3, true);
        m_fullSyncRequired.insert((m_editIndex + 2) % 3, true);
    }
    auto &transform = editState.transforms.value(clipId);

    bool changed = (transform.layer != layer || std::abs(transform.timePosition - time) > 0.001);

    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;

        auto &render = editState.renderStates.value(clipId);
        render.needsUpdate = true;
        editState.renderGraphDirty = true;

        m_dirtyForBuffer.value((m_editIndex + 1) % 3).insert(clipId);
        m_dirtyForBuffer.value((m_editIndex + 2) % 3).insert(clipId);
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &editState = m_buffers.value(m_editIndex);

    if (!editState.audioStates.contains(clipId)) {
        m_fullSyncRequired.insert((m_editIndex + 1) % 3, true);
        m_fullSyncRequired.insert((m_editIndex + 2) % 3, true);
    }
    auto &audio = editState.audioStates.value(clipId);
    audio.clipId = clipId;
    audio.startFrame = startFrame;
    audio.durationFrames = durationFrames;
    audio.volume = volume;
    audio.pan = pan;
    audio.mute = mute;

    // 音声パラメータは頻繁に変わる可能性があるためダーティフラグ処理
    m_dirtyForBuffer.value((m_editIndex + 1) % 3).insert(clipId);
    m_dirtyForBuffer.value((m_editIndex + 2) % 3).insert(clipId);
}

void ECS::updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color) {
    auto &editState = m_buffers.value(m_editIndex);

    if (!editState.metadataStates.contains(clipId)) {
        m_fullSyncRequired.insert((m_editIndex + 1) % 3, true);
        m_fullSyncRequired.insert((m_editIndex + 2) % 3, true);
    }
    auto &meta = editState.metadataStates.value(clipId);

    if (meta.clipId != clipId || meta.name != name || meta.source != source || meta.type != type || meta.color != color) {
        meta.clipId = clipId;
        meta.name = name;
        meta.source = source;
        meta.type = type;
        meta.color = color;
        m_dirtyForBuffer.value((m_editIndex + 1) % 3).insert(clipId);
        m_dirtyForBuffer.value((m_editIndex + 2) % 3).insert(clipId);
    }
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

    if (m_fullSyncRequired.value(m_editIndex)) {
        // 構造変更があった、または初期状態の場合はフルコピー
        m_buffers.insert(m_editIndex, m_buffers.value(nextActive));
        m_fullSyncRequired.insert(m_editIndex, false);
        m_dirtyForBuffer.value(m_editIndex).clear();
    } else {
        // 部分更新: 変更があったコンポーネントのみコピー
        const auto &src = m_buffers.value(nextActive);
        auto &dst = m_buffers.value(m_editIndex);

        dst.renderGraphDirty = src.renderGraphDirty;

        for (int id : m_dirtyForBuffer.value(m_editIndex)) {
            if (const auto *s = src.transforms.find(id)) {
                dst.transforms.insert(id, *s);
            }
            if (const auto *s = src.renderStates.find(id)) {
                dst.renderStates.insert(id, *s);
            }
            if (const auto *s = src.audioStates.find(id)) {
                dst.audioStates.insert(id, *s);
            }
            if (const auto *s = src.metadataStates.find(id)) {
                dst.metadataStates.insert(id, *s);
            }
        }
        m_dirtyForBuffer.value(m_editIndex).clear();
    }

    // 最新の active インデックスを公開
    m_activeIndex.store(nextActive, std::memory_order_release);
}

auto ECS::getSnapshot() const -> const ECSState * { return &m_buffers.value(m_activeIndex.load(std::memory_order_acquire)); }

auto ECS::isRenderGraphDirty() const -> bool { return m_buffers.value(m_editIndex).renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers.value(m_editIndex).renderGraphDirty = false; }

} // namespace Rina::Engine::Timeline

extern "C" {
auto rina_lua_get_audio_volume(int clipId) -> float {
    const auto *state = Rina::Engine::Timeline::ECS::instance().getSnapshot();
    if (state == nullptr) {
        return 1.0F;
    }

    const auto *it = state->audioStates.find(clipId);
    if (it != nullptr) {
        return it->volume;
    }
    return 1.0F;
}
}
