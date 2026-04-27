#pragma once
#include "ecs_profiler.hpp"
#include "string_table.hpp"
#include <QString>
#include <QVariantMap>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace AviQtl::Engine::Timeline {

// フェーズ1: ClipIDの最大値を定数化し、QSet<int>を排除する
// ClipIDが0以上この値未満であることをassertで保証する
inline constexpr int MAX_CLIP_ID = 4096;

// フェーズ1: ダーティ追跡をbitset+bool1個に統合
// QSet<int> + bool の2構造 → DirtyFlags 1構造へ
// 512バイト(=4096bit) のbitsetはL1キャッシュ8ライン相当でアクセス効率が高い
struct DirtyFlags {
    std::bitset<MAX_CLIP_ID> dirty;
    bool fullSync = false;
};

// Sparse Set (疎集合) パターンによるデータ指向コンテナ
template <typename T> class DenseComponentMap {
  public:
    T &operator[](int clipId) {
        ensureSparseSize(clipId);
        int &denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex >= 0) {
            ECS_PROF_INC(denseMapHit);
            return m_data[static_cast<std::size_t>(denseIndex)];
        }
        ECS_PROF_INC(denseMapMiss);
        denseIndex = static_cast<int>(m_data.size());
        m_entities.push_back(clipId);
        m_data.push_back(T{});
        return m_data.back();
    }

    T *find(int clipId) {
        if (clipId < 0 || clipId >= static_cast<int>(m_sparse.size()))
            return nullptr;
        const int denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex < 0)
            return nullptr;
        return &m_data[static_cast<std::size_t>(denseIndex)];
    }

    const T *find(int clipId) const { return const_cast<DenseComponentMap *>(this)->find(clipId); }

    void erase(int clipId) {
        if (clipId < 0 || clipId >= static_cast<int>(m_sparse.size()))
            return;
        int denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex < 0)
            return;

        int lastIndex = static_cast<int>(m_data.size()) - 1;
        if (denseIndex != lastIndex) {
            m_data[static_cast<std::size_t>(denseIndex)] = std::move(m_data[static_cast<std::size_t>(lastIndex)]);
            int movedClipId = m_entities[static_cast<std::size_t>(lastIndex)];
            m_entities[static_cast<std::size_t>(denseIndex)] = movedClipId;
            m_sparse[static_cast<std::size_t>(movedClipId)] = denseIndex;
        }

        m_data.pop_back();
        m_entities.pop_back();
        m_sparse[static_cast<std::size_t>(clipId)] = -1;
    }

    // フェーズ1: 引数をbitsetに変更 → QSet::contains() O(1)hash → bitset::test() O(1)L1直撃
    bool syncAlive(const std::bitset<MAX_CLIP_ID> &aliveFlags) {
        bool changed = false;
        for (int i = static_cast<int>(m_entities.size()) - 1; i >= 0; --i) {
            int id = m_entities[static_cast<std::size_t>(i)];
            // idがMAX_CLIP_ID未満かつaliveでなければ削除
            if (id >= MAX_CLIP_ID || !aliveFlags.test(static_cast<std::size_t>(id))) {
                erase(id);
                changed = true;
                ECS_PROF_INC(syncAliveRemoved);
            }
        }
        return changed;
    }

    // データ指向アクセス用のイテレータ（ポインタ）
    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() { return m_data.empty() ? nullptr : &m_data[0]; }
    iterator end() { return m_data.empty() ? nullptr : &m_data[0] + m_data.size(); }
    const_iterator begin() const { return m_data.empty() ? nullptr : &m_data[0]; }
    const_iterator end() const { return m_data.empty() ? nullptr : &m_data[0] + m_data.size(); }

    bool contains(int clipId) const {
        if (clipId < 0 || clipId >= static_cast<int>(m_sparse.size()))
            return false;
        return m_sparse[static_cast<std::size_t>(clipId)] != -1;
    }

  private:
    void ensureSparseSize(int clipId) {
        if (clipId < 0)
            return;
        const std::size_t needed = static_cast<std::size_t>(clipId) + 1;
        if (m_sparse.size() < needed)
            m_sparse.resize(needed, -1);
    }

    std::vector<int> m_entities; // Dense IDs
    std::vector<T> m_data;       // Dense Components
    std::vector<int> m_sparse;
};

struct AudioComponent {
    int clipId = -1;
    int startFrame = 0;
    int durationFrames = 0;
    float volume = 1.0f;
    float pan = 0.0f;
    bool mute = false;
};

struct TransformComponent {
    int layer = 0;
    double timePosition = 0.0;
    int startFrame = 0;
    int durationFrames = 0;
};

// フェーズ2: QString を排除し trivially_copyable な POD 構造体へ
// name/source/type は StringTable のIDで保持
// color は "#RRGGBB"/"#AARRGGBB" を uint32_t ARGB にパック
struct MetadataComponent {
    int32_t clipId = -1;
    uint32_t nameId = 0;    // StringTable index
    uint32_t sourceId = 0;  // StringTable index
    uint32_t typeId = 0;    // StringTable index
    uint32_t colorRGBA = 0; // ARGB packed
};
static_assert(sizeof(MetadataComponent) == 20, "MetadataComponent size check failed");
static_assert(std::is_trivially_copyable_v<MetadataComponent>, "MetadataComponent must be trivially copyable");

struct RenderComponent {
    bool needsUpdate = true;
    QString effectChain;
};

struct ECSState {
    bool renderGraphDirty = false;
    DenseComponentMap<TransformComponent> transforms;
    DenseComponentMap<RenderComponent> renderStates;
    DenseComponentMap<AudioComponent> audioStates;
    DenseComponentMap<MetadataComponent> metadataStates;
};

class ECS {
  public:
    static ECS &instance();

    // フェーズ1: QSet<int> → std::bitset<MAX_CLIP_ID>
    void syncClipIds(const std::bitset<MAX_CLIP_ID> &aliveFlags);
    void updateClipState(int clipId, int layer, double time);
    void updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute);
    void updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color);

    void commit();

    ECSState &editState() { return m_buffers[m_editIndex]; }

    // フェーズ1: QSet::insert → bitset::set
    void markEvaluatedParamsDirty(int clipId) {
        assert(clipId >= 0 && clipId < MAX_CLIP_ID);
        m_dirtyFlags[(m_editIndex + 1) % 3].dirty.set(static_cast<std::size_t>(clipId));
        m_dirtyFlags[(m_editIndex + 2) % 3].dirty.set(static_cast<std::size_t>(clipId));
    }

    const ECSState *getSnapshot() const;

    // フェーズ2: StringTableへの読み取りアクセス（レンダラー/Lua向け）
    const StringTable &stringTable() const { return m_stringTable; }

    bool isRenderGraphDirty() const;
    void markRenderGraphClean();

  private:
    ECS();

    // トリプルバッファリングのための固定長配列
    std::array<ECSState, 3> m_buffers;

    // 現在UIスレッドが書き込んでいるバッファのインデックス
    int m_editIndex = 0;

    // バックグラウンドスレッドが現在読み取っている(最新の確定済み)バッファのインデックス
    std::atomic<int> m_activeIndex{0};

    // フェーズ2: 文字列インターンプール（SoA内から heap 文字列を排除）
    StringTable m_stringTable;

    // フェーズ1: QSet<int> x3 + bool x3 → DirtyFlags x3 に統合
    // DirtyFlags: { bitset<4096> dirty; bool fullSync; }
    std::array<DirtyFlags, 3> m_dirtyFlags;
};

} // namespace AviQtl::Engine::Timeline
