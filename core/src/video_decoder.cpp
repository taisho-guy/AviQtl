#include "video_decoder.hpp"
#include "settings_manager.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QThread>
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

    // 動画本来のFPSを抽出する
    if (m_stream->avg_frame_rate.den > 0 && m_stream->avg_frame_rate.num > 0) {
        m_sourceFps = av_q2d(m_stream->avg_frame_rate);
    } else if (m_stream->r_frame_rate.den > 0 && m_stream->r_frame_rate.num > 0) {
        m_sourceFps = av_q2d(m_stream->r_frame_rate);
    } else {
        m_sourceFps = 60.0;
    }

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
        // スレッド制限を解除し、FFmpegの自動最適化（利用可能な全コア）を最大限活用する
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
    if (!m_isReady || frame < 0)
        return;

    // 最新の要求フレームを更新
    m_lastRequestedFrame = frame;

    // キャッシュにあれば即時適用
    if (m_frameCache.contains(frame)) {
        QByteArray *bytes = m_frameCache.object(frame);
        if (bytes && m_decCtx && m_decCtx->width > 0 && m_decCtx->height > 0) {
            QImage img(reinterpret_cast<const uchar *>(bytes->constData()), m_decCtx->width, m_decCtx->height, QImage::Format_RGBA8888);
            m_store->setFrameSafe(QString::number(m_clipId), img);
            emit frameReady(frame);
        }
        // 先読みパイプラインを途切れさせないためキャッシュヒット時もタスクを投げる
        (void)QtConcurrent::run([this, fps]() { decodeTask(-1, fps); });
        return;
    }

    (void)QtConcurrent::run([this, fps]() { decodeTask(-1, fps); });
}

void VideoDecoder::decodeTask(int, double fps) {
    // tryLock でキュー詰まりを回避。すでにデコード中なら新タスクは即終了
    if (!m_mutex.tryLock()) {
        return;
    }

    // ロック取得成功時、スコープを抜けるときに確実にアンロックする
    struct MutexUnlocker {
        QMutex *m;
        ~MutexUnlocker() { m->unlock(); }
    } unlocker{&m_mutex};

    if (!m_decCtx || m_index.empty())
        return;

    // 常に最新のリクエストフレームを追いかけるループ
    while (true) {
        int currentRequest = m_lastRequestedFrame.load();

        // 常に要求フレームの4つ先まで裏でデコードしておく
        int prefetchGoal = std::min((int)m_index.size() - 1, currentRequest + 4);

        // 逆再生や大ジャンプをした場合は現在位置をリセット
        if (currentRequest < m_lastDecodedFrame - 2 || currentRequest > m_lastDecodedFrame + 50) {
            m_lastDecodedFrame = -1;
        }

        if (m_lastDecodedFrame >= prefetchGoal)
            break;

        int targetFrame = std::max(currentRequest, m_lastDecodedFrame + 1);
        if (targetFrame >= (int)m_index.size())
            targetFrame = (int)m_index.size() - 1;
        int64_t targetPts = m_index[targetFrame].pts;

        bool needSeek = true;
        if (m_lastDecodedFrame != -1 && targetFrame > m_lastDecodedFrame && targetFrame <= m_lastDecodedFrame + 50) {
            needSeek = false;
        }

        if (needSeek) {
            int keyIndex = currentRequest;
            while (keyIndex > 0 && !m_index[keyIndex].isKeyframe)
                keyIndex--;
            av_seek_frame(m_fmtCtx, m_streamIndex, m_index[keyIndex].pts, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(m_decCtx);
            m_lastDecodedFrame = keyIndex - 1;
        }

        bool frameFound = false;
        AVPacket *pkt = av_packet_alloc();
        int maxDecodeCount = 500;

        while (maxDecodeCount-- > 0) {
            int newRequest = m_lastRequestedFrame.load();
            if (newRequest != currentRequest) {
                if (newRequest < m_lastDecodedFrame - 2 || newRequest > targetFrame + 50) {
                    break;
                }
                currentRequest = newRequest;
                prefetchGoal = std::min((int)m_index.size() - 1, currentRequest + 4);
                targetFrame = std::max(currentRequest, m_lastDecodedFrame + 1);
                if (targetFrame >= (int)m_index.size())
                    targetFrame = (int)m_index.size() - 1;
                targetPts = m_index[targetFrame].pts;
            }

            int ret = av_read_frame(m_fmtCtx, pkt);
            if (ret == AVERROR_EOF) {
                avcodec_send_packet(m_decCtx, nullptr);
            } else if (ret < 0) {
                break;
            } else if (pkt->stream_index != m_streamIndex) {
                av_packet_unref(pkt);
                continue;
            } else {
                if (currentRequest - m_lastDecodedFrame > 5) {
                    m_decCtx->skip_frame = AVDISCARD_NONREF;
                } else {
                    m_decCtx->skip_frame = AVDISCARD_DEFAULT;
                }
                avcodec_send_packet(m_decCtx, pkt);
                av_packet_unref(pkt);
            }

            while (true) {
                ret = avcodec_receive_frame(m_decCtx, m_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0) {
                    break;
                }

                int64_t pts = (m_frame->best_effort_timestamp != AV_NOPTS_VALUE) ? m_frame->best_effort_timestamp : m_frame->pts;

                auto it = std::lower_bound(m_index.begin(), m_index.end(), pts, [](const FrameIndexEntry &e, int64_t p) { return e.pts < p; });
                if (it != m_index.end()) {
                    m_lastDecodedFrame = std::distance(m_index.begin(), it);
                } else {
                    m_lastDecodedFrame++;
                }

                if (m_lastDecodedFrame >= targetFrame || pts >= targetPts) {
                    m_lastDecodedFrame = targetFrame;
                    frameFound = true;
                    goto process_frame;
                }
            }
        }

    process_frame:
        av_packet_unref(pkt);
        av_packet_free(&pkt);

        if (frameFound) {
            AVFrame *srcFrame = m_frame;
            if (m_frame->format == m_hwPixFmt) {
                if (av_hwframe_transfer_data(m_swFrame, m_frame, 0) < 0)
                    break;
                srcFrame = m_swFrame;
            }

            // SWS_POINTに変更し無駄な補間演算を全カット
            m_swsCtx = sws_getCachedContext(m_swsCtx, m_decCtx->width, m_decCtx->height, (AVPixelFormat)srcFrame->format, m_decCtx->width, m_decCtx->height, AV_PIX_FMT_RGBA, SWS_POINT, nullptr, nullptr, nullptr);

            QImage result(m_decCtx->width, m_decCtx->height, QImage::Format_RGBA8888);
            uint8_t *dstData[4] = {result.bits(), nullptr, nullptr, nullptr};
            int dstLinesize[4] = {static_cast<int>(result.bytesPerLine()), 0, 0, 0};

            sws_scale(m_swsCtx, srcFrame->data, srcFrame->linesize, 0, m_decCtx->height, dstData, dstLinesize);

            int cacheKey = targetFrame;

            QMetaObject::invokeMethod(this, [this, result, cacheKey]() {
                if (!m_frameCache.contains(cacheKey)) {
                    auto *bytes = new QByteArray(reinterpret_cast<const char *>(result.constBits()), result.sizeInBytes());
                    m_frameCache.insert(cacheKey, bytes, bytes->size());
                }
                // 先読み画像で画面を上書きしないよう現在の要求と一致した場合のみ更新
                if (cacheKey == m_lastRequestedFrame.load()) {
                    m_store->setFrameSafe(QString::number(m_clipId), result);
                    emit ready();
                }
            });
        } else {
            break;
        }
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