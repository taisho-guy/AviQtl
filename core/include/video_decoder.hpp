#pragma once

#include "media_decoder.hpp"
#include <QByteArray>
#include <QCache>

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct AVBufferRef;
struct SwsContext;
#include <libavutil/pixfmt.h>
}

namespace Rina::Core {

class VideoFrameStore; // forward declaration
class VideoRenderItem; // forward declaration

class VideoDecoder : public MediaDecoder {
    Q_OBJECT
  public:
    explicit VideoDecoder(int clipId, const QUrl &source, VideoFrameStore *store, QObject *parent = nullptr);
    ~VideoDecoder() override;

    void seekToFrame(int frame, double fps);
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override {}
    double sourceFps() const { return m_sourceFps; }

  protected:
    void startDecoding() override;

    // Not used by video
    std::vector<float> getSamples(double startTime, int count) override { return {}; }

  signals:
    // VideoRenderItem へフレームデータを渡すためのシグナル
    void frameDecoded(QByteArray data, int width, int height);

  private:
    bool buildIndex(); // 全フレームスキャンしてインデックス構築

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
    double m_sourceFps = 60.0;

    // デコード結果の軽量キャッシュ（フレーム番号 → 生 RGBA バイト列）
    // インスタンスごとにキャッシュを持つ
    QCache<int, QByteArray> m_frameCache;
    std::atomic<int> m_lastRequestedFrame = -1; // Use std::atomic for thread-safe access

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
};
} // namespace Rina::Core