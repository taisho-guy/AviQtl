#include "video_frame_provider.hpp"
#include "video_frame_store.hpp"

namespace Rina::Core {

VideoFrameProvider::VideoFrameProvider(VideoFrameStore *store) : QQuickImageProvider(QQuickImageProvider::Image), m_store(store) {}

auto VideoFrameProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) -> QImage {
    // QMLから "1?v=123" のように来るので、"?"以降を切り捨ててキー "1" を取り出す
    const QString key = id.split('?').first();

    // 無効なキーや"-1"が来た場合は即座に透明な画像を返す（エラーログ抑制）
    if (key.isEmpty() || key == "-1" || key == "0") {
        return QImage(requestedSize.isValid() ? requestedSize : QSize(1, 1), QImage::Format_ARGB32_Premultiplied);
    }

    const auto img = m_store->frame(key);
    if (size != nullptr) {
        *size = img.size();
    }
    return img;
}

} // namespace Rina::Core