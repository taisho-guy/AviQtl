#include "ecs.hpp"
#include "ecs_profiler.hpp"
#include <QDebug>
#include <cassert>
#include <cmath>

namespace AviQtl::Engine::Timeline {

ECS::ECS() : m_editIndex(1) {
    m_activeIndex.store(0, std::memory_order_relaxed);
    for (auto &f : m_dirtyFlags)
        f.fullSync = true;
}

auto ECS::instance() -> ECS & {
    static ECS inst;
    return inst;
}

// フェーズ1: std::bitset<MAX_CLIP_ID> → O(1)L1直撃
void ECS::syncClipIds(const std::bitset<MAX_CLIP_ID> &aliveFlags) {
    auto &editState = m_buffers[m_editIndex];
    bool changed = false;
    changed |= editState.transforms.syncAlive(aliveFlags);
    changed |= editState.renderStates.syncAlive(aliveFlags);
    changed |= editState.audioStates.syncAlive(aliveFlags);
    changed |= editState.metadataStates.syncAlive(aliveFlags);
    if (changed) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
}

void ECS::updateClipState(int clipId, int layer, double time) {
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];
    if (!editState.transforms.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
    auto &transform = editState.transforms[clipId];
    bool changed = (transform.layer != layer) || (std::abs(transform.timePosition - time) > 0.001);
    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;
        auto &render = editState.renderStates[clipId];
        render.needsUpdate = true;
        editState.renderGraphDirty = true;
    }
    const auto cidx = static_cast<std::size_t>(clipId);
    m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
    m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
    ECS_PROF_INC(dirtyBitSetCount);
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, // NOLINT
                               float volume, float pan, bool mute) {
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];
    if (!editState.audioStates.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
    auto &audio = editState.audioStates[clipId];
    audio.clipId = clipId;
    audio.startFrame = startFrame;
    audio.durationFrames = durationFrames;
    audio.volume = volume;
    audio.pan = pan;
    audio.mute = mute;
    const auto cidx = static_cast<std::size_t>(clipId);
    m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
    m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
    ECS_PROF_INC(dirtyBitSetCount);
}

// フェーズ2: QString → StringTable ID + SoA uint32_t
static auto parseColorRGBA(const QString &colorStr) -> uint32_t {
    QString s = colorStr.trimmed();
    if (s.startsWith(u'#'))
        s.remove(0, 1);
    bool ok = false;
    const uint32_t val = s.toUInt(&ok, 16);
    if (!ok)
        return 0xFF000000u;
    if (s.length() == 6)
        return 0xFF000000u | val;
    return val;
}

void ECS::updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color) {
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];
    if (!editState.metadataStates.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
    const uint32_t nId = m_stringTable.intern(name.toStdString());
    const uint32_t sId = m_stringTable.intern(source.toStdString());
    const uint32_t tId = m_stringTable.intern(type.toStdString());
    const uint32_t cRGBA = parseColorRGBA(color);
    auto &meta = editState.metadataStates[clipId];
    if (meta.clipId != static_cast<int32_t>(clipId) || meta.nameId != nId || meta.sourceId != sId || meta.typeId != tId || meta.colorRGBA != cRGBA) {
        meta.clipId = static_cast<int32_t>(clipId);
        meta.nameId = nId;
        meta.sourceId = sId;
        meta.typeId = tId;
        meta.colorRGBA = cRGBA;
    }
    const auto cidx = static_cast<std::size_t>(clipId);
    m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
    m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
}

