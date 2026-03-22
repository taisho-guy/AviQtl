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

enum AVPixelFormat VideoDecoder::gethwformat(AVCodecContext *ctx, const enum AVPixelFormat *pixfmts) {
    const enum AVPixelFormat *p;
    auto *decoder = reinterpret_cast<VideoDecoder *>(ctx->opaque);
    for (p = pixfmts; *p != -1; p++) {
        if (*p == decoder->mhwPixFmt)
            return *p;
    }
    return avcodec_default_get_format(ctx, pixfmts);
}

VideoDecoder::VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent) : MediaDecoder(clipId, source, parent), mstore(store) {
    mframe = av_frame_alloc();
    updateCacheSize();
    connect(&SettingsManager::instance(), &SettingsManager::settingsChanged, this, &VideoDecoder::updateCacheSize);
}

VideoDecoder::~VideoDecoder() {
    mclosing.store(true, std::memory_order_release);
    if (minitFuture.isRunning())
        minitFuture.waitForFinished();
    if (mdecodeFuture.isRunning())
        mdecodeFuture.waitForFinished();
    close();
    if (mswsCtx)
        sws_freeContext(mswsCtx);
    if (mframe)
        av_frame_free(&mframe);
}

void VideoDecoder::startDecoding() {
    minitFuture = QtConcurrent::run([this]() {
        QString path = m_source.toLocalFile();
        if (path.isEmpty())
            path = m_source.toString();
        if (open(path)) {
            m_isReady = true;
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    emit ready();
                    emit videoMetaReady(int(mindex.size()), msourceFps);
                },
                Qt::QueuedConnection);
        }
    });
}

bool VideoDecoder::open(const QString &path) {
    QMutexLocker locker(&m_mutex);
    close();
    mlastDecodedFrame = -1;
    mindex.clear();

    if (avformat_open_input(&mfmtCtx, path.toStdString().c_str(), nullptr, nullptr) != 0)
        return false;
    if (avformat_find_stream_info(mfmtCtx, nullptr) < 0)
        return false;

    mstreamIndex = av_find_best_stream(mfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (mstreamIndex < 0)
        return false;

    mstream = mfmtCtx->streams[mstreamIndex];
    mtimeBase = mstream->time_base;
    double fps = av_q2d(mstream->avg_frame_rate);
    if (fps <= 0.0)
        fps = av_q2d(mstream->r_frame_rate);
    msourceFps = fps;

    const AVCodec *codec = avcodec_find_decoder(mstream->codecpar->codec_id);
    if (!codec)
        return false;

    mdecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(mdecCtx, mstream->codecpar);

    mhwPixFmt = -1;
    const char *hwtypenames[] = {"cuda", "vaapi", "d3d11va", "dxva2", "videotoolbox", nullptr};
    for (const char **name = hwtypenames; *name; ++name) {
        enum AVHWDeviceType type = av_hwdevice_find_type_by_name(*name);
        if (type == AV_HWDEVICE_TYPE_NONE)
            continue;
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config)
                break;
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type) {
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
    if (!mhwDeviceCtx) {
        if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
            mdecCtx->thread_type = FF_THREAD_FRAME;
            mdecCtx->thread_count = Rina::Core::SettingsManager::instance().value("videoDecoderThreads", 0).toInt();
        } else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
            mdecCtx->thread_type = FF_THREAD_SLICE;
            mdecCtx->thread_count = Rina::Core::SettingsManager::instance().value("videoDecoderThreads", 0).toInt();
        }
    }

    if (avcodec_open2(mdecCtx, codec, nullptr) != 0)
        return false;
    if (!buildIndex())
        return false;
    return true;
}

