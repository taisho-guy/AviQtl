#include "video_frame_store.hpp"

namespace Rina::Core {

VideoFrameStore::VideoFrameStore(QObject *parent) : QObject(parent) {}

void VideoFrameStore::setFrame(const QString &key, const QImage &frame) {
    QMutexLocker locker(&m_mutex);
    m_frames.insert(key, frame);
}

void VideoFrameStore::setFrameSafe(const QString &key, const QImage &frame) {
    setFrame(key, frame);
    emit frameUpdated(key);
}

void VideoFrameStore::setVideoFrameSafe(const QString &key, const QVideoFrame &frame) {
    QVideoSink *s = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_sinks.contains(key)) {
            m_sinks.insert(key, new QVideoSink(this));
        }
        s = m_sinks.value(key);
    }
    if (s && frame.isValid()) {
        s->setVideoFrame(frame);
    }
    emit frameUpdated(key);
}

QVideoSink *VideoFrameStore::sink(const QString &key) {
    QMutexLocker locker(&m_mutex);
    if (!m_sinks.contains(key)) {
        m_sinks.insert(key, new QVideoSink(this));
    }
    return m_sinks.value(key);
}

void VideoFrameStore::registerSink(const QString &key, QVideoSink *sink) {
    QMutexLocker locker(&m_mutex);
    if (m_sinks.contains(key) && m_sinks.value(key)->parent() == this) {
        m_sinks.value(key)->deleteLater();
    }
    m_sinks.insert(key, sink);
}

bool VideoFrameStore::hasFrame(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_frames.contains(key) || m_sinks.contains(key);
}

void VideoFrameStore::invalidateFrame(const QString &key) {
    QMutexLocker locker(&m_mutex);
    m_frames.remove(key);
    // Note: QVideoSink 自体は削除しない（QML側で再利用されるため）
}

void VideoFrameStore::invalidateScene(int sceneId) {
    QMutexLocker locker(&m_mutex);
    QString prefix = QString::number(sceneId) + "_";
    for (auto it = m_frames.begin(); it != m_frames.end();) {
        if (it.key().startsWith(prefix)) {
            it = m_frames.erase(it);
        } else {
            ++it;
        }
    }
}

void VideoFrameStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_frames.clear();
}

QImage VideoFrameStore::frame(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_frames.value(key);
}

} // namespace Rina::Core
