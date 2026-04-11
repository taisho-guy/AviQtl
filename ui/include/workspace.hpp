#pragma once
#include "timeline_controller.hpp"
#include "video_frame_store.hpp"
#include <QList>
#include <QObject>
#include <QVariantList>

namespace Rina::UI {

class Workspace : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Rina::UI::TimelineController *currentTimeline READ currentTimeline NOTIFY currentTimelineChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)

  public:
    explicit Workspace(QObject *parent = nullptr);

    void setVideoFrameStore(Rina::Core::VideoFrameStore *store);
    Rina::Core::VideoFrameStore *videoFrameStore() const { return m_videoFrameStore; }

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    TimelineController *currentTimeline() const;

    QVariantList tabs() const;

    Q_INVOKABLE void newProject();
    Q_INVOKABLE void closeProject(int index);
    Q_INVOKABLE void loadProject(const QString &fileUrl);

  signals:
    void currentIndexChanged();
    void currentTimelineChanged();
    void tabsChanged();

  private slots:
    void onTabStateChanged();

  private:
    QList<TimelineController *> m_timelines;
    int m_currentIndex = -1;
    int m_untitledCounter = 1;
    Rina::Core::VideoFrameStore *m_videoFrameStore = nullptr;
};

} // namespace Rina::UI
