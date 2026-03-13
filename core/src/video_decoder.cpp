#include "video_decoder.hpp"
#include "settings_manager.hpp"
#include "video_frame_store.hpp"
#include <QDebug>
#include <QPointer>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

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
    // ワーカースレッドの新規タスク開始・invokeMethod実行をブロック
    m_closing.store(true, std::memory_order_release);

    if (m_initFuture.isRunning())
        m_initFuture.waitForFinished();
    if (m_decodeFuture.isRunning())
        m_decodeFuture.waitForFinished();
    close(); // Removed unnecessary lock to prevent UI deadlock
    if (m_swsCtx)
        sws_freeContext(m_swsCtx);
    if (m_frame)
        av_frame_free(&m_frame);
    if (m_swFrame)
        av_frame_free(&m_swFrame);
}

void VideoDecoder::startDecoding() {
    m_initFuture = QtConcurrent::run([this] {
        QString path = m_source.toLocalFile();
        if (path.isEmpty())
            path = m_source.toString();

        if (open(path)) {
            m_isReady = true;
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    emit ready();
                    emit videoMetaReady((int)m_index.size(), m_sourceFps);
                },
                Qt::QueuedConnection);
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
    m_timeBase = m_stream->time_base;
    double fps = av_q2d(m_stream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = av_q2d(m_stream->r_frame_rate);
    }
    m_sourceFps = fps;
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
            m_decCtx->thread_count = Rina::Core::SettingsManager::instance().value("videoDecoderThreads", 0).toInt();
        } else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            m_decCtx->thread_type = FF_THREAD_SLICE;
            m_decCtx->thread_count = Rina::Core::SettingsManager::instance().value("videoDecoderThreads", 0).toInt();
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

double VideoDecoder::sourceFps() const { return m_sourceFps; }

int VideoDecoder::totalFrameCount() const { return (int)m_index.size(); }

int VideoDecoder::frameIndexFromSeconds(double seconds) const {
    if (m_index.empty())
        return 0;

    const double tb = av_q2d(m_timeBase);
    if (tb <= 0.0) {
        double fps = m_sourceFps > 0.0 ? m_sourceFps : 30.0;
        int f = static_cast<int>(std::llround(seconds * fps));
        if (f < 0)
            f = 0;
        if (f >= (int)m_index.size())
            f = (int)m_index.size() - 1;
        return f;
    }

    const int64_t targetPts = (int64_t)std::llround(seconds / tb);

    auto it = std::lower_bound(m_index.begin(), m_index.end(), targetPts, [](const FrameIndexEntry &e, int64_t v) { return e.pts < v; });

    int idx = (int)std::distance(m_index.begin(), it);
    if (idx <= 0)
        return 0;
    if (idx >= (int)m_index.size())
        return (int)m_index.size() - 1;

    const int64_t a = m_index[idx - 1].pts;
    const int64_t b = m_index[idx].pts;
    return (std::llabs(targetPts - a) <= std::llabs(b - targetPts)) ? (idx - 1) : idx;
}

void VideoDecoder::seekToTime(double seconds) {
    if (!m_isReady)
        return;
    if (seconds < 0.0)
        seconds = 0.0;

    const int frame = frameIndexFromSeconds(seconds);
    seekToFrame(frame, m_sourceFps);
}
void VideoDecoder::seekToFrame(int frame, double fps) {
    if (!m_isReady)
        return;
    if (frame < 0)
        return;

    // 要求フレームを更新
    m_lastRequestedFrame.store(frame, std::memory_order_release);

    // キャッシュチェックと Store への転送は、排他制御下（decodeTask内）で行うよう委譲する。
    // これにより、UIスレッドとワーカースレッド間での m_frameCache 競合によるSIGSEGVを完全に防ぐ。

    // isDecoding フラグで多重投入を防ぎ、単一ワーカーとして動かす
    bool expected = false;
    if (m_isDecoding.compare_exchange_strong(expected, true)) {
        m_decodeFuture = QtConcurrent::run([this, fps]() {
            while (!m_closing.load(std::memory_order_acquire)) {
                int targetFrame = m_lastRequestedFrame.load(std::memory_order_acquire);
                decodeTask(targetFrame, fps);

                if (m_lastRequestedFrame.load(std::memory_order_acquire) == targetFrame) {
                    m_isDecoding.store(false, std::memory_order_release);
                    if (m_lastRequestedFrame.load(std::memory_order_acquire) != targetFrame) {
                        bool exp = false;
                        if (m_isDecoding.compare_exchange_strong(exp, true)) {
                            continue;
                        }
                    }
                    break;
                }
            }
        });
    }
}

void VideoDecoder::decodeTask(int targetFrame, double fps) {
    QMutexLocker locker(&m_mutex);
    if (m_closing.load(std::memory_order_acquire))
        return;
    if (!m_decCtx || m_index.empty())
        return;

    if (targetFrame < 0)
        targetFrame = 0;
    if (targetFrame >= (int)m_index.size())
        targetFrame = (int)m_index.size() - 1;

    if (m_lastRequestedFrame != targetFrame)
        return;

    // キャッシュに存在する場合はデコードをスキップして即反映
    if (m_frameCache.contains(targetFrame)) {
        m_store->setVideoFrameSafe(QString::number(m_clipId), *m_frameCache[targetFrame]);
        // UIスレッドへ安全にシグナルを発火
        QMetaObject::invokeMethod(this, [this, targetFrame]() { emit frameReady(targetFrame); }, Qt::QueuedConnection);
        return;
    }

    const auto &targetEntry = m_index[targetFrame];
    int64_t targetPts = targetEntry.pts;

    bool needSeek = true;
    // P/Bフレームによる内部状態(参照フレーム等)の維持とFFmpegの特性を活かし、
    // 順方向なら長めの空回し（最大120フレーム）を許容してシークのコストと状態破綻を回避する
    if (m_lastDecodedFrame != -1 && targetFrame > m_lastDecodedFrame && targetFrame <= m_lastDecodedFrame + 120) {
        needSeek = false;
    }

    if (needSeek) {
        int keyIndex = targetFrame;
        while (keyIndex > 0 && !m_index[keyIndex].isKeyframe) {
            keyIndex--;
        }
        int64_t seekPts = m_index[keyIndex].pts;

        if (keyIndex == 0) {
            int64_t startTime = m_fmtCtx->streams[m_streamIndex]->start_time;
            if (startTime != AV_NOPTS_VALUE && startTime < seekPts)
                seekPts = startTime;
        }

        int ret = av_seek_frame(m_fmtCtx, m_streamIndex, seekPts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            av_seek_frame(m_fmtCtx, -1, seekPts, AVSEEK_FLAG_BACKWARD);
        }
        avcodec_flush_buffers(m_decCtx);
        m_lastDecodedFrame = keyIndex - 1;
    }

    bool frameFound = false;
    AVPacket *pkt = av_packet_alloc();
    int maxDecodeCount = Rina::Core::SettingsManager::instance().value("videoDecoderMaxPrefetchCount", 500).toInt();
    bool eof = false;

    while (maxDecodeCount-- > 0 && !frameFound) {
        int ret = 0;
        if (!eof) {
            ret = av_read_frame(m_fmtCtx, pkt);
            if (ret < 0) {
                eof = true;
            }
        }

        if (eof) {
            ret = avcodec_send_packet(m_decCtx, nullptr);
        } else if (pkt->stream_index == m_streamIndex) {
            ret = avcodec_send_packet(m_decCtx, pkt);
        }

        if (!eof) {
            av_packet_unref(pkt);
        }

        while (ret >= 0 || ret == AVERROR(EAGAIN)) {
            int rxRet = avcodec_receive_frame(m_decCtx, m_frame);
            if (rxRet == AVERROR(EAGAIN)) {
                break;
            }
            if (rxRet == AVERROR_EOF) {
                eof = true;
                break;
            }
            if (rxRet < 0) {
                break;
            }

            int64_t currentPts = m_frame->best_effort_timestamp != AV_NOPTS_VALUE ? m_frame->best_effort_timestamp : m_frame->pts;

            if (currentPts >= targetPts) {
                m_lastDecodedFrame = targetFrame;
                frameFound = true;
                break;
            }
        }

        // Removed premature EOF break. We must drain the codec until AVERROR_EOF.
    }

    av_packet_free(&pkt);

    if (frameFound) {
        AVFrame *srcFrame = m_frame;
        if (m_frame->format == m_hwPixFmt) {
            if (av_hwframe_transfer_data(m_swFrame, m_frame, 0) < 0) {
                return;
            }
            srcFrame = m_swFrame;
        }

        QVideoFrameFormat::PixelFormat qtFmt = QVideoFrameFormat::Format_Invalid;
        switch (srcFrame->format) {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            qtFmt = QVideoFrameFormat::Format_YUV420P;
            break;
        case AV_PIX_FMT_NV12:
            qtFmt = QVideoFrameFormat::Format_NV12;
            break;
        case AV_PIX_FMT_RGBA:
            qtFmt = QVideoFrameFormat::Format_RGBA8888;
            break;
        default:
            qtFmt = QVideoFrameFormat::Format_YUV420P;
            break;
        }

        QVideoFrameFormat format(QSize(m_decCtx->width, m_decCtx->height), qtFmt);
        QVideoFrame videoFrame(format);

        bool mapped = videoFrame.map(QVideoFrame::WriteOnly);
        if (mapped) {
            // 究極の最適化: linesize が完全に一致する場合は、プレーン全体を一気に memcpy する
            if (qtFmt == QVideoFrameFormat::Format_YUV420P) {
                for (int i = 0; i < 3; ++i) {
                    if (videoFrame.bits(i) && srcFrame->data[i]) {
                        int h = (i == 0) ? m_decCtx->height : m_decCtx->height / 2;
                        int srcLine = srcFrame->linesize[i];
                        int dstLine = videoFrame.bytesPerLine(i);
                        if (srcLine == dstLine) {
                            memcpy(videoFrame.bits(i), srcFrame->data[i], dstLine * h);
                        } else {
                            int copyLine = std::min(srcLine, dstLine);
                            for (int y = 0; y < h; ++y) {
                                memcpy(videoFrame.bits(i) + y * dstLine, srcFrame->data[i] + y * srcLine, copyLine);
                            }
                        }
                    }
                }
            } else if (qtFmt == QVideoFrameFormat::Format_NV12) {
                for (int i = 0; i < 2; ++i) {
                    if (videoFrame.bits(i) && srcFrame->data[i]) {
                        int h = (i == 0) ? m_decCtx->height : m_decCtx->height / 2;
                        int srcLine = srcFrame->linesize[i];
                        int dstLine = videoFrame.bytesPerLine(i);
                        if (srcLine == dstLine) {
                            memcpy(videoFrame.bits(i), srcFrame->data[i], dstLine * h);
                        } else {
                            int copyLine = std::min(srcLine, dstLine);
                            for (int y = 0; y < h; ++y) {
                                memcpy(videoFrame.bits(i) + y * dstLine, srcFrame->data[i] + y * srcLine, copyLine);
                            }
                        }
                    }
                }
            } else if (qtFmt == QVideoFrameFormat::Format_RGBA8888) {
                int srcLine = srcFrame->linesize[0];
                int dstLine = videoFrame.bytesPerLine(0);
                if (srcLine == dstLine) {
                    memcpy(videoFrame.bits(0), srcFrame->data[0], dstLine * m_decCtx->height);
                } else {
                    int copyLine = std::min(srcLine, dstLine);
                    for (int y = 0; y < m_decCtx->height; ++y) {
                        memcpy(videoFrame.bits(0) + y * dstLine, srcFrame->data[0] + y * srcLine, copyLine);
                    }
                }
            }
            videoFrame.unmap();
            videoFrame.setStartTime(-1);
            videoFrame.setEndTime(-1);
        }

        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(this, [self, videoFrame, targetFrame, mapped]() {
            if (!self || self->m_closing.load(std::memory_order_acquire))
                return;
            if (mapped && videoFrame.isValid()) {
                self->m_store->setVideoFrameSafe(QString::number(self->m_clipId), videoFrame);
                int cost = videoFrame.width() * videoFrame.height() * 4;
                self->m_frameCache.insert(targetFrame, new QVideoFrame(videoFrame), cost);
            }
            emit self->frameReady(targetFrame);
        });
    } else {
        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(this, [self, targetFrame]() {
            if (!self || self->m_closing.load(std::memory_order_acquire))
                return;
            emit self->frameReady(targetFrame);
        });
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