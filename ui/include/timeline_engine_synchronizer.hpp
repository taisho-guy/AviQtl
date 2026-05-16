#pragma once
#include "timeline_types.hpp"
#include <QObject>
#include <QPointer>
#include <QVariantMap>
#include <vector>

namespace AviQtl::UI {
class TimelineController;

// O(log n + k) の区間検索を実現するための Interval Tree ノード
struct IntervalNode {
    ClipData *clip = nullptr;
    int maxEnd = 0;
    int left = -1;
    int right = -1;
};

class TimelineEngineSynchronizer : public QObject {
    Q_OBJECT
  public:
    explicit TimelineEngineSynchronizer(TimelineController *controller, QObject *parent = nullptr);

    int timelineDuration() const { return m_timelineDuration; }

    void updateActiveClipsList();
    void rebuildClipIndex();

  private:
    int buildIntervalTree(int left, int right);
    void queryIntervalTree(int nodeIdx, int frame, QList<ClipData *> &result);

    TimelineController *m_controller;

    QList<ClipData *> m_sortedClips;
    std::vector<IntervalNode> m_intervalTree;
    int m_treeRoot = -1;

    int m_maxDuration = 0;
    int m_timelineDuration = 0;
};
} // namespace AviQtl::UI
