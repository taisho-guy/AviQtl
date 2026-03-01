#include "video_decoder.hpp"
#include "settings_manager.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace Rina::Core {

enum AVPixelFormat VideoDecoder::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    auto *decoder = reinterpret_cast<VideoDecoder *>(ctx->opaque);

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == decoder->m_hwPixFmt) {
            return *p;
        }
    }
    // フォールバック
    return avcodec_default_get_format(ctx, pix_fmts);
}

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : MediaDecoder(clipId, source, parent), m_store(store) {
    m_frame = av_frame_alloc();
    m_swFrame = av_frame_alloc();

    // キャッシュ容量設定: SettingsManagerから取得し、変更を監視
    updateCacheSize();
    connect(&SettingsManager::instance(), &SettingsManager::settingsChanged, this, &VideoDecoder::updateCacheSize);
}

VideoDecoder::~VideoDecoder() {
    close();
    if (m_swsCtx)
        sws_freeContext(m_swsCtx);
    if (m_frame)
        av_frame_free(&m_frame);
    if (m_swFrame)
        av_frame_free(&m_swFrame);
}

void VideoDecoder::startDecoding() {
    (void)QtConcurrent::run([this] {
        QString path = m_source.toLocalFile();
        if (path.isEmpty())
            path = m_source.toString();

        if (open(path)) {
            m_isReady = true;
            emit ready();
        }
    });
}

bool VideoDecoder::open(const QString &path) {
    QMutexLocker locker(&m_mutex);
    close();
    m_lastDecodedFrame = -1;
    m_index.clear();

    if (avformat_open_input(&m_fmtCtx, path.toStdString().c_str(), nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0)
        return false;

    m_streamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_streamIndex < 0)
        return false;

    m_stream = m_fmtCtx->streams[m_streamIndex];
    const AVCodec *codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
    if (!codec)
        return false;

    m_decCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_decCtx, m_stream->codecpar);

    // ハードウェアデコーダーの初期化試行
    m_hwPixFmt = -1; // AV_PIX_FMT_NONE
    const char *hw_type_names[] = {"cuda", "vaapi", "d3d11va", "dxva2", "videotoolbox", nullptr};

    for (const char **name = hw_type_names; *name; name++) {
        enum AVHWDeviceType type = av_hwdevice_find_type_by_name(*name);
        if (type == AV_HWDEVICE_TYPE_NONE)
            continue;

        // コーデックがこのHWデバイス設定をサポートしているか確認
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config)
                break;

            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
                // デバイス作成
                if (av_hwdevice_ctx_create(&m_hwDeviceCtx, type, nullptr, nullptr, 0) >= 0) { // m_hwDeviceCtx is in base
                    qDebug() << "[VideoDecoder] Hardware device initialized:" << *name;
                    m_hwPixFmt = config->pix_fmt;
                    m_decCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
                    m_decCtx->get_format = get_hw_format;
                    m_decCtx->opaque = this;
                    goto hw_init_done;
                }
            }
        }
    }

hw_init_done:
    if (!m_hwDeviceCtx) {
        // HW初期化に失敗した場合はSWマルチスレッドを使用
        if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
            m_decCtx->thread_type = FF_THREAD_FRAME;
            m_decCtx->thread_count = 0;
        } else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            m_decCtx->thread_type = FF_THREAD_SLICE;
            m_decCtx->thread_count = 0;
        }
    }

    if (avcodec_open2(m_decCtx, codec, nullptr) < 0)
        return false;

    if (!buildIndex()) {
        qWarning() << "[VideoDecoder] Failed to build index for" << path;
        return false;
    }

    qDebug() << "[VideoDecoder] Opened:" << path << "Codec:" << codec->name << "Frames:" << m_index.size();
    return true;
}

