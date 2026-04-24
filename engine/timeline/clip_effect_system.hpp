#pragma once
#include "effect_data.hpp"
#include "engine/timeline/ecs.hpp"
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <utility>

namespace Rina::Engine::Timeline {

class ClipEffectSystem {
  public:
    // ── 評価 API ──────────────────────────────────────────────────────────────
    // キャッシュなし版（テスト・Undo プレビュー用）
    // durationFrames: クリップ長（TransformComponent.durationFrames を渡す）
    // fps: シーンの fps
    static QVariantMap evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame, int durationFrames, double fps = 60.0);

    static QVariant evaluateParam(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int effectIndex, const QString &paramName, int relFrame, int durationFrames, double fps = 60.0);

    // キャッシュあり版（レンダリングループ・UI リアルタイム評価用）
    // effectCaches は ECSState::effectCaches を渡す（mutable）
    static QVariantMap evaluateParamsCached(ECSState &state, int clipId, int relFrame, int durationFrames, double fps = 60.0);

    static QVariant evaluateParamCached(ECSState &state, int clipId, int effectIndex, const QString &paramName, int relFrame, int durationFrames, double fps = 60.0);

    static void addEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data);
    static void restoreEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data);

    // effectIndex == -1 で末尾。削除成功時 true。outRemoved が非 null なら削除データを格納する。
    static bool removeEffect(ECSState &state, int clipId, int effectIndex, Rina::UI::EffectData *outRemoved = nullptr);

    // sortedDescIndices は降順で渡す
    static bool removeMultipleEffects(ECSState &state, int clipId, const QList<int> &sortedDescIndices, QList<Rina::UI::EffectData> *outRemoved = nullptr);

    // ascData は昇順で渡す
    static void restoreMultipleEffects(ECSState &state, int clipId, const QList<Rina::UI::EffectData> &ascData);

    static void setEffectEnabled(ECSState &state, int clipId, int effectIndex, bool enabled);
    static bool reorderEffects(ECSState &state, int clipId, int oldIndex, int newIndex);
    static bool applyPermutation(ECSState &state, int clipId, const QList<int> &perm);
    static void pasteEffect(ECSState &state, int clipId, int targetIndex, const Rina::UI::EffectData &data);
    static void updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, const QVariant &value);
    static void setKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options);
    static void removeKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame);
    static std::pair<QVariantMap, QVariantMap> splitKeyframeTracks(const QVariantMap &tracks, const QVariantMap &params, int firstHalfDuration, int originalDuration);
    static void setKeyframeTracksAt(ECSState &state, int clipId, int effectIndex, const QVariantMap &tracks, int durationFrames);
};

} // namespace Rina::Engine::Timeline
