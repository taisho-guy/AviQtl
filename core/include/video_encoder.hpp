#pragma once

#include "gpu_texture_handle.hpp"
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>
#include <memory>
#include <mutex>

// FFmpeg forward declarations to keep header clean
extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct SwsContext;
struct SwrContext;
struct AVAudioFifo;
struct AVBufferRef;
}

namespace Rina::Core {

class VideoEncoder : public QObject {
    Q_OBJECT
  public:
    struct Config {
        int width;
        int height;
        int fps_num;
        int fps_den;
        int64_t bitrate = 15'000'000;
        int crf = -1;                     // -1 = bitrateモード, 0〜51 = CRFモード
        QString codecName = "h264_vaapi"; // AMD Radeon 780M on Linux
        QString audioCodecName = "aac";
        int64_t audioBitrate = 192'000;
        QString outputUrl;
        int startFrame = 0;
        int endFrame = -1; // -1 = タイムライン末尾まで
    };

    explicit VideoEncoder(QObject *parent = nullptr);
    ~VideoEncoder();

    bool open(const Config &config);
    Q_INVOKABLE bool open(const QVariantMap &config);
    bool pushFrame(const QImage &img, int64_t pts);                // CPU -> HW Upload
    bool pushTexture(const GpuTextureHandle &handle, int64_t pts); // GPU Texture -> HW Upload
    bool addAudioStream(int sampleRate = 48000, int channels = 2);
    bool pushAudio(const float *samples, int sampleCount);
    void close();

  private:
    Config m_config;
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_encCtx = nullptr;
    AVStream *m_stream = nullptr;
    AVFrame *m_hwFrame = nullptr; // For VA-API
    AVFrame *m_swFrame = nullptr; // For CPU staging
    SwsContext *m_swsCtx = nullptr;
    AVBufferRef *m_hwDeviceCtx = nullptr;

    // Audio
    AVStream *m_audioStream = nullptr;
    AVCodecContext *m_audioEncCtx = nullptr;
    SwrContext *m_swrCtx = nullptr;
    AVAudioFifo *m_audioFifo = nullptr;
    AVFrame *m_audioFrame = nullptr;
    int64_t m_audioPts = 0;
    bool m_headerWritten = false;
    std::mutex m_mutex;

    bool initHardware(const QString &codecName);
    bool writeHeaderIfNeeded();
    void cleanup();
};

} // namespace Rina::Core