bool VideoDecoder::buildIndex() {
    // This function is now called from open()
    // ファイル先頭へシーク
    if (av_seek_frame(m_fmtCtx, m_streamIndex, 0, AVSEEK_FLAG_BACKWARD) < 0) {
        av_seek_frame(m_fmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
    }

    // メモリ再確保のコストを削減
    if (m_stream->nb_frames > 0) {
        m_index.reserve(m_stream->nb_frames);
    } else {
        m_index.reserve(SettingsManager::instance().value("videoDecoderIndexReserve", 108000).toInt());
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
    // No need for a mutex here if it's only called from the destructor or a locked context
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
        m_decCtx = nullptr;
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
    // m_frame and m_swFrame are freed in destructor
    // m_swsCtx is freed in destructor
}

void VideoDecoder::seek(qint64 ms) {
    // This is for the base class interface. Video seeking is frame-based.
    // We can emit the signal for compatibility if needed.
    emit seekRequested(ms);
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    if (!m_isReady) {
        // If not ready, queue the seek? For now, just ignore.
        return;
    }

    if (frame < 0)
        return;

    // 最新の要求フレームを更新
    m_lastRequestedFrame = frame;

    // キャッシュにあれば即時適用
    if (m_frameCache.contains(frame)) {
        m_store->setFrameSafe(QString::number(m_clipId), *m_frameCache[frame]);
        emit frameReady(frame);
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
    // 閾値を30フレーム(60fpsで0.5秒)まで緩和し、シーケンシャル読み込みを優先する
    if (m_lastDecodedFrame != -1 && targetFrame > m_lastDecodedFrame && targetFrame <= m_lastDecodedFrame + 30) {
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
    AVPacket *pkt = av_packet_alloc();
    int maxDecodeCount = (targetFrame - m_lastDecodedFrame) + 300; // 安全マージン

    while (maxDecodeCount-- > 0) {
        int ret = av_read_frame(m_fmtCtx, pkt);
        if (ret < 0)
            break; // EOF or Error

        if (pkt->stream_index == m_streamIndex) {
            // パケット送信 (EAGAINハンドリング付き)
            ret = avcodec_send_packet(m_decCtx, pkt);

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
                ret = avcodec_send_packet(m_decCtx, pkt);
            }

            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                av_packet_unref(pkt);
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
        av_packet_unref(pkt);
    }

process_frame:
    av_packet_unref(pkt);
    av_packet_free(&pkt);

    if (frameFound) {
        AVFrame *srcFrame = m_frame;

        // HWフレームの場合はCPUメモリへ転送(ダウンロード)する
        if (m_frame->format == m_hwPixFmt) {
            if (av_hwframe_transfer_data(m_swFrame, m_frame, 0) < 0) {
                qWarning() << "[VideoDecoder] Error transferring frame data to CPU";
                return;
            }
            srcFrame = m_swFrame;
        }

        // YUV -> RGB 変換
        // srcFrame->format を使用する (HWデコード時はNV12などがここに入っている)
        m_swsCtx = sws_getCachedContext(m_swsCtx, m_decCtx->width, m_decCtx->height, (AVPixelFormat)srcFrame->format, m_decCtx->width, m_decCtx->height, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

        // QImageを直接確保し、sws_scaleの出力先として指定することで中間バッファとコピーを排除 (Zero-copy write)
        QImage result(m_decCtx->width, m_decCtx->height, QImage::Format_RGBA8888);
        uint8_t *dstData[4] = {result.bits(), nullptr, nullptr, nullptr};
        int dstLinesize[4] = {static_cast<int>(result.bytesPerLine()), 0, 0, 0};

        // 変換実行
        sws_scale(m_swsCtx, srcFrame->data, srcFrame->linesize, 0, m_decCtx->height, dstData, dstLinesize);

        // メインスレッドへ通知
        QMetaObject::invokeMethod(this, [this, result, targetFrame]() {
            m_store->setFrameSafe(QString::number(m_clipId), result);
            if (!m_frameCache.contains(targetFrame)) {
                m_frameCache.insert(targetFrame, new QImage(result), result.sizeInBytes());
            }
            emit ready(); // Or a more specific signal if needed
        });
    } else {
        // ターゲットが見つからなかった場合（ファイルの末尾など）
        // 必要なら黒画像などを返す処理をここに追加
        return;
    }
}

void VideoDecoder::updateCacheSize() {
    int sizeMB = SettingsManager::instance().settings().value("cacheSize", 512).toInt();
    int minSizeMB = SettingsManager::instance().value("videoDecoderMinCacheMB", 64).toInt();
    if (sizeMB < minSizeMB)
        sizeMB = minSizeMB;
    m_frameCache.setMaxCost(sizeMB * 1024 * 1024);
}

} // namespace Rina::Core