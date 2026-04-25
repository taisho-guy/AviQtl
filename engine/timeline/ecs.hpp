#pragma once
#include "effect_data.hpp"
#include "timeline_types.hpp"
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace AviQtl::Engine::Timeline {

// Sparse Set (疎集合) パターンによるデータ指向コンテナ
template <typename T> class DenseComponentMap {
  public:
    T &operator[](int clipId) {
        ensureSparseSize(clipId);
        int &denseIndex = m_sparse[static_cast<std::size_t>(clipId)];
        if (denseIndex >= 0)
            return m_data[static_cast<std::size_t>(denseIndex)];

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

    bool syncAlive(const QSet<int> &aliveIds) {
        bool changed = false;
        for (int i = static_cast<int>(m_entities.size()) - 1; i >= 0; --i) {
            int id = m_entities[static_cast<std::size_t>(i)];
            if (!aliveIds.contains(id)) {
                erase(id);
                changed = true;
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
    int clipId = -1; // Added for collision checking and DOD iteration
    int layer = 0;
    double timePosition = 0.0;
    int startFrame = 0;
    int durationFrames = 0;
};

struct MetadataComponent {
    int clipId = -1;
    QString name;
    QString source;
    QString type;
    QString color;
};

struct RenderComponent {
    bool needsUpdate = true;
    QString effectChain;
};

struct SelectionComponent {
    int clipId = -1;
    bool isSelected = false;
};

// 前方宣言または不完全型の回避のため、実体が定義されている timeline_types.hpp をインクルードするか、void* で保持する手もあるが、
// プロジェクトの構造上 QList<T> は前方宣言でもポインタなら持てる。
// 実際の利用時には effect_model.hpp などが必要。
struct EffectStackComponent {
    int clipId = -1;
    QList<AviQtl::UI::EffectData> effects;
};

// エフェクト 1 つ分の補間キャッシュ（paramName → フラットポイント列）
struct InterpEffectCache {
    // paramName → flattenStructuredTrack() の結果（ソート済みポイント列）
    QHash<QString, QVariantList> resolvedTracks;
    // このキャッシュを構築した時の durationFrames
    // durationFrames が変化したらキャッシュ全体を無効化する
    int lastDuration = -1;
};

// clipId 1 つ分の補間キャッシュ（EffectStackComponent::effects と 1:1 対応）
struct InterpolationCacheComponent {
    int clipId = -1;
    QList<InterpEffectCache> perEffect;
};

// ── トリプルバッファ外のキャッシュストア
// ECSState には含まれず commit() のコピー対象にならない。
// ECS シングルトンが直接所有する。
class InterpolationCache {
  public:
    // clipId × effectIndex のキャッシュを取得（なければ nullptr）
    InterpEffectCache *findEffect(int clipId, int effectIndex) noexcept {
        auto it = m_store.find(clipId);
        if (it == m_store.end())
            return nullptr;
        if (effectIndex < 0 || effectIndex >= it->perEffect.size())
            return nullptr;
        return &it->perEffect[effectIndex];
    }

    // clipId × effectIndex のキャッシュを取得（なければ生成）
    // effectsCount: EffectStackComponent::effects.size() を渡す
    InterpEffectCache &getOrCreate(int clipId, int effectIndex, int effectsCount) {
        auto &comp = m_store[clipId];
        comp.clipId = clipId;
        if (comp.perEffect.size() != effectsCount)
            comp.perEffect.resize(effectsCount);
        return comp.perEffect[effectIndex];
    }

    // 単一パラメータを無効化（setKeyframe / removeKeyframe 後）
    void invalidateParam(int clipId, int effectIndex, const QString &paramName) noexcept {
        auto it = m_store.find(clipId);
        if (it == m_store.end())
            return;
        if (effectIndex < 0 || effectIndex >= it->perEffect.size())
            return;
        it->perEffect[effectIndex].resolvedTracks.remove(paramName);
    }

    // クリップの全エフェクトキャッシュを無効化（エフェクト追加・削除・並び替え後）
    void invalidateAll(int clipId) noexcept {
        auto it = m_store.find(clipId);
        if (it == m_store.end())
            return;
        for (auto &e : it->perEffect)
            e.resolvedTracks.clear();
    }

    // クリップ削除時にエントリ自体を除去
    void remove(int clipId) { m_store.remove(clipId); }

    // effects サイズ変化に追従（addEffect / restoreEffect 等）
    void resize(int clipId, int newSize) {
        m_store[clipId].clipId = clipId;
        m_store[clipId].perEffect.resize(newSize);
    }

    // 特定インデックスのエントリを削除（removeEffect 後）
    void removeAt(int clipId, int effectIndex) {
        auto it = m_store.find(clipId);
        if (it == m_store.end())
            return;
        if (effectIndex < 0 || effectIndex >= it->perEffect.size())
            return;
        it->perEffect.removeAt(effectIndex);
    }

    // エントリを移動（reorderEffects 後）
    void move(int clipId, int oldIndex, int newIndex) {
        auto it = m_store.find(clipId);
        if (it == m_store.end())
            return;
        if (oldIndex < 0 || oldIndex >= it->perEffect.size())
            return;
        if (newIndex < 0 || newIndex >= it->perEffect.size())
            return;
        it->perEffect.move(oldIndex, newIndex);
    }

    // 全エントリをリセット（applyPermutation 後の安全クリア）
    void resetAll(int clipId, int effectsCount) {
        auto &comp = m_store[clipId];
        comp.clipId = clipId;
        comp.perEffect.assign(effectsCount, InterpEffectCache{});
    }

  private:
    QHash<int, InterpolationCacheComponent> m_store;
};

struct AudioStackComponent {
    int clipId = -1;
    QList<AviQtl::UI::AudioPluginState> audioPlugins;
};

struct ECSState {
    bool renderGraphDirty = false;
    DenseComponentMap<TransformComponent> transforms;
    DenseComponentMap<RenderComponent> renderStates;
    DenseComponentMap<AudioComponent> audioStates;
    DenseComponentMap<MetadataComponent> metadataStates;

    DenseComponentMap<SelectionComponent> selections;
    DenseComponentMap<EffectStackComponent> effectStacks;
    DenseComponentMap<AudioStackComponent> audioStacks;
    // 補間キャッシュは ECSState に含めない。
    // ECS::interpCache() （トリプルバッファ外）を使うこと。
};

class ECS {
  public:
    static ECS &instance();

    void syncClipIds(const QSet<int> &aliveIds);
    void updateClipState(int clipId, int layer, int startFrame, int durationFrames, double timePosition = 0.0);
    void updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute);
    void removeClip(int clipId);
    void updateMetadata(int clipId, const QString &name, const QString &source, const QString &type, const QString &color);
    void updateEffectStack(int clipId, const QList<AviQtl::UI::EffectData> &effects);

    void commit();

    ECSState &editState() { return m_buffers[m_editIndex]; }
    void markEvaluatedParamsDirty(int clipId) {
        m_dirtyForBuffer[(m_editIndex + 1) % 3].insert(clipId);
        m_dirtyForBuffer[(m_editIndex + 2) % 3].insert(clipId);
    }

    // 補間キャッシュアクセサ（トリプルバッファ外・コピー非対象）
    InterpolationCache &interpCache() noexcept { return m_interpCache; }
    const InterpolationCache &interpCache() const noexcept { return m_interpCache; }

    // 特定パラメータのキャッシュを無効化する（setKeyframe / removeKeyframe から呼ぶ）
    void invalidateEffectCache(int clipId, int effectIndex, const QString &paramName);
    // クリップのエフェクトキャッシュ全体を無効化する（エフェクト追加・削除・並び替え時）
    void invalidateAllEffectCaches(int clipId);

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

    // トリプルバッファリングの部分更新用
    // 各バッファが次に編集対象となったとき、同期が必要なClipIDリスト
    std::array<QSet<int>, 3> m_dirtyForBuffer;
    // 構造変更（追加・削除）があったため、フルコピーが必要かどうか
    std::array<bool, 3> m_fullSyncRequired{};

    // トリプルバッファ外の補間キャッシュ（commit() でコピーされない）
    InterpolationCache m_interpCache;
};

} // namespace AviQtl::Engine::Timeline
