#include "video_decoder.hpp"
#include "settings_manager.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace Rina::Core {

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : QObject(parent), m_clipId(clipId), m_store(store) {
    QString path = source.toLocalFile();
    if (path.isEmpty())
        path = source.toString();

    if (!open(path)) {
        qWarning() << "[VideoDecoder] Failed to open:" << path;
    }

    // キャッシュ容量設定: SettingsManagerから取得し、変更を監視
    updateCacheSize();
    connect(&SettingsManager::instance(), &SettingsManager::settingsChanged, this, &VideoDecoder::updateCacheSize);
}

VideoDecoder::~VideoDecoder() { close(); }

bool VideoDecoder::open(const QString &path) {
    QMutexLocker locker(&m_mutex);
    close();
    m_lastDecodedFrame = -1;
    m_index.clear();

    int ret = avformat_open_input(&m_fmtCtx, path.toStdString().c_str(), nullptr, nullptr);
    if (ret < 0)
        return false;

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0)
        return false;

    // Find video stream
    m_streamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_streamIndex < 0)
        return false;

    m_stream = m_fmtCtx->streams[m_streamIndex];
    const AVCodec *codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
    if (!codec)
        return false;

    m_decCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_decCtx, m_stream->codecpar);

    // マルチスレッドデコード設定
    // "Late SEI is not implemented" 警告とフリーズ回避のため、フレームスレッドを無効化(1)する
    // m_decCtx->thread_count = 1;
    if (avcodec_open2(m_decCtx, codec, nullptr) < 0)
        return false;

    m_frame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    m_pkt = av_packet_alloc();

    // インデックス構築 (L-SMASH Worksライクな挙動)
    if (!buildIndex()) {
        qWarning() << "[VideoDecoder] Failed to build index";
        return false;
    }

    qDebug() << "[VideoDecoder] Opened:" << path << "Codec:" << codec->name << "Frames:" << m_index.size();
    return true;
}

