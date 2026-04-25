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

namespace AviQtl::Core {

class AudioDecoder : public MediaDecoder {
    Q_OBJECT
  public:
    explicit AudioDecoder(int clipId, const QUrl &source, QObject *parent = nullptr);

    ~AudioDecoder() override;

    void setSampleRate(int sampleRate) override;
    void seek(qint64 ms) override;
    void setPlaying(bool playing) override;

    std::vector<float> getSamples(double startTime, int count) override;
    std::vector<float> getPeaks(double startSec, double durationSec, int pixelWidth);
    double totalDurationSec() const;

  protected:
    void startDecoding() override;

  private:
    void closeFFmpeg();
    void buildPeakCache();

    struct PeakEntry {
        float min;
        float max;
    };
    struct PeakLevel {
        int samplesPerEntry;
        std::vector<PeakEntry> peaks;
    };

    // FFmpeg コンテキスト
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    AVStream *m_stream = nullptr;
    int m_streamIdx = -1;
    SwrContext *m_swrCtx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_pkt = nullptr;

    std::vector<float> m_fullAudioData;
    std::vector<PeakLevel> m_peakPyramid;
    QFuture<void> m_decodeFuture;
    std::atomic<bool> m_closing{false};
    std::atomic<bool> m_isPlaying{false}; // インターリーブされたPCMデータ (L, R, L, R...)
};

} // namespace AviQtl::Core