bool VideoDecoder::buildIndex() {
    if (av_seek_frame(mfmtCtx, mstreamIndex, 0, AVSEEK_FLAG_BACKWARD) < 0)
        av_seek_frame(mfmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);

    if (mstream->nb_frames > 0)
        mindex.reserve(mstream->nb_frames);
    else
        mindex.reserve(SettingsManager::instance().value("videoDecoderIndexReserve", 108000).toInt());

    AVPacket *pkt = av_packet_alloc();
    while (av_read_frame(mfmtCtx, pkt) >= 0) {
        if (pkt->stream_index == mstreamIndex)
            mindex.push_back({pkt->pts, pkt->dts, bool(pkt->flags & AV_PKT_FLAG_KEY)});
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    std::sort(mindex.begin(), mindex.end(), [](const auto &a, const auto &b) {
        if (a.pts != AV_NOPTS_VALUE && b.pts != AV_NOPTS_VALUE)
            return a.pts < b.pts;
        return a.dts < b.dts;
    });
    return true;
}

void VideoDecoder::close() {
    if (mdecCtx)
        avcodec_free_context(&mdecCtx);
    mdecCtx = nullptr;
    if (mfmtCtx)
        avformat_close_input(&mfmtCtx);
    mfmtCtx = nullptr;
    if (mhwDeviceCtx)
        av_buffer_unref(&mhwDeviceCtx);
    mhwDeviceCtx = nullptr;
}

void VideoDecoder::seek(qint64 ms) { emit seekRequested(ms); }

double VideoDecoder::sourceFps() const { return msourceFps; }
int VideoDecoder::totalFrameCount() const { return int(mindex.size()); }

int VideoDecoder::frameIndexFromSeconds(double seconds) const {
    if (mindex.empty())
        return 0;
    const double tb = av_q2d(mtimeBase);
    if (tb <= 0.0) {
        double fps = msourceFps > 0.0 ? msourceFps : 30.0;
        int f = static_cast<int>(std::llround(seconds * fps));
        f = std::max(f, 0);
        f = std::min(f, int(mindex.size()) - 1);
        return f;
    }
    const int64_t targetPts = int64_t(std::llround(seconds / tb));
    auto it = std::lower_bound(mindex.begin(), mindex.end(), targetPts, [](const auto &e, int64_t v) { return e.pts < v; });
    int idx = int(std::distance(mindex.begin(), it));
    if (idx <= 0)
        return 0;
    if (idx >= int(mindex.size()))
        return int(mindex.size()) - 1;
    const int64_t a = mindex[idx - 1].pts;
    const int64_t b = mindex[idx].pts;
    return std::llabs(targetPts - a) <= std::llabs(b - targetPts) ? idx - 1 : idx;
}

void VideoDecoder::seekToTime(double seconds) {
    if (!m_isReady)
        return;
    if (seconds < 0.0)
        seconds = 0.0;
    const int frame = frameIndexFromSeconds(seconds);
    seekToFrame(frame, msourceFps);
}

void VideoDecoder::seekToFrame(int frame, double fps) {
    if (!m_isReady)
        return;
    if (frame < 0)
        return;
    mlastRequestedFrame.store(frame, std::memory_order_release);

    bool expected = false;
    if (!misDecoding.compare_exchange_strong(expected, true))
        return;

    mdecodeFuture = QtConcurrent::run([this, fps]() {
        while (!mclosing.load(std::memory_order_acquire)) {
            int targetFrame = mlastRequestedFrame.load(std::memory_order_acquire);
            decodeTask(targetFrame, fps);
            if (mlastRequestedFrame.load(std::memory_order_acquire) == targetFrame) {
                misDecoding.store(false, std::memory_order_release);
                if (mlastRequestedFrame.load(std::memory_order_acquire) != targetFrame) {
                    bool exp = false;
                    if (misDecoding.compare_exchange_strong(exp, true))
                        continue;
                }
                break;
            }
        }
    });
}

void VideoDecoder::decodeTask(int targetFrame, double fps) {
    QMutexLocker locker(&m_mutex);
    if (mclosing.load(std::memory_order_acquire))
        return;
    if (!mdecCtx || mindex.empty())
        return;

    targetFrame = std::max(targetFrame, 0);
    targetFrame = std::min(targetFrame, int(mindex.size()) - 1);
    if (mlastRequestedFrame != targetFrame)
        return;

    if (mframeCache.contains(targetFrame)) {
        mstore->setVideoFrameSafe(QString::number(clipId()), *mframeCache[targetFrame]);
        QMetaObject::invokeMethod(this, [this, targetFrame]() { emit frameReady(targetFrame); }, Qt::QueuedConnection);
        return;
    }

    const auto &targetEntry = mindex[targetFrame];
    int64_t targetPts = targetEntry.pts;
    bool needSeek = true;

    if (mlastDecodedFrame != -1 && targetFrame > mlastDecodedFrame && targetFrame <= mlastDecodedFrame + 120)
        needSeek = false;

    if (needSeek) {
        int keyIndex = targetFrame;
        while (keyIndex > 0 && !mindex[keyIndex].isKeyframe)
            --keyIndex;
        int64_t seekPts = mindex[keyIndex].pts;
        if (keyIndex == 0) {
            int64_t startTime = mfmtCtx->streams[mstreamIndex]->start_time;
            if (startTime != AV_NOPTS_VALUE)
                seekPts += startTime;
        }
        int ret = av_seek_frame(mfmtCtx, mstreamIndex, seekPts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0)
            av_seek_frame(mfmtCtx, -1, seekPts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(mdecCtx);
        mlastDecodedFrame = keyIndex - 1;
    }

    bool frameFound = false;
    AVPacket *pkt = av_packet_alloc();
    int maxDecodeCount = SettingsManager::instance().value("videoDecoderMaxPrefetchCount", 500).toInt();
    bool eof = false;

    while (maxDecodeCount-- > 0 && !frameFound) {
        int ret = 0;
        if (!eof) {
            ret = av_read_frame(mfmtCtx, pkt);
            if (ret < 0)
                eof = true;
        }
        if (eof)
            ret = avcodec_send_packet(mdecCtx, nullptr);
        else if (pkt->stream_index == mstreamIndex)
            ret = avcodec_send_packet(mdecCtx, pkt);
        if (!eof)
            av_packet_unref(pkt);

        while (ret >= 0 || ret == AVERROR(EAGAIN)) {
            int rxRet = avcodec_receive_frame(mdecCtx, mframe);
            if (rxRet == AVERROR(EAGAIN))
                break;
            if (rxRet == AVERROR_EOF) {
                eof = true;
                break;
            }
            if (rxRet < 0)
                break;
            int64_t currentPts = mframe->best_effort_timestamp != AV_NOPTS_VALUE ? mframe->best_effort_timestamp : mframe->pts;
            if (currentPts >= targetPts) {
                mlastDecodedFrame = targetFrame;
                frameFound = true;
                break;
            }
        }
    }
    av_packet_free(&pkt);

    if (frameFound) {
        AVFrame *srcFrame = mframe;
        AVFrame *swFrame = nullptr;

        if (mframe->format == mhwPixFmt) {
            swFrame = av_frame_alloc();
            if (av_hwframe_transfer_data(swFrame, mframe, 0) != 0) {
                av_frame_free(&swFrame);
                return;
            }
            srcFrame = swFrame;
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
        if (swFrame)
            av_frame_free(&swFrame);

        QVideoFrameFormat format(QSize(mdecCtx->width, mdecCtx->height), qtFmt);
        QVideoFrame videoFrame(new FFmpegVideoBuffer(ownedFrame, format), format);
        av_frame_free(&ownedFrame);

        videoFrame.setStartTime(-1);
        videoFrame.setEndTime(-1);

        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, videoFrame, targetFrame]() mutable {
                if (!self || self->mclosing.load(std::memory_order_acquire))
                    return;
                if (videoFrame.isValid()) {
                    self->mstore->setVideoFrameSafe(QString::number(self->clipId()), videoFrame);
                    int cost = videoFrame.width() * videoFrame.height();
                    self->mframeCache.insert(targetFrame, new QVideoFrame(videoFrame), cost);
                    emit self->frameReady(targetFrame);
                }
            },
            Qt::QueuedConnection);
    } else {
        QPointer<VideoDecoder> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, targetFrame]() {
                if (!self || self->mclosing.load(std::memory_order_acquire))
                    return;
                emit self->frameReady(targetFrame);
            },
            Qt::QueuedConnection);
    }
}

void VideoDecoder::updateCacheSize() {
    int sizeMB = SettingsManager::instance().settings().value("cacheSize", 512).toInt();
    int minSizeMB = SettingsManager::instance().value("videoDecoderMinCacheMB", 64).toInt();
    if (sizeMB < minSizeMB)
        sizeMB = minSizeMB;
    mframeCache.setMaxCost(sizeMB * 1024 * 1024);
}

void VideoDecoder::setPlaying(bool playing) {
    // 再生状態をスレッドセーフに更新
    misPlaying.store(playing, std::memory_order_release);
}

} // namespace Rina::Core
