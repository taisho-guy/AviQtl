#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"

namespace Rina::Core {

VideoFrameProvider::VideoFrameProvider(VideoFrameStore *store) : QQuickImageProvider(QQuickImageProvider::Image), m_store(store) {}

QImage VideoFrameProvider::requestImage(const QString &id, QSize *size, const QSize & /*requestedSize*/) {
    // QMLから "1?v=123" のように来るので、"?"以降を切り捨ててキー "1" を取り出す
    const QString key = id.split('?').first();
    const auto img = m_store->frame(key);
    if (size)
        *size = img.size();
    return img;
}

} // namespace Rina::Core