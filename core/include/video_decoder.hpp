#pragma once

#include "media_decoder.hpp"
#include <QCache>
#include <QImage>

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct AVBufferRef;
struct SwsContext;
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace Rina::Core {

class VideoFrameStore; // forward declaration

class VideoDecoder : public MediaDecoder {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);
    ~VideoDecoder() override;

    void seekToFrame(int frame, double fps);
    void seekToTime(double seconds);
    double sourceFps() const;
    int totalFrameCount() const;
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override {}

  signals:
    void videoMetaReady(int totalFrameCount, double sourceFps);

  protected:
    void startDecoding() override;

    // Not used by video
    std::vector<float> getSamples(double startTime, int count) override { return {}; }

  private:
    bool buildIndex();
    int frameIndexFromSeconds(double seconds) const; // 全フレームスキャンしてインデックス構築

    struct FrameIndexEntry {
        int64_t pts;
        int64_t dts;
        bool isKeyframe;
    };
    void decodeTask(int targetFrame, double fps);
    bool open(const QString &path);
    void close();

    void updateCacheSize();

    VideoFrameStore *m_store = nullptr;

    // FFmpeg Contexts
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVStream *m_stream = nullptr;
    int m_streamIndex = -1;
    AVFrame *m_frame = nullptr;
    AVFrame *m_swFrame = nullptr; // HW download用
    SwsContext *m_swsCtx = nullptr;
    AVBufferRef *m_hwDeviceCtx = nullptr;
    int m_hwPixFmt = -1; // AV_PIX_FMT_NONE

    // State
    int m_lastDecodedFrame = -1;
    std::vector<FrameIndexEntry> m_index; // フレーム番号 -> PTS/Keyframe マップ

    // LRUキャッシュ: Key="frameNumber", Value=QImage
    // インスタンスごとにキャッシュを持つ
    QCache<int, QImage> m_frameCache;
    std::atomic<int> m_lastRequestedFrame = -1; // Use std::atomic for thread-safe access
    double m_sourceFps = 0.0;
    AVRational m_timeBase{0, 1};

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
};
} // namespace Rina::Core