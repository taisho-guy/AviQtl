#include <QSet>
#pragma once
#include <memory>
#include <unordered_map>

namespace Rina::Engine::Timeline {

struct Component {
    virtual ~Component() = default;
};

struct AudioComponent : Component {
    int clipId = -1;
    int startFrame = 0;
    int durationFrames = 0;
    float volume = 1.0f;
    float pan = 0.0f;
    bool mute = false;
};

class ECS {
  public:
    static ECS &instance();

    // UIからの操作を受け取るメソッド群
    void syncClipIds(const QSet<int> &aliveIds);
    void updateClipState(int clipId, int layer, double time);
    void updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute);
    const std::unordered_map<int, std::unique_ptr<AudioComponent>> &getAudioComponents() const;

    bool isRenderGraphDirty() const;
    void markRenderGraphClean();

    // Phase 2.2: 内部状態へのポインタを取得 (Lua MOD用)
    void *getInternalStatePtr();
};
} // namespace Rina::Engine::Timeline