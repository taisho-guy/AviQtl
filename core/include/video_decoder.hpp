#pragma once

#include <QCache>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QVideoSink>

namespace Rina::Core {

class VideoFrameStore; // forward declaration

class VideoDecoder : public QObject {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);

    Q_INVOKABLE void seekToFrame(int frame, double fps);
    Q_INVOKABLE void setPlaying(bool playing);

  private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);
    void updateCacheSize();

  private:
    int m_clipId;
    VideoFrameStore *m_store;
    QMediaPlayer *m_player;
    QVideoSink *m_sink;

    // LRUキャッシュ: Key="frameNumber", Value=QImage
    // インスタンスごとにキャッシュを持つ
    QCache<int, QImage> m_frameCache;
    int m_lastRequestedFrame = -1;
    qint64 m_lastFrameTimestamp = -1; // 重複フレームチェック用
    double m_fps = 60.0;
};
} // namespace Rina::Core