#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>
#include <memory>

// FFmpeg forward declarations to keep header clean
extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct SwsContext;
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
        int bitrate;
        QString codecName = "h264_vaapi"; // AMD Radeon 780M on Linux
        QString outputUrl;
    };

    explicit VideoEncoder(QObject *parent = nullptr);
    ~VideoEncoder();

    bool open(const Config &config);
    Q_INVOKABLE bool open(const QVariantMap &config);
    bool pushFrame(const QImage &img, int64_t pts);                           // Temporary: CPU -> HW Upload
    bool pushTexture(unsigned int textureId, const QSize &size, int64_t pts); // GL Texture -> HW Upload
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

    bool initHardware(const QString &codecName);
    void cleanup();
};

} // namespace Rina::Core