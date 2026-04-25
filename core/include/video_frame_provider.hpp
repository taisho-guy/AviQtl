#pragma once

#include <QQuickImageProvider>

namespace AviQtl::Core {

class VideoFrameStore;

class VideoFrameProvider : public QQuickImageProvider {
    Q_OBJECT
  public:
    VideoFrameProvider(const VideoFrameProvider &) = delete;
    VideoFrameProvider &operator=(const VideoFrameProvider &) = delete;
    explicit VideoFrameProvider(VideoFrameStore *store);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

  private:
    VideoFrameStore *m_store;
};
} // namespace AviQtl::Core