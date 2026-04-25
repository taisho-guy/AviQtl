#include "ecs.hpp"
#include <QDebug>
#include <cmath>

namespace AviQtl::Engine::Timeline {

ECS::ECS() : m_editIndex(1) {
    m_activeIndex.store(0, std::memory_order_relaxed);

    m_fullSyncRequired.fill(true);
}

auto ECS::instance() -> ECS & {
    static ECS inst;
    return inst;
}

void ECS::syncClipIds(const QSet<int> &aliveIds) {
    auto &editState = m_buffers[m_editIndex];
    bool changed = false;
    changed |= editState.transforms.syncAlive(aliveIds);
    changed |= editState.renderStates.syncAlive(aliveIds);
    changed |= editState.audioStates.syncAlive(aliveIds);
    changed |= editState.metadataStates.syncAlive(aliveIds);

    if (changed) {
        m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
        m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
    }
}

void ECS::updateClipState(int clipId, int layer, int startFrame, int durationFrames, double timePosition) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &editState = m_buffers[m_editIndex];

    if (!editState.transforms.contains(clipId)) {
        m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
        m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
    }
    auto &transform = editState.transforms[clipId];

    bool changed = (transform.layer != layer || transform.startFrame != startFrame || transform.durationFrames != durationFrames || std::abs(transform.timePosition - timePosition) > 0.001);

    if (changed) {
        transform.clipId = clipId;
        transform.layer = layer;
        transform.startFrame = startFrame;
        transform.durationFrames = durationFrames;
        transform.timePosition = timePosition;

        auto &render = editState.renderStates[clipId];
        render.needsUpdate = true;
        editState.renderGraphDirty = true;

        m_dirtyForBuffer[(m_editIndex + 1) % 3].insert(clipId);
        m_dirtyForBuffer[(m_editIndex + 2) % 3].insert(clipId);
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &editState = m_buffers[m_editIndex];

    if (!editState.audioStates.contains(clipId)) {
        m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
        m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
    }
    auto &audio = editState.audioStates[clipId];
    audio.clipId = clipId;
    audio.startFrame = startFrame;
    audio.durationFrames = durationFrames;
    audio.volume = volume;
    audio.pan = pan;
    audio.mute = mute;

    // 音声パラメータは頻繁に変わる可能性があるためダーティフラグ処理
    m_dirtyForBuffer[(m_editIndex + 1) % 3].insert(clipId);
    m_dirtyForBuffer[(m_editIndex + 2) % 3].insert(clipId);
}

void ECS::removeClip(int clipId) {
    auto &editState = m_buffers[m_editIndex];
    editState.transforms.erase(clipId);
    editState.metadataStates.erase(clipId);
    editState.selections.erase(clipId);
    editState.effectStacks.erase(clipId);
    editState.audioStacks.erase(clipId);
    editState.audioStates.erase(clipId);
    editState.renderStates.erase(clipId);
    editState.renderGraphDirty = true;
    m_interpCache.remove(clipId);
    m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
    m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
}

void ECS::updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color) {
    auto &editState = m_buffers[m_editIndex];

    if (!editState.metadataStates.contains(clipId)) {
        m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
        m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
    }
    auto &meta = editState.metadataStates[clipId];

    if (meta.clipId != clipId || meta.name != name || meta.source != source || meta.type != type || meta.color != color) {
        meta.clipId = clipId;
        meta.name = name;
        meta.source = source;
        meta.type = type;
        meta.color = color;
        m_dirtyForBuffer[(m_editIndex + 1) % 3].insert(clipId);
        m_dirtyForBuffer[(m_editIndex + 2) % 3].insert(clipId);
    }
}

void ECS::updateEffectStack(int clipId, const QList<AviQtl::UI::EffectData> &effects) {
    auto &state = m_buffers[m_editIndex];

    // operator[] は要素がなければ新規作成し、あれば参照を返す (ensureSparseSize等も内部で実行される)
    state.effectStacks[clipId].effects = effects;

    // 他のバッファにも伝搬させるためのダーティフラグを立てる（必要であれば）
    m_dirtyForBuffer[(m_editIndex + 1) % 3].insert(clipId);
    m_dirtyForBuffer[(m_editIndex + 2) % 3].insert(clipId);
    state.renderGraphDirty = true;
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

    if (m_fullSyncRequired[m_editIndex]) {
        // 構造変更があった、または初期状態の場合はフルコピー
        m_buffers[m_editIndex] = m_buffers[nextActive];
        m_fullSyncRequired[m_editIndex] = false;
        m_dirtyForBuffer[m_editIndex].clear();
    } else {
        // 部分更新: 変更があったコンポーネントのみコピー
        const auto &src = m_buffers[nextActive];
        auto &dst = m_buffers[m_editIndex];

        dst.renderGraphDirty = src.renderGraphDirty;

        for (int id : m_dirtyForBuffer[m_editIndex]) {
            if (const auto *s = src.transforms.find(id)) {
                dst.transforms[id] = *s;
            }
            if (const auto *s = src.renderStates.find(id)) {
                dst.renderStates[id] = *s;
            }
            if (const auto *s = src.audioStates.find(id)) {
                dst.audioStates[id] = *s;
            }
            if (const auto *s = src.metadataStates.find(id)) {
                dst.metadataStates[id] = *s;
            }
            if (const auto *s = src.effectStacks.find(id)) {
                dst.effectStacks[id] = *s;
            }
            if (const auto *s = src.selections.find(id)) {
                dst.selections[id] = *s;
            }
            if (const auto *s = src.audioStacks.find(id)) {
                dst.audioStacks[id] = *s;
            }
        }
        m_dirtyForBuffer[m_editIndex].clear();
    }

    // 最新の active インデックスを公開
    m_activeIndex.store(nextActive, std::memory_order_release);
}

auto ECS::getSnapshot() const -> const ECSState * { return &m_buffers[m_activeIndex.load(std::memory_order_acquire)]; }

auto ECS::isRenderGraphDirty() const -> bool { return m_buffers[m_editIndex].renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers[m_editIndex].renderGraphDirty = false; }

void ECS::invalidateEffectCache(int clipId, int effectIndex, const QString &paramName) { m_interpCache.invalidateParam(clipId, effectIndex, paramName); }

void ECS::invalidateAllEffectCaches(int clipId) { m_interpCache.invalidateAll(clipId); }

} // namespace AviQtl::Engine::Timeline

extern "C" {
auto aviqtl_lua_get_audio_volume(int clipId) -> float {
    const auto *state = AviQtl::Engine::Timeline::ECS::instance().getSnapshot();
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
