#pragma once
#include "effect_data.hpp"
#include "engine/timeline/ecs.hpp"
#include <QList>
#include <QString>
#include <QVariantMap>
#include <utility>

namespace Rina::Engine::Timeline {

class ClipEffectSystem {
  public:
    // ECSのコンポーネントから直接エフェクトパラメータを評価する
    static QVariantMap evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame);

    // エフェクト追加（新規）
    static void addEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data);

    // エフェクト復元（Undo redo 側）
    static void restoreEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data);

    // エフェクト削除（effectIndex == -1 で末尾）
    // 削除成功時 true。outRemoved が非 null なら削除データを格納する。
    static bool removeEffect(ECSState &state, int clipId, int effectIndex, Rina::UI::EffectData *outRemoved = nullptr);

    // 複数エフェクト削除（sortedDescIndices は降順）
    static bool removeMultipleEffects(ECSState &state, int clipId, const QList<int> &sortedDescIndices, QList<Rina::UI::EffectData> *outRemoved = nullptr);

    // 複数エフェクト復元（ascData は昇順）
    static void restoreMultipleEffects(ECSState &state, int clipId, const QList<Rina::UI::EffectData> &ascData);

    // エフェクト有効/無効切り替え
    static void setEffectEnabled(ECSState &state, int clipId, int effectIndex, bool enabled);

    // 単一エフェクト並び替え
    static bool reorderEffects(ECSState &state, int clipId, int oldIndex, int newIndex);

    // 置換配列による並び替え（Undo 対応）
    static bool applyPermutation(ECSState &state, int clipId, const QList<int> &perm);

    // クリップボードからのペースト
    static void pasteEffect(ECSState &state, int clipId, int targetIndex, const Rina::UI::EffectData &data);

    // パラメータ書き込み
    static void updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, const QVariant &value);

    // キーフレーム設定
    static void setKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options);

    // キーフレーム削除
    static void removeKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame);

    static std::pair<QVariantMap, QVariantMap> splitKeyframeTracks(const QVariantMap &tracks, const QVariantMap &params, int firstHalfDuration, int originalDuration);

    static void setKeyframeTracksAt(ECSState &state, int clipId, int effectIndex, const QVariantMap &tracks, int durationFrames);
};

} // namespace Rina::Engine::Timeline
