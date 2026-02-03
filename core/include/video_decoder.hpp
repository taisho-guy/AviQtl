#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QVideoSink>
#include <QTimer>

namespace Rina::Core {

class VideoFrameStore; // forward declaration

class VideoDecoder : public QObject {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);

    Q_INVOKABLE void seekToFrame(int frame, double fps);

  private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

  private:
    int m_clipId;
    VideoFrameStore *m_store;
    QMediaPlayer *m_player;
    QVideoSink *m_sink;
};
} // namespace Rina::Core