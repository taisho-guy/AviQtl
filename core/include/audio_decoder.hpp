#pragma once

#include "media_decoder.hpp"
#include <QFuture>
#include <atomic>
#include <mutex>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace Rina::Core {

class AudioDecoder : public MediaDecoder {
    Q_OBJECT
  public:
    explicit AudioDecoder(int clipId, const QUrl &source, QObject *parent = nullptr);

    ~AudioDecoder() override;

    void setSampleRate(int sampleRate) override;
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override;

    void getSamples(double startTime, int count, float *outBuffer) override;
    double totalDurationSec() const;
    std::vector<float> calculateWaveformPeaks(int pixelWidth, int samplesPerPixel) const;

  protected:
    void startDecoding() override;

  private:
    void closeFFmpeg();

    // FFmpeg コンテキスト
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVStream *m_stream = nullptr;
    int m_streamIdx = -1;
    SwrContext *m_swrCtx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_pkt = nullptr;

    std::vector<float> m_fullAudioData;
    QFuture<void> m_decodeFuture;
    std::atomic<bool> m_closing{false};
    std::atomic<bool> m_isPlaying{false}; // インターリーブされたPCMデータ (L, R, L, R...)
};

} // namespace Rina::Core