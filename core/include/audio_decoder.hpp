#pragma once

#include "media_decoder.hpp"
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
    void setPlaying(bool) override {}

    std::vector<float> getSamples(double startTime, int count) override;

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

    std::vector<float> m_fullAudioData; // インターリーブされたPCMデータ (L, R, L, R...)
};

} // namespace Rina::Core