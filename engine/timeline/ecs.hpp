#pragma once

namespace Rina::Engine::Timeline {
class ECS {
  public:
    static ECS &instance();

    // UIからの操作を受け取るメソッド
    void updateClipState(int clipId, int layer, double time);
    bool isRenderGraphDirty() const;
    void markRenderGraphClean();
};
} // namespace Rina::Engine::Timeline