bool VideoDecoder::buildIndex() {
    // ファイル先頭へシーク
    if (av_seek_frame(m_fmtCtx, m_streamIndex, 0, AVSEEK_FLAG_BACKWARD) < 0) {
        av_seek_frame(m_fmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
    }

    AVPacket *pkt = av_packet_alloc();
    while (av_read_frame(m_fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == m_streamIndex) {
            m_index.push_back({pkt->pts, pkt->dts, (bool)(pkt->flags & AV_PKT_FLAG_KEY)});
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    // PTS順にソート（表示順序を確定）
    std::sort(m_index.begin(), m_index.end(), [](const FrameIndexEntry &a, const FrameIndexEntry &b) {
        if (a.pts != AV_NOPTS_VALUE && b.pts != AV_NOPTS_VALUE)
            return a.pts < b.pts;
        return a.dts < b.dts;
    });

    return true;
}

void VideoDecoder::close() {
    // mutex is usually locked by caller or destructor
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
    }
    if (m_pkt) {
        av_packet_free(&m_pkt);
    }
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    if (frame < 0)
        return;

    // 最新の要求フレームを更新
    m_lastRequestedFrame = frame;

    // キャッシュにあれば即時適用
    if (m_frameCache.contains(frame)) {
        m_store->setFrameSafe(QString::number(m_clipId), *m_frameCache[frame]);
        emit frameDecoded(frame);
        return;
    }

    // 非同期でデコードタスクを実行
    (void)QtConcurrent::run([this, frame, fps]() { decodeTask(frame, fps); });
}

void VideoDecoder::decodeTask(int targetFrame, double fps) {
    QMutexLocker locker(&m_mutex);
    if (!m_decCtx || m_index.empty())
        return;

    // 範囲制限
    if (targetFrame < 0)
        targetFrame = 0;
    if (targetFrame >= (int)m_index.size())
        targetFrame = (int)m_index.size() - 1;

    // 既に別の新しいリクエストが来ている場合はスキップ（簡易的なキャンセル）
    if (m_lastRequestedFrame != targetFrame)
        return;

    // インデックスから正確なターゲット情報を取得
    const auto &targetEntry = m_index[targetFrame];
    int64_t targetPts = targetEntry.pts;

    // シークが必要か判定
    // 連続再生(次のフレーム)ならシーク不要だが、誤差を考慮して判定
    bool needSeek = true;
    // 修正: 多少のフレーム飛び（ドロップ）があっても、直近ならシークせずにデコードを進める
    // これにより、GOP先頭への巻き戻しと再デコードによるフリーズ（処理落ち）を防ぐ
    if (m_lastDecodedFrame != -1 && targetFrame > m_lastDecodedFrame && targetFrame <= m_lastDecodedFrame + 12) {
        needSeek = false;
    }

    if (needSeek) {
        // インデックスを使って直前のキーフレームを探す
        int keyIndex = targetFrame;
        while (keyIndex > 0 && !m_index[keyIndex].isKeyframe) {
            keyIndex--;
        }
        int64_t seekPts = m_index[keyIndex].pts;

        // 正確なPTSへシーク
        int ret = av_seek_frame(m_fmtCtx, m_streamIndex, seekPts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            qWarning() << "[VideoDecoder] Seek failed to PTS" << seekPts;
            return;
        }
        avcodec_flush_buffers(m_decCtx);
        m_lastDecodedFrame = -1; // リセット
    }

    // デコードループ (プリロール)
    // キーフレームからターゲットPTSまで空読みする
    bool frameFound = false;
    int maxDecodeCount = (targetFrame - m_lastDecodedFrame) + 300; // 安全マージン

    while (maxDecodeCount-- > 0) {
        int ret = av_read_frame(m_fmtCtx, m_pkt);
        if (ret < 0)
            break; // EOF or Error

        if (m_pkt->stream_index == m_streamIndex) {
            // パケット送信 (EAGAINハンドリング付き)
            ret = avcodec_send_packet(m_decCtx, m_pkt);

            // 入力バッファが一杯(EAGAIN)の場合、出力(フレーム)を読み出して空ける
            while (ret == AVERROR(EAGAIN)) {
                int rxRet = avcodec_receive_frame(m_decCtx, m_frame);
                if (rxRet < 0)
                    break; // 受信もできない場合は中断

                // PTSの一致を確認（インデックスベースなので正確）
                if (m_frame->pts == targetPts) {
                    m_lastDecodedFrame = targetFrame;
                    frameFound = true;
                    goto process_frame;
                }

                // 再試行
                ret = avcodec_send_packet(m_decCtx, m_pkt);
            }

            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                av_packet_unref(m_pkt);
                continue;
            }

            // 送信成功後のフレーム回収 (通常フロー)
            while (true) {
                int rxRet = avcodec_receive_frame(m_decCtx, m_frame);
                if (rxRet < 0)
                    break; // EAGAIN or EOF

                if (m_frame->pts == targetPts) {
                    m_lastDecodedFrame = targetFrame;
                    frameFound = true;
                    goto process_frame;
                }
            }
        }
        av_packet_unref(m_pkt);
    }

process_frame:
    av_packet_unref(m_pkt);

    if (frameFound) {
        // YUV -> RGB 変換
        if (!m_swsCtx || m_decCtx->width != m_rgbFrame->width || m_decCtx->height != m_rgbFrame->height) {

            if (m_swsCtx)
                sws_freeContext(m_swsCtx);

            // QImage::Format_RGBA8888 に合わせる
            m_swsCtx = sws_getContext(m_decCtx->width, m_decCtx->height, m_decCtx->pix_fmt, m_decCtx->width, m_decCtx->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

            // RGBフレームバッファ確保
            av_frame_unref(m_rgbFrame);
            m_rgbFrame->width = m_decCtx->width;
            m_rgbFrame->height = m_decCtx->height;
            m_rgbFrame->format = AV_PIX_FMT_RGBA;
            av_frame_get_buffer(m_rgbFrame, 32);
        }

        // 変換実行
        sws_scale(m_swsCtx, m_frame->data, m_frame->linesize, 0, m_decCtx->height, m_rgbFrame->data, m_rgbFrame->linesize);

        // QImage作成 (ディープコピーしてキャッシュへ)
        QImage img(m_rgbFrame->data[0], m_decCtx->width, m_decCtx->height, m_rgbFrame->linesize[0], QImage::Format_RGBA8888);
        QImage result = img.copy(); // Detach from AVFrame buffer

        // メインスレッドへ通知
        QMetaObject::invokeMethod(this, [this, result, targetFrame]() {
            m_store->setFrameSafe(QString::number(m_clipId), result);
            if (!m_frameCache.contains(targetFrame)) {
                m_frameCache.insert(targetFrame, new QImage(result), result.sizeInBytes());
            }
            emit frameDecoded(targetFrame);
        });
    } else {
        // ターゲットが見つからなかった場合（ファイルの末尾など）
        // 必要なら黒画像などを返す処理をここに追加
        return;
    }
}

void VideoDecoder::updateCacheSize() {
    int sizeMB = SettingsManager::instance().settings().value("cacheSize", 512).toInt();
    if (sizeMB < 64)
        sizeMB = 64; // 安全策: 最小64MB
    m_frameCache.setMaxCost(sizeMB * 1024 * 1024);
}

} // namespace Rina::Core