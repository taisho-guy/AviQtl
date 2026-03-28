#pragma once
#include "clip_model.hpp"
#include "timeline_types.hpp"
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>

namespace Rina::UI {
class TimelineController;
class ClipModel;

struct ClipEngineResult {
    int clipId;
    int layer;
    int relFrame;
    bool hasAudio = false;
    int startFrame = 0;
    int durationFrames = 0;
    float vol = 1.0f;
    float pan = 0.0f;
    bool mute = false;
    QVariantMap evaluatedParams; // 追加: 計算済みの全パラメータ
};

class TimelineEngineSynchronizer : public QObject {
    Q_OBJECT
  public:
    explicit TimelineEngineSynchronizer(TimelineController *controller, QObject *parent = nullptr);

    ClipModel *clipModel() const { return m_clipModel; }
    ClipModel *renderModel() const { return m_renderModel; }
    int timelineDuration() const { return m_timelineDuration; }

    void updateActiveClipsList();
    void rebuildClipIndex();

    QVariantMap getCachedParams(int clipId) const { return m_paramCache.value(clipId); }

  private:
    void updateECSState(const QList<ClipData *> &activeClips, int currentFrame);

    void handleResultsReady();

    TimelineController *m_controller;
    ClipModel *m_clipModel;
    ClipModel *m_renderModel;

    QList<ClipData *> m_sortedClips;
    int m_maxDuration = 0;
    int m_timelineDuration = 0;

    QFutureWatcher<ClipEngineResult> m_futureWatcher;
    QHash<int, QVariantMap> m_paramCache; // クリップID -> パラメータマップ
};
} // namespace Rina::UI