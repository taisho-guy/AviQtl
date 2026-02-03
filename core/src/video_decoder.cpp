#include "video_decoder.hpp"
#include "video_frame_store.hpp"
#include <QVideoFrame>
#include <QDebug>

namespace Rina::Core {

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : QObject(parent), m_clipId(clipId), m_store(store) {
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);

    m_player->setVideoSink(m_sink);
    m_player->setSource(source);

    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoDecoder::onVideoFrameChanged);

    m_player->pause(); // シークのみ、自動再生しない

    qDebug() << "VideoDecoder created for clipId:" << clipId << "source:" << source;
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    qint64 ms = static_cast<qint64>((frame / fps) * 1000.0);
    // 現在位置と大きく異なる場合のみシーク（パフォーマンスのため）
    if (qAbs(m_player->position() - ms) > 100)
        m_player->setPosition(ms);
}

void VideoDecoder::onVideoFrameChanged(const QVideoFrame &vf) {
    if (!vf.isValid()) return;
    m_store->setFrame(QString::number(m_clipId), vf.toImage());
}

} // namespace Rina::Core