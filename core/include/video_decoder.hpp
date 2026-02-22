#pragma once

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QUrl>
#include <atomic>

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct AVBufferRef;
#include <libavutil/pixfmt.h>
}

namespace Rina::Core {

class VideoFrameStore; // forward declaration

class VideoDecoder : public QObject {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);
    ~VideoDecoder();

    Q_INVOKABLE void seekToFrame(int frame, double fps);
    Q_INVOKABLE void setPlaying(bool playing) {}     // 互換性維持のためのスタブ
    Q_INVOKABLE void setPlaybackRate(double rate) {} // 互換性維持のためのスタブ

  signals:
    void frameDecoded(int frame);

  private:
    bool open(const QString &path);
    void close();
    bool buildIndex(); // 全フレームスキャンしてインデックス構築

    struct FrameIndexEntry {
        int64_t pts;
        int64_t dts;
        bool isKeyframe;
    };
    void decodeTask(int targetFrame, double fps);

    void updateCacheSize();

    int m_clipId;
    VideoFrameStore *m_store;

    // FFmpeg Contexts
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVStream *m_stream = nullptr;
    int m_streamIndex = -1;
    AVFrame *m_frame = nullptr;
    AVFrame *m_swFrame = nullptr; // HW download用
    AVPacket *m_pkt = nullptr;
    SwsContext *m_swsCtx = nullptr;
    AVBufferRef *m_hwDeviceCtx = nullptr;
    int m_hwPixFmt = -1; // AV_PIX_FMT_NONE

    // State
    int m_lastDecodedFrame = -1;
    QMutex m_mutex;
    std::vector<FrameIndexEntry> m_index; // フレーム番号 -> PTS/Keyframe マップ

    // LRUキャッシュ: Key="frameNumber", Value=QImage
    // インスタンスごとにキャッシュを持つ
    QCache<int, QImage> m_frameCache;
    std::atomic<int> m_lastRequestedFrame = -1;

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
};
} // namespace Rina::Core