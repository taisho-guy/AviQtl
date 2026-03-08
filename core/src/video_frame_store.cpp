#include "video_frame_store.hpp"
#include <QMetaObject>
#include <QThread>
#include <QDebug>
#include <QPointer>

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
    if (QThread::currentThread() != thread()) {
        QVideoFrame copy(frame);
        QMetaObject::invokeMethod(this, [this, key, copy]() mutable {
            setVideoFrameSafe(key, copy);
        }, Qt::QueuedConnection);
        return;
    }

    QPointer<QVideoSink> sink;
    {
        QMutexLocker locker(&m_mutex);
        m_lastVideoFrames.insert(key, frame);

        if (m_sinks.contains(key) && !m_sinks.value(key).isNull()) {
            sink = m_sinks.value(key);
        }
    }

    if (sink && frame.isValid()) {
        sink->setVideoFrame(frame);
    }
    emit frameUpdated(key);
}

QVideoSink *VideoFrameStore::sink(const QString &key) {
    QMutexLocker locker(&m_mutex);
    if (!m_sinks.contains(key) || m_sinks.value(key).isNull()) {
        auto *s = new QVideoSink(this);
        m_sinks.insert(key, s);
    }
    return m_sinks.value(key);
}

void VideoFrameStore::registerSink(const QString &key, QVideoSink *sink) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, key, sink]() { registerSink(key, sink); }, Qt::QueuedConnection);
        return;
    }

    QVideoFrame last;
    {
        QMutexLocker locker(&m_mutex);

        if (m_sinks.contains(key) && !m_sinks.value(key).isNull() && m_sinks.value(key)->parent() == this) {
            m_sinks.value(key)->deleteLater();
        }

        m_sinks.insert(key, sink);

        if (sink) {
            QObject::connect(sink, &QObject::destroyed, this, [this, key, rawSink = sink]() {
                QMutexLocker locker(&m_mutex);
                if (m_sinks.contains(key)) {
                    QVideoSink* current = m_sinks.value(key).data();
                    if (current == nullptr || current == rawSink) {
                        m_sinks.remove(key);
                    }
                }
            });
        }

        if (m_lastVideoFrames.contains(key)) {
            last = m_lastVideoFrames.value(key);
        }
    }

    if (sink && last.isValid()) {
        sink->setVideoFrame(last);
        emit frameUpdated(key);
    }
}

bool VideoFrameStore::hasFrame(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_frames.contains(key) || m_sinks.contains(key);
}

void VideoFrameStore::invalidateFrame(const QString &key) {
    if (QThread::currentThread() != thread()) { QMetaObject::invokeMethod(this, [this, key]() { invalidateFrame(key); }, Qt::QueuedConnection); return; }
    QMutexLocker locker(&m_mutex);
    m_frames.remove(key);
    m_lastVideoFrames.remove(key);
    // Note: QVideoSink 自体は削除しない（QML側で再利用されるため）
}

void VideoFrameStore::invalidateScene(int sceneId) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, sceneId]() { invalidateScene(sceneId); }, Qt::QueuedConnection);
        return;
    }

    QMutexLocker locker(&m_mutex);
    QString prefix = QString::number(sceneId) + "_";

    for (auto it = m_frames.begin(); it != m_frames.end();) {
        if (it.key().startsWith(prefix)) {
            it = m_frames.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_lastVideoFrames.begin(); it != m_lastVideoFrames.end();) {
        if (it.key().startsWith(prefix)) {
            it = m_lastVideoFrames.erase(it);
        } else {
            ++it;
        }
    }
}

void VideoFrameStore::clear() {
    if (QThread::currentThread() != thread()) { QMetaObject::invokeMethod(this, [this]() { clear(); }, Qt::QueuedConnection); return; }
    QMutexLocker locker(&m_mutex);
    m_frames.clear();
    m_lastVideoFrames.clear();
}

QImage VideoFrameStore::frame(const QString &key) const {
    QMutexLocker locker(&m_mutex);
    return m_frames.value(key);
}

} // namespace Rina::Core
