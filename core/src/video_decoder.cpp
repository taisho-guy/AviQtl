#include "video_decoder.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QVideoFrame>

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

    // キャッシュ容量設定: 256MB (1920x1080x4byte ~= 8MB -> 約32フレーム分)
    // この値は将来的にユーザー設定から変更できるようにすると良い
    m_frameCache.setMaxCost(256 * 1024 * 1024);

    // 初期化時はPause状態で待機
    m_player->pause();

    // 勝手な再生を防ぐために再生速度を0にする手もあるが、まずはPauseを徹底
    // connect(m_player, &QMediaPlayer::playbackStateChanged, ...) で監視済み
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    if (!m_player)
        return;

    // 1. キャッシュ確認
    if (m_frameCache.contains(frame)) {
        qDebug() << "VideoDecoder: Cache hit for frame" << frame;
        m_store->setFrameSafe(QString::number(m_clipId), *m_frameCache[frame]);
        return;
    }

    qDebug() << "VideoDecoder: Cache miss for frame" << frame << "- seeking...";

    const qint64 ms = static_cast<qint64>((frame / fps) * 1000.0);
    m_lastRequestedFrame = frame; // リクエストしたフレーム番号を記憶
    qDebug() << "VideoDecoder::seekToFrame() clipId:" << m_clipId << "frame:" << frame << "ms:" << ms;

    m_player->setPosition(ms);

    // Play/Pause のトグルは廃止。setPositionだけでフレーム更新されるはず
    // もし更新されない環境なら、pause() のまま setPosition() が効くか確認が必要
}

void VideoDecoder::onVideoFrameChanged(const QVideoFrame &vf) {
    if (!vf.isValid())
        return;

    // 重複フレームのフィルタリング: タイムスタンプが前回と同じなら無視
    // これにより無駄なQImage変換とStore更新を防ぐ（ロング動画での安定性向上）
    if (vf.startTime() == m_lastFrameTimestamp)
        return;
    m_lastFrameTimestamp = vf.startTime();

    qDebug() << "VideoDecoder::onVideoFrameChanged() clipId:" << m_clipId << "pts:" << vf.startTime();

    QImage img = vf.toImage();

    if (!img.isNull()) {
        const QString key = QString::number(m_clipId);
        // Storeへ転送
        m_store->setFrameSafe(key, img);

        if (m_lastRequestedFrame >= 0 && !m_frameCache.contains(m_lastRequestedFrame)) {
            m_frameCache.insert(m_lastRequestedFrame, new QImage(img), img.sizeInBytes());
        }
    }
}

} // namespace Rina::Core