#include "ecs.hpp"
#include "ecs_profiler.hpp"
#include <QDebug>
#include <cassert>
#include <cmath>

namespace AviQtl::Engine::Timeline {

ECS::ECS() : m_editIndex(1) {
    m_activeIndex.store(0, std::memory_order_relaxed);
    // フェーズ1: m_fullSyncRequired.fill(true) → DirtyFlags::fullSync を全バッファにセット
    for (auto &f : m_dirtyFlags) {
        f.fullSync = true;
    }
}

auto ECS::instance() -> ECS & {
    static ECS inst;
    return inst;
}

// フェーズ1: 引数を std::bitset<MAX_CLIP_ID> に変更
// QSet::contains() → bitset::test() でキャッシュライン直撃
void ECS::syncClipIds(const std::bitset<MAX_CLIP_ID> &aliveFlags) {
    auto &editState = m_buffers[m_editIndex];
    bool changed = false;
    changed |= editState.transforms.syncAlive(aliveFlags);
    changed |= editState.renderStates.syncAlive(aliveFlags);
    changed |= editState.audioStates.syncAlive(aliveFlags);
    changed |= editState.metadataStates.syncAlive(aliveFlags);

    if (changed) {
        // フェーズ1: m_fullSyncRequired[X] = true → m_dirtyFlags[X].fullSync = true
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
}

void ECS::updateClipState(int clipId, int layer, double time) { // NOLINT(bugprone-easily-swappable-parameters)
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];

    if (!editState.transforms.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }
    auto &transform = editState.transforms[clipId];

    bool changed = (transform.layer != layer || std::abs(transform.timePosition - time) > 0.001);

    if (changed) {
        transform.layer = layer;
        transform.timePosition = time;

        auto &render = editState.renderStates[clipId];
        render.needsUpdate = true;
        editState.renderGraphDirty = true;

        // フェーズ1: QSet::insert → bitset::set
        const auto cidx = static_cast<std::size_t>(clipId);
        m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
        m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
        ECS_PROF_INC(dirtyBitSetCount);
    }
}

void ECS::updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute) { // NOLINT(bugprone-easily-swappable-parameters)
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

    // 音声パラメータは頻繁に変わる可能性があるためダーティフラグ処理
    const auto cidx = static_cast<std::size_t>(clipId);
    m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
    m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
    ECS_PROF_INC(dirtyBitSetCount);
}

// フェーズ2: "#RRGGBB" / "#AARRGGBB" を ARGB uint32_t にパック
// Qt依存を最小化するため QString で受けて変換のみ行う
static auto parseColorRGBA(const QString &colorStr) -> uint32_t {
    QString s = colorStr.trimmed();
    if (s.startsWith(u'#')) {
        s.remove(0, 1);
    }
    bool ok = false;
    const uint32_t val = s.toUInt(&ok, 16);
    if (!ok) {
        return 0xFF000000u; // 不明な場合は不透明黒
    }
    if (s.length() == 6) {
        return 0xFF000000u | val; // RRGGBB → AARRGGBB
    }
    return val; // AARRGGBB
}

void ECS::updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color) {
    assert(clipId >= 0 && clipId < MAX_CLIP_ID);
    auto &editState = m_buffers[m_editIndex];

    if (!editState.metadataStates.contains(clipId)) {
        m_dirtyFlags[(m_editIndex + 1) % 3].fullSync = true;
        m_dirtyFlags[(m_editIndex + 2) % 3].fullSync = true;
    }

    // フェーズ2: QString → StringTable ID に変換してからSoAへ書き込む
    // これにより比較コストが uint32_t == uint32_t のみになる
    const uint32_t nId = m_stringTable.intern(name.toStdString());
    const uint32_t sId = m_stringTable.intern(source.toStdString());
    const uint32_t tId = m_stringTable.intern(type.toStdString());
    const uint32_t cRGBA = parseColorRGBA(color);

    auto &meta = editState.metadataStates[clipId];

    if (meta.clipId != clipId || meta.nameId != nId || meta.sourceId != sId || meta.typeId != tId || meta.colorRGBA != cRGBA) {
        meta.clipId = static_cast<int32_t>(clipId);
        meta.nameId = nId;
        meta.sourceId = sId;
        meta.typeId = tId;
        meta.colorRGBA = cRGBA;
        const auto cidx = static_cast<std::size_t>(clipId);
        m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(cidx);
        m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(cidx);
    }
}

void ECS::commit() {
    ECS_PROF_INC(commitCount);

    // 現在の edit バッファのインデックスを新しい active バッファにする
    int nextActive = m_editIndex;

    // 次の edit バッファを選ぶ (0, 1, 2 を循環)
    m_editIndex = (m_editIndex + 1) % 3;

    // 次の edit バッファが現在 active として使われているなら、もう一つ進める
    if (m_editIndex == m_activeIndex.load(std::memory_order_acquire)) {
        m_editIndex = (m_editIndex + 1) % 3;
    }

    // フェーズ1: m_fullSyncRequired[X] → m_dirtyFlags[X].fullSync
    if (m_dirtyFlags[m_editIndex].fullSync) {
        // 構造変更があった、または初期状態の場合はフルコピー
        m_buffers[m_editIndex] = m_buffers[nextActive];
        m_dirtyFlags[m_editIndex].fullSync = false;
        m_dirtyFlags[m_editIndex].dirty.reset();
    } else {
        // 部分更新: 変更があったコンポーネントのみコピー
        // フェーズ1: for (int id : QSet) → bitset走査
        // 4096bit = 512byte = 8キャッシュライン: 全走査でもL1完結
        const auto &src = m_buffers[nextActive];
        auto &dst = m_buffers[m_editIndex];

        dst.renderGraphDirty = src.renderGraphDirty;

        const auto &dirtyBits = m_dirtyFlags[m_editIndex].dirty;
        for (std::size_t i = 0; i < MAX_CLIP_ID; ++i) {
            if (!dirtyBits.test(i))
                continue;
            const int id = static_cast<int>(i);
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
        }
        m_dirtyFlags[m_editIndex].dirty.reset();
    }

    // 最新の active インデックスを公開
    m_activeIndex.store(nextActive, std::memory_order_release);
}

auto ECS::getSnapshot() const -> const ECSState * { return &m_buffers[m_activeIndex.load(std::memory_order_acquire)]; }

auto ECS::isRenderGraphDirty() const -> bool { return m_buffers[m_editIndex].renderGraphDirty; }

void ECS::markRenderGraphClean() { m_buffers[m_editIndex].renderGraphDirty = false; }

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
