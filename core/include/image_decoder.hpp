#pragma once
#include "media_decoder.hpp"
#include <QFuture>
#include <QUrl>
#include <QVideoFrame>

namespace Rina::Core {

class VideoFrameStore;

class ImageDecoder : public MediaDecoder {
    Q_OBJECT
  public:
    explicit ImageDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);
    ~ImageDecoder() override;

    // MediaDecoder interface
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override;
    void startDecoding() override;

    void load(); // 互換性のため維持

  signals:
    // MediaDecoder::ready を使用

  private:
    void decodeImage(const QString &path);

    VideoFrameStore *m_store;
    QVideoFrame m_cachedVideoFrame;
    QImage m_cachedImage;
    QFuture<void> m_future;
};

} // namespace Rina::Core