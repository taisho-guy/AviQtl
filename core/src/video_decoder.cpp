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

    // シーク完了シグナルを使って確実にフレームを取得する
    // Qt6では mediaStatusChanged で BufferedMedia 等を監視する手もあるが
    // ここではシンプルに sink の更新を待つ
    qDebug() << "VideoDecoder created for clipId:" << m_clipId << "source:" << source;
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
    qDebug() << "VideoDecoder::onVideoFrameChanged() called for clipId:" << m_clipId << "valid:" << vf.isValid();

    if (!vf.isValid())
        return;

    QImage img = vf.toImage();
    qDebug() << "VideoDecoder: Frame converted to QImage, size:" << img.size() << "null:" << img.isNull();

    if (!img.isNull()) {
        const QString key = QString::number(m_clipId);
        // Storeへ転送
        m_store->setFrameSafe(key, img);

        // キャッシュに登録（コスト＝バイト数）
        // 実際に取得できたフレームが、リクエストしたものと一致すると仮定
        // シークの非同期性でズレる可能性はあるが、LRUなので「近いフレーム」が残ればOK
        if (m_lastRequestedFrame >= 0 && !m_frameCache.contains(m_lastRequestedFrame)) {
            m_frameCache.insert(m_lastRequestedFrame, new QImage(img), img.sizeInBytes());
        }
    }
}

} // namespace Rina::Core