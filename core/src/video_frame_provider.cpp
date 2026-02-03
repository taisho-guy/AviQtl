#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"

namespace Rina::Core {

VideoFrameProvider::VideoFrameProvider(VideoFrameStore *store) : QQuickImageProvider(QQuickImageProvider::Image), m_store(store) {}

QImage VideoFrameProvider::requestImage(const QString &id, QSize *size, const QSize & /*requestedSize*/) {
    const auto img = m_store->frame(id);
    if (size)
        *size = img.size();
    return img;
}

} // namespace Rina::Core