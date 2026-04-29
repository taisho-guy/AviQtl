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

// フェーズ7: startFrame / durationFrames を TransformComponent に追加
// これにより GpuClipSoA の startFrames[] / durationFrames[] の正規のソースになる
// Phase 6 では TransformComponent.startFrame が常に 0 のまま writeSSBOLayout に
// 渡されていたため、シェーダー側で正しいタイミング計算ができていなかった
void ECS::updateClipState(int clipId, int layer, double time, int startFrame, int durationFrames) {
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];
    if (!editState.transforms.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
    auto &transform = editState.transforms[clipId];
    bool changed = (transform.layer != layer)
                || (std::abs(transform.timePosition - time) > 0.001)
                || (transform.startFrame != startFrame)
                || (transform.durationFrames != durationFrames);
    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;
        transform.startFrame = startFrame;
        transform.durationFrames = durationFrames;
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
    const int justWritten = m_editIndex;

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
        next = (pending != -1) ? pending : (justWritten + 1) % 3;
    }
    m_editIndex = next;

    if (m_dirtyFlags[m_editIndex].fullSync) {
        m_buffers[m_editIndex] = m_buffers[justWritten];
        m_dirtyFlags[m_editIndex].fullSync = false;
        m_dirtyFlags[m_editIndex].dirty.reset();
    } else {
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

    // フェーズ5: mailbox に deposit
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

// フェーズ7: GpuTransformSoA + GpuAudioSoA を廃止し GpuClipSoA 1本に統合
// 変更点:
//   引数: (GpuTransformSoA&, GpuAudioSoA&) → (GpuClipSoA&)
//   SSBO 本数: 2本 → 1本 (SSBO_BINDING_CLIP = 0)
//   startFrames / durationFrames の重複を除去 (2,064 bytes 削減)
//   transforms.forEach を一次ループとし、同 clipId の AudioComponent を find() で結合
//   find() は DenseComponentMap の O(1) sparse lookup のため全体は O(k) を維持
void ECS::writeSSBOLayout(GpuClipSoA &out) const {
    ECS_PROF_INC(ssboWriteCount);

    const ECSState *state = getSnapshot();

    out.count = 0;
    state->transforms.forEach([&](int clipId, const TransformComponent &tc) {
        if (out.count >= MAX_ACTIVE_CLIPS)
            return;
        const int idx = out.count++;

        out.clipIds[idx]        = static_cast<int32_t>(clipId);
        out.layers[idx]         = static_cast<int32_t>(tc.layer);
        out.timePositions[idx]  = static_cast<float>(tc.timePosition);
        // タイミング情報: TransformComponent が正規のソース (Phase 6 のバグ修正)
        out.startFrames[idx]    = static_cast<int32_t>(tc.startFrame);
        out.durationFrames[idx] = static_cast<int32_t>(tc.durationFrames);

        // Audio フィールド: AudioComponent が存在する場合のみ転写
        // 存在しない場合 (テキスト・画像・矩形等) はデフォルト値を維持
        if (const auto *ac = state->audioStates.find(clipId)) {
            out.volumes[idx] = ac->volume;
            out.pans[idx]    = ac->pan;
            out.mutes[idx]   = ac->mute ? 1 : 0;
        } else {
            out.volumes[idx] = 1.0f;
            out.pans[idx]    = 0.0f;
            out.mutes[idx]   = 0;
        }
    });
}

auto ECS::isRenderGraphDirty() const -> bool { return m_buffers[m_editIndex].renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers[m_editIndex].renderGraphDirty = false; }

} // namespace AviQtl::Engine::Timeline
