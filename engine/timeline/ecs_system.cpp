#include "ecs.hpp"
#include <QDebug>
#include <cmath>

namespace Rina::Engine::Timeline {

ECS::ECS() {
    m_activeIndex.store(0, std::memory_order_relaxed);
    m_editIndex = 1;
    m_fullSyncRequired.fill(true);
}

ECS &ECS::instance() {
    static ECS inst;
    return inst;
}

void ECS::syncClipIds(const QSet<int> &aliveIds) {
    auto &editState = m_buffers[m_editIndex];
    bool changed = false;

    // 削除されるIDを特定
    QSet<int> deletedIds;
    for (const auto &entry : editState.transforms) {
        if (!aliveIds.contains(entry.first))
            deletedIds.insert(entry.first);
    }

    changed |= editState.transforms.syncAlive(aliveIds);
    changed |= editState.renderStates.syncAlive(aliveIds);
    changed |= editState.audioStates.syncAlive(aliveIds);
    changed |= editState.metadataStates.syncAlive(aliveIds);

    if (changed && !deletedIds.isEmpty()) {
        for (int i = 0; i < 3; ++i)
            if (i != m_editIndex)
                m_dirtyForBuffer[i].unite(deletedIds);
    }
}

void ECS::updateClipState(int clipId, int layer, double time) {
    auto &editState = m_buffers[m_editIndex];

    if (!editState.transforms.contains(clipId)) {
        m_fullSyncRequired[(m_editIndex + 1) % 3] = true;
        m_fullSyncRequired[(m_editIndex + 2) % 3] = true;
    }
    auto &transform = editState.transforms[clipId];

    bool changed = (transform.layer != layer || std::abs(transform.timePosition - time) > 0.001);

    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;

        auto &render = editState.renderStates[clipId];
        render.needsUpdate = true;
        editState.renderGraphDirty = true;

        m_dirtyForBuffer[m_editIndex].insert(clipId);
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) {
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
    m_dirtyForBuffer[m_editIndex].insert(clipId);
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
        m_dirtyForBuffer[m_editIndex].insert(clipId);
    }
}

void ECS::commit() {
    // 現在の edit バッファのインデックスを新しい active バッファにする
    int lastEditIndex = m_editIndex;

    // 次の edit バッファを選ぶ (0, 1, 2 を循環)
    m_editIndex = (m_editIndex + 1) % 3;

    // 次の edit バッファが現在 active として使われているなら、もう一つ進める
    if (m_editIndex == m_activeIndex.load(std::memory_order_acquire)) {
        m_editIndex = (m_editIndex + 1) % 3;
    }

    auto &src = m_buffers[lastEditIndex];
    auto &dst = m_buffers[m_editIndex];

    // 重要：受取側（次の編集バッファ）のDirtyリストをリセット
    // これにより、以前のサイクルで残った古いDirtyフラグがゾンビ化するのを防ぐ
    m_dirtyForBuffer[m_editIndex].clear();

    if (m_fullSyncRequired[m_editIndex]) {
        // 構造変更があった、または初期状態の場合はフルコピー
        dst = src;
        m_fullSyncRequired[m_editIndex] = false;

        // フルコピーした場合でも、lastEditIndexで発生した変更（Dirty）は
        // このバッファ（m_editIndex）を経由してさらに次のバッファへ伝播させる必要があるため
        // Dirtyフラグを引き継ぐ
        for (int id : m_dirtyForBuffer[lastEditIndex]) {
            m_dirtyForBuffer[m_editIndex].insert(id);
        }
    } else {
        // 部分更新: 変更があったコンポーネントのみコピー
        dst.renderGraphDirty = src.renderGraphDirty;

        // 直前の編集（lastEditIndex）で変更された内容を、
        // 次の編集用バッファ（m_editIndex）に同期する
        for (int id : m_dirtyForBuffer[lastEditIndex]) {
            if (src.transforms.contains(id))
                dst.transforms[id] = src.transforms[id];
            else
                dst.transforms.remove(id);

            if (src.renderStates.contains(id))
                dst.renderStates[id] = src.renderStates[id];
            else
                dst.renderStates.remove(id);

            if (src.audioStates.contains(id))
                dst.audioStates[id] = src.audioStates[id];
            else
                dst.audioStates.remove(id);

            if (src.metadataStates.contains(id))
                dst.metadataStates[id] = src.metadataStates[id];
            else
                dst.metadataStates.remove(id);

            // 3つ目のバッファへの伝播のため、Dirtyフラグを累積させる
            m_dirtyForBuffer[m_editIndex].insert(id);
        }
    }

    // 役割を終えたバッファのDirtyをクリア
    m_dirtyForBuffer[lastEditIndex].clear();

    // 最新の active インデックスを公開
    m_activeIndex.store(lastEditIndex, std::memory_order_release);
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
