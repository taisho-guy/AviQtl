#pragma once

#include <QQuickImageProvider>

namespace Rina::Core {

class VideoFrameStore;

class VideoFrameProvider : public QQuickImageProvider {
  public:
    explicit VideoFrameProvider(VideoFrameStore *store);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

  private:
    VideoFrameStore *m_store;
};
} // namespace Rina::Core