#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QVideoSink>
#include <QCache>

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

    // LRUキャッシュ: Key="frameNumber", Value=QImage
    // インスタンスごとにキャッシュを持つ
    QCache<int, QImage> m_frameCache;
    int m_lastRequestedFrame = -1;
};
} // namespace Rina::Core