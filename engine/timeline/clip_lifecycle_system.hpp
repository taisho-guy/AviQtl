#pragma once
#include "engine/timeline/ecs.hpp"
#include "timeline_types.hpp"

namespace Rina::Engine::Timeline {

class ClipLifecycleSystem {
  public:
    // 新規クリップ(Entity)を作成し、初期コンポーネントをセットアップする
    static void createClip(ECSState &state, int clipId, const QString &type, int layer, int startFrame, int durationFrames);

    // クリップ(Entity)と全コンポーネントを削除する
    static void destroyClip(ECSState &state, int clipId);

    // スナップショット(DTO)からクリップ状態を復元する
    static void restoreClipFromDTO(ECSState &state, const Rina::UI::ClipData &dto);
};

} // namespace Rina::Engine::Timeline
