#pragma once
#include "engine/timeline/ecs.hpp"

namespace Rina::UI {

/// クリップ1本のフルスナップショット（Undo / Clipboard 正本）
/// Phase 3 Step 6 完了まで pasteClip / PasteClipCommand は ClipData 経由を維持する
struct ClipSnapshot {
    Rina::Engine::Timeline::TransformComponent transform;
    Rina::Engine::Timeline::MetadataComponent metadata;
    Rina::Engine::Timeline::EffectStackComponent effectStack;
    Rina::Engine::Timeline::AudioStackComponent audioStack;

    [[nodiscard]] bool isValid() const { return transform.clipId >= 0; }
};

} // namespace Rina::UI
