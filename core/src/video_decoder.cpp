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
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace AviQtl::Core {

auto VideoDecoder::gethwformat(AVCodecContext *ctx, const enum AVPixelFormat *pixfmts) -> enum AVPixelFormat {
    const enum AVPixelFormat *p = nullptr;
    auto *decoder = reinterpret_cast<VideoDecoder *>(ctx->opaque);
    for (p = pixfmts; *p != -1; p++) {
        if (*p == decoder->mhwPixFmt) {
            return *p;
        }
    }
    return avcodec_default_get_format(ctx, pixfmts);
}

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : MediaDecoder(clipId, source, parent), mstore(store), mframe(av_frame_alloc()) {

    updateCacheSize();
    connect(&SettingsManager::instance(), &SettingsManager::settingsChanged, this, &VideoDecoder::updateCacheSize);
}

VideoDecoder::~VideoDecoder() {
    mclosing.store(true, std::memory_order_release);
    if (minitFuture.isRunning()) {
        minitFuture.waitForFinished();
    }
    if (mdecodeFuture.isRunning()) {
        mdecodeFuture.waitForFinished();
    }
    close();
    if (mswsCtx != nullptr) {
        sws_freeContext(mswsCtx);
    }
    if (mframe != nullptr) {
        av_frame_free(&mframe);
    }
}

void VideoDecoder::startDecoding() {
    minitFuture = QtConcurrent::run([this]() -> void {
        QString path = m_source.toLocalFile();
        if (path.isEmpty()) {
            path = m_source.toString();
        }
        if (open(path)) {
            m_isReady = true;
            QMetaObject::invokeMethod(
                this,
                [this]() -> void {
                    emit ready();
                    emit videoMetaReady(static_cast<int>(mindex.size()), msourceFps);
                },
                Qt::QueuedConnection);
        }
    });
}

auto VideoDecoder::open(const QString &path) -> bool {
    QMutexLocker locker(&m_mutex);
    close();
    mlastDecodedFrame = -1;
    mindex.clear();

    if (avformat_open_input(&mfmtCtx, path.toStdString().c_str(), nullptr, nullptr) != 0) {
        return false;
    }
    if (avformat_find_stream_info(mfmtCtx, nullptr) < 0) {
        return false;
    }

    mstreamIndex = av_find_best_stream(mfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (mstreamIndex < 0) {
        return false;
    }

    mstream = mfmtCtx->streams[mstreamIndex];
    mtimeBase = mstream->time_base;
    double fps = av_q2d(mstream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = av_q2d(mstream->r_frame_rate);
    }
    msourceFps = fps;

    const AVCodec *codec = avcodec_find_decoder(mstream->codecpar->codec_id);
    if (codec == nullptr) {
        return false;
    }

    mdecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(mdecCtx, mstream->codecpar);

    mhwPixFmt = -1;
    const char *hwtypenames[] = {"cuda", "vaapi", "d3d11va", "dxva2", "videotoolbox", nullptr};
    for (const char **name = hwtypenames; (*name) != nullptr; ++name) {
        enum AVHWDeviceType type = av_hwdevice_find_type_by_name(*name);
        if (type == AV_HWDEVICE_TYPE_NONE) {
            continue;
        }
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (config == nullptr) {
                break;
            }
            if (((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0) && config->device_type == type) {
                if (av_hwdevice_ctx_create(&mhwDeviceCtx, type, nullptr, nullptr, 0) == 0) {
                    mhwPixFmt = config->pix_fmt;
                    mdecCtx->hw_device_ctx = av_buffer_ref(mhwDeviceCtx);
                    mdecCtx->get_format = gethwformat;
                    mdecCtx->opaque = this;
                    goto hwinitdone;
                }
            }
        }
    }
hwinitdone:
    if (mhwDeviceCtx == nullptr) {
        if ((codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) != 0) {
            mdecCtx->thread_type = FF_THREAD_FRAME;
            mdecCtx->thread_count = AviQtl::Core::SettingsManager::instance().value(QStringLiteral("videoDecoderThreads"), 0).toInt();
        } else if ((codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) != 0) {
            mdecCtx->thread_type = FF_THREAD_SLICE;
            mdecCtx->thread_count = AviQtl::Core::SettingsManager::instance().value(QStringLiteral("videoDecoderThreads"), 0).toInt();
        }
    }

    if (avcodec_open2(mdecCtx, codec, nullptr) != 0) {
        return false;
    }
    if (!buildIndex()) {
        return false;
    }
    return true;
}

auto VideoDecoder::buildIndex() -> bool {
    if (av_seek_frame(mfmtCtx, mstreamIndex, 0, AVSEEK_FLAG_BACKWARD) < 0) {
        av_seek_frame(mfmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
    }

    if (mstream->nb_frames > 0) {
        mindex.reserve(mstream->nb_frames);
    } else {
        mindex.reserve(SettingsManager::instance().value(QStringLiteral("videoDecoderIndexReserve"), 108000).toInt());
    }

    AVPacket *pkt = av_packet_alloc();
    while (av_read_frame(mfmtCtx, pkt) >= 0) {
        if (pkt->stream_index == mstreamIndex) {
            mindex.push_back({.pts = pkt->pts, .dts = pkt->dts, .isKeyframe = static_cast<bool>(pkt->flags & AV_PKT_FLAG_KEY)});
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    std::ranges::sort(mindex, [](const auto &a, const auto &b) -> auto {
        if (a.pts != AV_NOPTS_VALUE && b.pts != AV_NOPTS_VALUE) {
            return a.pts < b.pts;
        }
        return a.dts < b.dts;
    });
    return true;
}

void VideoDecoder::close() {
    if (mdecCtx != nullptr) {
        avcodec_free_context(&mdecCtx);
    }
    mdecCtx = nullptr;
    if (mfmtCtx != nullptr) {
        avformat_close_input(&mfmtCtx);
    }
    mfmtCtx = nullptr;
    if (mhwDeviceCtx != nullptr) {
        av_buffer_unref(&mhwDeviceCtx);
    }
    mhwDeviceCtx = nullptr;
}

void VideoDecoder::seek(qint64 ms) { emit seekRequested(ms); }

auto VideoDecoder::sourceFps() const -> double { return msourceFps; }
auto VideoDecoder::totalFrameCount() const -> int { return static_cast<int>(mindex.size()); }

auto VideoDecoder::frameIndexFromSeconds(double seconds) const -> int {
    if (mindex.empty()) {
        return 0;
    }
    const double tb = av_q2d(mtimeBase);
    if (tb <= 0.0) {
        double fps = msourceFps > 0.0 ? msourceFps : 30.0;
        int f = static_cast<int>(std::llround(seconds * fps));
        f = std::max(f, 0);
        f = std::min(f, static_cast<int>(mindex.size()) - 1);
        return f;
    }
    const auto targetPts = static_cast<int64_t>(std::llround(seconds / tb));
    auto it = std::ranges::lower_bound(mindex, targetPts, std::ranges::less{}, &FrameIndexEntry::pts);
    int idx = static_cast<int>(std::distance(mindex.begin(), it));
    if (idx <= 0) {
        return 0;
    }
    if (std::cmp_greater_equal(idx, mindex.size())) {
        return static_cast<int>(mindex.size()) - 1;
    }
    const int64_t a = mindex[idx - 1].pts;
    const int64_t b = mindex[idx].pts;
    return std::llabs(targetPts - a) <= std::llabs(b - targetPts) ? idx - 1 : idx;
}

void VideoDecoder::seekToTime(double seconds) {
    if (!m_isReady) {
        return;
    }
    seconds = std::max(seconds, 0.0);
    const int frame = frameIndexFromSeconds(seconds);
    seekToFrame(frame, msourceFps);
}

void VideoDecoder::seekToFrame(int frame, double fps) { // NOLINT(bugprone-easily-swappable-parameters)
    if (!m_isReady) {
        return;
    }
    if (frame < 0) {
        return;
    }
    mlastRequestedFrame.store(frame, std::memory_order_release);

    bool expected = false;
    if (!misDecoding.compare_exchange_strong(expected, true)) {
        return;
    }

    mdecodeFuture = QtConcurrent::run([this, fps]() -> void {
        while (!mclosing.load(std::memory_order_acquire)) {
            int targetFrame = mlastRequestedFrame.load(std::memory_order_acquire);
            decodeTask(targetFrame, fps);
            if (mlastRequestedFrame.load(std::memory_order_acquire) == targetFrame) {
                misDecoding.store(false, std::memory_order_release);
                if (mlastRequestedFrame.load(std::memory_order_acquire) != targetFrame) {
                    bool exp = false;
                    if (misDecoding.compare_exchange_strong(exp, true)) {
                        continue;
                    }
                }
                break;
            }
        }
    });
}

void VideoDecoder::decodeTask(int targetFrame, double fps) { // NOLINT(bugprone-easily-swappable-parameters)
    QMutexLocker locker(&m_mutex);
    if (mclosing.load(std::memory_order_acquire)) {
        return;
    }
    if ((mdecCtx == nullptr) || mindex.empty()) {
        return;
    }

    targetFrame = std::max(targetFrame, 0);
    targetFrame = std::min(targetFrame, static_cast<int>(mindex.size()) - 1);
    if (mlastRequestedFrame != targetFrame) {
        return;
    }

    if (mframeCache.contains(targetFrame)) {
        mstore->setVideoFrameSafe(QString::number(clipId()), *mframeCache.object(targetFrame));
        QMetaObject::invokeMethod(this, [this, targetFrame]() -> void { emit frameReady(targetFrame); }, Qt::QueuedConnection);
        return;
    }

    const auto &targetEntry = mindex[targetFrame];
    int64_t targetPts = targetEntry.pts;
    bool needSeek = true;

    if (mlastDecodedFrame != -1 && targetFrame > mlastDecodedFrame && targetFrame <= mlastDecodedFrame + 120) {
        needSeek = false;
    }

    if (needSeek) {
        int keyIndex = targetFrame;
        while (keyIndex > 0 && !mindex[keyIndex].isKeyframe) {
            --keyIndex;
        }
        int64_t seekPts = mindex[keyIndex].pts;
        int ret = av_seek_frame(mfmtCtx, mstreamIndex, seekPts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            av_seek_frame(mfmtCtx, -1, seekPts, AVSEEK_FLAG_BACKWARD);
        }
        avcodec_flush_buffers(mdecCtx);
        mlastDecodedFrame = keyIndex - 1;
    }

    bool frameFound = false;
    AVPacket *pkt = av_packet_alloc();
    int maxDecodeCount = SettingsManager::instance().value(QStringLiteral("videoDecoderMaxPrefetchCount"), 500).toInt();
    bool eof = false;

    while (maxDecodeCount-- > 0 && !frameFound) {
        int ret = 0;
        if (!eof) {
            ret = av_read_frame(mfmtCtx, pkt);
            if (ret < 0) {
                eof = true;
            }
        }
        if (eof) {
            ret = avcodec_send_packet(mdecCtx, nullptr);
        } else if (pkt->stream_index == mstreamIndex) {
            ret = avcodec_send_packet(mdecCtx, pkt);
        }
        if (!eof) {
            av_packet_unref(pkt);
        }

        while (ret >= 0 || ret == AVERROR(EAGAIN)) {
            int rxRet = avcodec_receive_frame(mdecCtx, mframe);
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

            int64_t currentPts = mframe->best_effort_timestamp != AV_NOPTS_VALUE ? mframe->best_effort_timestamp : mframe->pts;

            auto it = std::ranges::lower_bound(mindex, currentPts, std::ranges::less{}, &FrameIndexEntry::pts);
            int decodedFrameIndex = -1;
            if (it != mindex.end() && it->pts == currentPts) {
                decodedFrameIndex = static_cast<int>(std::distance(mindex.begin(), it));
            }

            if (decodedFrameIndex != -1 && !mframeCache.contains(decodedFrameIndex)) {
                AVFrame *srcFrame = mframe;
                AVFrame *swFrame = nullptr;
                AVFrame *convertedFrame = nullptr;

                if (mframe->format == mhwPixFmt) {
                    swFrame = av_frame_alloc();
                    if (av_hwframe_transfer_data(swFrame, mframe, 0) == 0) {
                        srcFrame = swFrame;
                    } else {
                        av_frame_free(&swFrame);
                        srcFrame = nullptr;
                    }
                }

                if (srcFrame != nullptr) {
                    // Qtが直接扱えないフォーマット(10bit等)の場合、YUV420P(8bit)に変換する
                    bool isSupported = (srcFrame->format == AV_PIX_FMT_YUV420P || srcFrame->format == AV_PIX_FMT_YUVJ420P || srcFrame->format == AV_PIX_FMT_NV12 || srcFrame->format == AV_PIX_FMT_RGBA);

                    if (!isSupported) {
                        mswsCtx = sws_getCachedContext(mswsCtx, srcFrame->width, srcFrame->height, static_cast<AVPixelFormat>(srcFrame->format), srcFrame->width, srcFrame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
                        if (mswsCtx != nullptr) {
                            convertedFrame = av_frame_alloc();
                            convertedFrame->format = AV_PIX_FMT_YUV420P;
                            convertedFrame->width = srcFrame->width;
                            convertedFrame->height = srcFrame->height;
                            if (av_frame_get_buffer(convertedFrame, 32) == 0) {
                                sws_scale(mswsCtx, srcFrame->data, srcFrame->linesize, 0, srcFrame->height, convertedFrame->data, convertedFrame->linesize);
                                convertedFrame->pts = srcFrame->pts;
                                convertedFrame->best_effort_timestamp = srcFrame->best_effort_timestamp;
                                srcFrame = convertedFrame;
                            } else {
                                av_frame_free(&convertedFrame);
                                convertedFrame = nullptr;
                            }
                        }
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

                    AVFrame *ownedFrame = av_frame_alloc();
                    av_frame_ref(ownedFrame, srcFrame);
                    if (swFrame != nullptr) {
                        av_frame_free(&swFrame);
                    }
                    if (convertedFrame != nullptr) {
                        av_frame_free(&convertedFrame);
                    }

                    QVideoFrameFormat format(QSize(mdecCtx->width, mdecCtx->height), qtFmt);
                    QVideoFrame videoFrame(new FFmpegVideoBuffer(ownedFrame, format), format);
                    av_frame_free(&ownedFrame);

                    videoFrame.setStartTime(-1);
                    videoFrame.setEndTime(-1);

                    if (videoFrame.isValid()) {
                        int cost = videoFrame.width() * videoFrame.height();
                        mframeCache.insert(decodedFrameIndex, new QVideoFrame(videoFrame), cost);
                    }
                }
            }

            if (currentPts >= targetPts) {
                mlastDecodedFrame = decodedFrameIndex != -1 ? decodedFrameIndex : targetFrame;
                frameFound = true;
                break;
            }
        }
    }
    av_packet_free(&pkt);

    if (mframeCache.contains(targetFrame)) {
        mstore->setVideoFrameSafe(QString::number(clipId()), *mframeCache.object(targetFrame));
        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, targetFrame]() -> void {
                if (self && !self->mclosing.load(std::memory_order_acquire)) {
                    emit self->frameReady(targetFrame);
                }
            },
            Qt::QueuedConnection);
    } else {
        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, targetFrame]() -> void {
                if (self && !self->mclosing.load(std::memory_order_acquire)) {
                    emit self->frameError(targetFrame);
                }
            },
            Qt::QueuedConnection);
    }
}

void VideoDecoder::updateCacheSize() {
    int sizeMB = SettingsManager::instance().settings().value(QStringLiteral("cacheSize"), 512).toInt();
    int minSizeMB = SettingsManager::instance().value(QStringLiteral("videoDecoderMinCacheMB"), 64).toInt();
    sizeMB = std::max(sizeMB, minSizeMB);
    mframeCache.setMaxCost(static_cast<qsizetype>(sizeMB) * 1024 * 1024);
}

void VideoDecoder::setPlaying(bool playing) {
    // 再生状態をスレッドセーフに更新
    misPlaying.store(playing, std::memory_order_release);
}

} // namespace AviQtl::Core
