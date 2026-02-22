#pragma once
#include "video_frame_store.hpp"
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>

namespace Rina::Core {

class SceneDecoder : public QObject {
    Q_OBJECT
    Q_PROPERTY(int targetSceneId READ targetSceneId WRITE setTargetSceneId NOTIFY targetSceneIdChanged)
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(Rina::Core::VideoFrameStore *store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(QObject *timelineBridge READ timelineBridge WRITE setTimelineBridge NOTIFY timelineBridgeChanged)
    Q_PROPERTY(QString frameKey READ frameKey NOTIFY frameKeyChanged)

  public:
    explicit SceneDecoder(QObject *parent = nullptr);
    ~SceneDecoder();

    int targetSceneId() const { return m_targetSceneId; }
    void setTargetSceneId(int id);

    int currentFrame() const { return m_currentFrame; }
    void setCurrentFrame(int frame);

    VideoFrameStore *store() const { return m_store; }
    void setStore(VideoFrameStore *store);

    QObject *timelineBridge() const { return m_timelineBridge; }
    void setTimelineBridge(QObject *bridge);

    QString frameKey() const;

  signals:
    void targetSceneIdChanged();
    void currentFrameChanged();
    void storeChanged();
    void timelineBridgeChanged();
    void frameKeyChanged();

  private:
    void updateRender();
    void ensureWindow();

    int m_targetSceneId = -1;
    int m_currentFrame = 0;
    VideoFrameStore *m_store = nullptr;
    QObject *m_timelineBridge = nullptr;

    QPointer<QQuickWindow> m_offscreenWindow;
    QQuickItem *m_rootItem = nullptr;
};
} // namespace Rina::Core
