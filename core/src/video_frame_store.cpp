#include "video_frame_store.hpp"
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QMutexLocker>
#include <QPainter>
#include <QThread>

namespace Rina::Core {

VideoFrameStore::VideoFrameStore(QObject *parent) : QObject(parent) {}

void VideoFrameStore::setFrame(const QString &key, const QImage &img) {
    // スレッドセーフ対策: 呼び出し元が別スレッドならメインスレッドへ委譲
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, key, img]() { setFrame(key, img); }, Qt::QueuedConnection);
        return;
    }

    QMutexLocker lock(&m_mutex);
    m_frames[key] = img.copy(); // Ensure a deep copy for thread safety
    emit frameUpdated(key);
}

QImage VideoFrameStore::frame(const QString &key) const {
    QMutexLocker lock(&m_mutex);
    if (m_frames.contains(key)) {
        return m_frames.value(key);
    }

    // デバッグ用: キーがない場合は動的に画像を生成して返す
    // これで「プロバイダは動いているがデータがない」のか「プロバイダ自体が死んでいる」のか判別可能
    qWarning() << "[VideoFrameStore] フレームが見つかりません。キー:" << key << "。デバッグ画像を生成します。";
    QImage debugImg(320, 180, QImage::Format_ARGB32);
    debugImg.fill(Qt::darkMagenta);
    QPainter p(&debugImg);
    p.setPen(Qt::white);
    p.drawText(debugImg.rect(), Qt::AlignCenter, "No Frame\nKey: " + key);
    return debugImg;
}

} // namespace Rina::Core