void ECS::commit() {
    ECS_PROF_INC(commitCount);

    // フェーズ5: mailbox パターンによるバッファ競合排除
    // commit() は m_pendingIndex に deposit するのみ
    // m_activeIndex の昇格は render thread 側の getSnapshot() が担う
    const int justWritten = m_editIndex;

    // 次の edit バッファを選択: active と pending の両方を避ける
    const int active = m_activeIndex.load(std::memory_order_acquire);
    const int pending = m_pendingIndex.load(std::memory_order_acquire);

    int next = -1;
    for (int c = 0; c < 3; ++c) {
        if (c == justWritten || c == active || c == pending)
            continue;
        next = c;
        break;
    }
    if (next == -1) {
        // 全バッファ使用中: UI が render より高速な場合 (pending を上書きしてフレームスキップ)
        next = (pending != -1) ? pending : (justWritten + 1) % 3;
    }
    m_editIndex = next;

    if (m_dirtyFlags[m_editIndex].fullSync) {
        // 構造変更があった、または初期状態の場合はフルコピー
        m_buffers[m_editIndex] = m_buffers[justWritten];
        m_dirtyFlags[m_editIndex].fullSync = false;
        m_dirtyFlags[m_editIndex].dirty.reset();
    } else {
        // 部分更新: フェーズ1の bitset 走査 (4096bit = L1キャッシュ 8ライン)
        const auto &src = m_buffers[justWritten];
        auto &dst = m_buffers[m_editIndex];
        dst.renderGraphDirty = src.renderGraphDirty;
        const auto &dirtyBits = m_dirtyFlags[m_editIndex].dirty;
        for (std::size_t i = 0; i < MAX_CLIP_ID; ++i) {
            if (!dirtyBits.test(i))
                continue;
            const int id = static_cast<int>(i);
            if (const auto *s = src.transforms.find(id))
                dst.transforms[id] = *s;
            if (const auto *s = src.renderStates.find(id))
                dst.renderStates[id] = *s;
            if (const auto *s = src.audioStates.find(id))
                dst.audioStates[id] = *s;
            if (const auto *s = src.metadataStates.find(id))
                dst.metadataStates[id] = *s;
        }
        m_dirtyFlags[m_editIndex].dirty.reset();
    }

    // フェーズ5: mailbox に deposit (m_activeIndex は触らない)
    m_pendingIndex.store(justWritten, std::memory_order_release);
}

auto ECS::getSnapshot() const -> const ECSState * {
    // フェーズ5: mailbox から最新バッファを取り出して active に昇格 (lazy swap)
    int pending = m_pendingIndex.load(std::memory_order_acquire);
    if (pending != -1) {
        if (m_pendingIndex.compare_exchange_strong(pending, -1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            m_activeIndex.store(pending, std::memory_order_release);
        }
    }
    return &m_buffers[m_activeIndex.load(std::memory_order_acquire)];
}

// フェーズ6: ECS スナップショット → SoA フラット配列への直接展開
// QVariantMap / QByteArray を一切経由しない zero-copy パス
void ECS::writeSSBOLayout(GpuTransformSoA &tfm, GpuAudioSoA &aud) const {
    ECS_PROF_INC(ssboWriteCount);

    const ECSState *state = getSnapshot();

    // TransformComponent → GpuTransformSoA
    tfm.count = 0;
    state->transforms.forEach([&](int clipId, const TransformComponent &tc) {
        if (tfm.count >= MAX_ACTIVE_CLIPS)
            return;
        const int idx = tfm.count++;
        tfm.clipIds[idx] = static_cast<int32_t>(clipId);
        tfm.layers[idx] = static_cast<int32_t>(tc.layer);
        tfm.timePositions[idx] = static_cast<float>(tc.timePosition);
        tfm.startFrames[idx] = static_cast<int32_t>(tc.startFrame);
        tfm.durationFrames[idx] = static_cast<int32_t>(tc.durationFrames);
    });

    // AudioComponent → GpuAudioSoA
    aud.count = 0;
    state->audioStates.forEach([&](int /*clipId*/, const AudioComponent &ac) {
        if (aud.count >= MAX_ACTIVE_CLIPS)
            return;
        const int idx = aud.count++;
        aud.volumes[idx] = ac.volume;
        aud.pans[idx] = ac.pan;
        aud.mutes[idx] = ac.mute ? 1 : 0;
        aud.startFrames[idx] = static_cast<int32_t>(ac.startFrame);
        aud.durationFrames[idx] = static_cast<int32_t>(ac.durationFrames);
    });
}

auto ECS::isRenderGraphDirty() const -> bool { return m_buffers[m_editIndex].renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers[m_editIndex].renderGraphDirty = false; }

} // namespace AviQtl::Engine::Timeline
