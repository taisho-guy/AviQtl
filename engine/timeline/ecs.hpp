#pragma once
#include <QSet>
#include <QString>
#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace Rina::Engine::Timeline {

// Sparse Set (疎集合) パターンによるデータ指向コンテナ
template <typename T> class DenseComponentMap {
  public:
    struct Entry {
        int first = -1;
        T second{};
    };

    using Storage = std::vector<Entry>;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    T &operator[](int clipId) {
        ensureSparseSize(clipId);
        int &denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex >= 0)
            return m_dense[static_cast<std::size_t>(denseIndex)].second;

        denseIndex = static_cast<int>(m_dense.size());
        m_dense.push_back(Entry{clipId, T{}});
        return m_dense.back().second;
    }

    iterator find(int clipId) {
        if (clipId < 0 || clipId >= static_cast<int>(m_sparse.size()))
            return m_dense.end();
        const int denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex < 0)
            return m_dense.end();
        return m_dense.begin() + denseIndex;
    }

    const_iterator find(int clipId) const {
        if (clipId < 0 || clipId >= static_cast<int>(m_sparse.size()))
            return m_dense.end();
        const int denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex < 0)
            return m_dense.end();
        return m_dense.begin() + denseIndex;
    }

    iterator erase(iterator it) {
        if (it == m_dense.end())
            return it;

        const int denseIndex = static_cast<int>(std::distance(m_dense.begin(), it));
        const int removedClipId = m_dense[static_cast<std::size_t>(denseIndex)].first;
        const int lastIndex = static_cast<int>(m_dense.size()) - 1;

        if (denseIndex != lastIndex) {
            m_dense[static_cast<std::size_t>(denseIndex)] = std::move(m_dense[static_cast<std::size_t>(lastIndex)]);
            const int movedClipId = m_dense[static_cast<std::size_t>(denseIndex)].first;
            m_sparse[static_cast<std::size_t>(movedClipId)] = denseIndex;
        }

        m_dense.pop_back();
        if (removedClipId >= 0 && removedClipId < static_cast<int>(m_sparse.size()))
            m_sparse[static_cast<std::size_t>(removedClipId)] = -1;

        if (denseIndex >= static_cast<int>(m_dense.size()))
            return m_dense.end();
        return m_dense.begin() + denseIndex;
    }

    void syncAlive(const QSet<int> &aliveIds) {
        for (int i = static_cast<int>(m_dense.size()) - 1; i >= 0; --i) {
            if (!aliveIds.contains(m_dense[static_cast<std::size_t>(i)].first))
                erase(m_dense.begin() + i);
        }
    }

    iterator begin() { return m_dense.begin(); }
    iterator end() { return m_dense.end(); }
    const_iterator begin() const { return m_dense.begin(); }
    const_iterator end() const { return m_dense.end(); }
    const_iterator cbegin() const { return m_dense.cbegin(); }
    const_iterator cend() const { return m_dense.cend(); }

  private:
    void ensureSparseSize(int clipId) {
        if (clipId < 0)
            return;
        const std::size_t needed = static_cast<std::size_t>(clipId) + 1;
        if (m_sparse.size() < needed)
            m_sparse.resize(needed, -1);
    }

    Storage m_dense;
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

struct RenderComponent {
    bool needsUpdate = true;
    QString effectChain;
};

struct ECSState {
    bool renderGraphDirty = false;
    DenseComponentMap<TransformComponent> transforms;
    DenseComponentMap<RenderComponent> renderStates;
    DenseComponentMap<AudioComponent> audioStates;
};

class ECS {
  public:
    static ECS &instance();

    void syncClipIds(const QSet<int> &aliveIds);
    void updateClipState(int clipId, int layer, double time);
    void updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute);

    void commit();

    // スナップショットのポインタを返す。戻り値は呼び出し側スレッドで安全に利用できる。
    const ECSState *getSnapshot() const;

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
};

} // namespace Rina::Engine::Timeline
