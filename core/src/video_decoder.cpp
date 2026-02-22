#include "video_decoder.hpp"
#include "settings_manager.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QVideoFrame>
#include <QtConcurrent>

namespace Rina::Core {

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : QObject(parent), m_clipId(clipId), m_store(store) {
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);

    m_player->setVideoSink(m_sink);
    m_player->setSource(source);

    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoDecoder::onVideoFrameChanged);

    // QMediaPlayerの状態変化を監視（デバッグ用）
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [](QMediaPlayer::PlaybackState state) { qDebug() << "VideoDecoder playback state changed:" << state; });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [](QMediaPlayer::Error error, const QString &errorString) { qWarning() << "VideoDecoder error:" << error << errorString; });

    // キャッシュ容量設定: SettingsManagerから取得し、変更を監視
    updateCacheSize();
    connect(&SettingsManager::instance(), &SettingsManager::settingsChanged, this, &VideoDecoder::updateCacheSize);

    // 初期化時はPause状態で待機
    m_player->pause();

    // 勝手な再生を防ぐために再生速度を0にする手もあるが、まずはPauseを徹底
    // connect(m_player, &QMediaPlayer::playbackStateChanged, ...) で監視済み
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    m_fps = fps;
    if (!m_player)
        return;

    // 再生中は setPosition (シーク) を避け、自然な再生に任せる
    // これにより GOP 境界でのフリーズやバッファフラッシュによるカクつきを防ぐ
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_lastRequestedFrame = frame;
        const qint64 targetMs = static_cast<qint64>((frame / fps) * 1000.0);
        // 許容範囲(300ms)を超えてズレた場合のみ補正シークを行う
        if (qAbs(m_player->position() - targetMs) > 300) {
            m_player->setPosition(targetMs);
        }
        return;
    }

    if (frame == m_lastRequestedFrame)
        return;

    // 1. キャッシュ確認
    if (m_frameCache.contains(frame)) {
        m_store->setFrameSafe(QString::number(m_clipId), *m_frameCache[frame]);
        m_lastRequestedFrame = frame;
        emit frameDecoded(frame); // キャッシュヒット時も完了通知を送る
        return;
    }

    const qint64 ms = static_cast<qint64>((frame / fps) * 1000.0);
    m_lastRequestedFrame = frame;

    // プリフェッチ: 次のフレーム(frame+1)もキャッシュになければ、デコーダを先行させる
    if (!m_frameCache.contains(frame + 1)) {
        m_player->setPosition(ms);
    }
}

void VideoDecoder::onVideoFrameChanged(const QVideoFrame &vf) {
    if (!vf.isValid() || m_lastRequestedFrame < 0)
        return;

    // タイムスタンプ重複ガード
    if (vf.startTime() == m_lastFrameTimestamp)
        return;

    int targetFrame = m_lastRequestedFrame;
    m_lastFrameTimestamp = vf.startTime();

    // 重い変換処理の前に、すでにキャッシュされているか再確認（競合防止）
    if (m_frameCache.contains(targetFrame))
        return;

    // 警告回避しつつ非同期実行。GPUテクスチャが利用可能ならマップを試みる
    auto future = QtConcurrent::run([this, vf, targetFrame]() {
        // GPU->CPUの転送(toImage)を最小化するため、まずはマップ可能かチェック
        if (const_cast<QVideoFrame &>(vf).map(QVideoFrame::ReadOnly)) {
            QImage img = vf.toImage();
            const_cast<QVideoFrame &>(vf).unmap();

            // 色味修正: FFmpegやShaderが期待する RGBA 順序に統一する
            if (img.format() != QImage::Format_RGBA8888) {
                img = img.convertToFormat(QImage::Format_RGBA8888);
            }

            if (!img.isNull()) {
                QMetaObject::invokeMethod(
                    this,
                    [this, img, targetFrame]() {
                        m_store->setFrameSafe(QString::number(m_clipId), img);
                        if (!m_frameCache.contains(targetFrame)) {
                            m_frameCache.insert(targetFrame, new QImage(img), img.sizeInBytes());
                        }
                        emit frameDecoded(targetFrame); // デコード完了通知
                    },
                    Qt::QueuedConnection);
            }
        }
    });
    (void)future; // 警告を確実に消去
}

void VideoDecoder::updateCacheSize() {
    int sizeMB = SettingsManager::instance().settings().value("cacheSize", 512).toInt();
    if (sizeMB < 64)
        sizeMB = 64; // 安全策: 最小64MB
    m_frameCache.setMaxCost(sizeMB * 1024 * 1024);
}

void VideoDecoder::setPlaying(bool playing) {
    if (!m_player)
        return;
    if (playing) {
        m_player->play();
        m_player->setPlaybackRate(m_playbackRate);
    } else {
        m_player->pause();
    }
}

void VideoDecoder::setPlaybackRate(double rate) {
    m_playbackRate = rate;
    if (m_player && m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->setPlaybackRate(rate);
    }
}

} // namespace Rina::Core