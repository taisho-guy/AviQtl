#include "video_frame_store.hpp"
#include <QDebug>
#include <QMutexLocker>
#include <QThread>

namespace Rina::Core {

VideoFrameStore::VideoFrameStore(QObject *parent) : QObject(parent) {}

void VideoFrameStore::setFrame(const QString &key, const QImage &img) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setFrameSafe", Qt::QueuedConnection, Q_ARG(QString, key), Q_ARG(QImage, img));
        return;
    }
    setFrameSafe(key, img);
}

void VideoFrameStore::setFrameSafe(const QString &key, const QImage &img) {
    {
        QMutexLocker lock(&m_mutex);
        m_frames[key] = img.copy();
    }
    emit frameUpdated(key);
}

QImage VideoFrameStore::frame(const QString &key) const {
    QMutexLocker lock(&m_mutex);
    if (m_frames.contains(key))
        return m_frames.value(key);

    QImage empty(1, 1, QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    return empty;
}

bool VideoFrameStore::hasFrame(const QString &key) const {
    QMutexLocker lock(&m_mutex);
    return m_frames.contains(key);
}

void VideoFrameStore::invalidateFrame(const QString &key) {
    {
        QMutexLocker lock(&m_mutex);
        m_frames.remove(key);
    }
    emit frameUpdated(key);
}

void VideoFrameStore::invalidateScene(int sceneId) {
    const QString prefix = QString("scene_%1_").arg(sceneId);
    QStringList toRemove;
    {
        QMutexLocker lock(&m_mutex);
        for (auto it = m_frames.constKeyValueBegin(); it != m_frames.constKeyValueEnd(); ++it) {
            if (it->first.startsWith(prefix))
                toRemove.append(it->first);
        }
        for (const auto &k : toRemove)
            m_frames.remove(k);
    }
    for (const auto &k : toRemove)
        emit frameUpdated(k);
}

void VideoFrameStore::clear() {
    QStringList keys;
    {
        QMutexLocker lock(&m_mutex);
        keys = m_frames.keys();
        m_frames.clear();
    }
    for (const auto &k : keys)
        emit frameUpdated(k);
}

} // namespace Rina::Core
