#pragma once
#include "engine/timeline/ecs.hpp"

namespace AviQtl::UI {

/// クリップ1本のフルスナップショット（Undo / Clipboard 正本）
/// Phase 3 Step 6 完了まで pasteClip / PasteClipCommand は ClipData 経由を維持する
struct ClipSnapshot {
    AviQtl::Engine::Timeline::TransformComponent transform;
    AviQtl::Engine::Timeline::MetadataComponent metadata;
    AviQtl::Engine::Timeline::EffectStackComponent effectStack;
    AviQtl::Engine::Timeline::AudioStackComponent audioStack;

    [[nodiscard]] bool isValid() const { return transform.clipId >= 0; }
};

} // namespace AviQtl::UI
