#pragma once
#include "clip_model.hpp"
#include "timeline_types.hpp"
#include <QObject>
#include <QPointer>

namespace Rina::UI {
class TimelineController;
class ClipModel;

class TimelineEngineSynchronizer : public QObject {
    Q_OBJECT
  public:
    explicit TimelineEngineSynchronizer(TimelineController *controller, QObject *parent = nullptr);

    ClipModel *clipModel() const { return m_clipModel; }
    int timelineDuration() const { return m_timelineDuration; }

    void updateActiveClipsList();
    void rebuildClipIndex();

  private:
    void updateECSState(const QList<ClipData *> &activeClips, int currentFrame);

    TimelineController *m_controller;
    ClipModel *m_clipModel;

    QList<ClipData *> m_sortedClips;
    int m_maxDuration = 0;
    int m_timelineDuration = 0;
};
} // namespace Rina::UI