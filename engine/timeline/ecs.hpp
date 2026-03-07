#pragma once
#include <QSet>
#include <QString>
#include <atomic>
#include <memory>
#include <unordered_map>

namespace Rina::Engine::Timeline {

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
    std::unordered_map<int, TransformComponent> transforms;
    std::unordered_map<int, RenderComponent> renderStates;
    std::unordered_map<int, AudioComponent> audioStates;
};

class ECS {
  public:
    static ECS &instance();

    void syncClipIds(const QSet<int> &aliveIds);
    void updateClipState(int clipId, int layer, double time);
    void updateAudioClipState(int clipId, int startFrame, int durationFrames, float volume, float pan, bool mute);

    void commit();
    std::shared_ptr<const ECSState> getSnapshot() const;

    bool isRenderGraphDirty() const;
    void markRenderGraphClean();

  private:
    ECS();
    ECSState m_editState;
    std::atomic<std::shared_ptr<const ECSState>> m_activeState;
};

} // namespace Rina::Engine::Timeline
