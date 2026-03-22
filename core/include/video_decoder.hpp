#pragma once
#include "ffmpeg_video_buffer.hpp"
#include "media_decoder.hpp"
#include <QCache>
#include <QFuture>
#include <QVideoFrame>

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct AVBufferRef;
struct SwsContext;
}
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>

namespace Rina::Core {

class VideoFrameStore;

class VideoDecoder : public Rina::Core::MediaDecoder {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);
    ~VideoDecoder() override;

    void seekToFrame(int frame, double fps);
    void seekToTime(double seconds);
    double sourceFps() const;
    int totalFrameCount() const;
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override;

  signals:
    void videoMetaReady(int totalFrameCount, double sourceFps);

  protected:
    void startDecoding() override;
    std::vector<float> getSamples(double startTime, int count) override { return {}; }

  private:
    bool buildIndex();
    int frameIndexFromSeconds(double seconds) const;

    struct FrameIndexEntry {
        int64_t pts;
        int64_t dts;
        bool isKeyframe;
    };

    void decodeTask(int targetFrame, double fps);
    bool open(const QString &path);
    void close();
    void updateCacheSize();

    VideoFrameStore *mstore = nullptr;

    AVFormatContext *mfmtCtx = nullptr;
    AVCodecContext *mdecCtx = nullptr;
    AVStream *mstream = nullptr;
    int mstreamIndex = -1;
    AVFrame *mframe = nullptr;
    SwsContext *mswsCtx = nullptr;
    AVBufferRef *mhwDeviceCtx = nullptr;
    int mhwPixFmt = -1; // AV_PIX_FMT_NONE

    int mlastDecodedFrame = -1;
    std::vector<FrameIndexEntry> mindex;
    QCache<int, QVideoFrame> mframeCache;
    std::atomic<int> mlastRequestedFrame{-1};
    std::atomic<bool> mclosing{false};
    std::atomic<bool> misPlaying{false};
    std::atomic<bool> misDecoding{false};
    QFuture<void> minitFuture;
    QFuture<void> mdecodeFuture;
    double msourceFps = 0.0;
    AVRational mtimeBase{0, 1};

    static enum AVPixelFormat gethwformat(AVCodecContext *ctx, const enum AVPixelFormat *pixfmts);
};

} // namespace Rina::Core
