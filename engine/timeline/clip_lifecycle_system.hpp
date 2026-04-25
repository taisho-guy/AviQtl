#pragma once
#include "clip_snapshot.hpp"
#include "effect_data.hpp"
#include "engine/timeline/ecs.hpp"
#include "timeline_types.hpp"

namespace AviQtl::Engine::Timeline {

class ClipLifecycleSystem {
  public:
    // 新規クリップ(Entity)を作成し、初期コンポーネントをセットアップする
    static void createClip(ECSState &state, int clipId, const QString &type, int layer, int startFrame, int durationFrames);

    // クリップ(Entity)と全コンポーネントを削除する
    static void destroyClip(ECSState &state, int clipId);

    // スナップショット(DTO)からクリップ状態を復元する
    static void restoreClipFromDTO(ECSState &state, const AviQtl::UI::ClipData &dto);
    static void restoreClipFromSnapshot(ECSState &state, const AviQtl::UI::ClipSnapshot &snap);
};

} // namespace AviQtl::Engine::Timeline
