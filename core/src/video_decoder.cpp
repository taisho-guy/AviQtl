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

    // QMediaPlayerの状態変化を監視（デバッグ用）
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        qDebug() << "VideoDecoder playback state changed:" << state;
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
        qWarning() << "VideoDecoder error:" << error << errorString;
    });

    // フレーム取得のためには一度play()が必要
    m_player->play();
    // 即座にpauseしてシーク待機状態にする
    QTimer::singleShot(100, this, [this]() {
        m_player->pause();
        qDebug() << "VideoDecoder: Ready for seeking";
    });

    qDebug() << "VideoDecoder created for clipId:" << clipId << "source:" << source;
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    if (!m_player)
        return;

    const qint64 ms = static_cast<qint64>((frame / fps) * 1000.0);
    qDebug() << "VideoDecoder::seekToFrame() clipId:" << m_clipId << "frame:" << frame << "ms:" << ms;

    m_player->setPosition(ms);

    // シーク後、一瞬play()してフレームを取得させる
    if (m_player->playbackState() != QMediaPlayer::PlayingState) {
        m_player->play();
        QTimer::singleShot(50, this, [this]() { m_player->pause(); });
    }
}

void VideoDecoder::onVideoFrameChanged(const QVideoFrame &vf) {
    qDebug() << "VideoDecoder::onVideoFrameChanged() called for clipId:" << m_clipId << "valid:" << vf.isValid();

    if (!vf.isValid())
        return;

    QImage img = vf.toImage();
    qDebug() << "VideoDecoder: Frame converted to QImage, size:" << img.size() << "null:" << img.isNull();

    if (!img.isNull()) {
        const QString key = QString::number(m_clipId);
        qDebug() << "VideoDecoder: Storing frame with key:" << key;
        m_store->setFrame(key, img);
    }
}

} // namespace Rina::Core