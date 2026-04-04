#pragma once
#include <QAbstractVideoBuffer>
#include <QVideoFrame>
#include <QVideoFrameFormat>

extern "C" {
#include <libavutil/frame.h>
}

namespace Rina::Core {

class FFmpegVideoBuffer final : public QAbstractVideoBuffer {
  public:
    FFmpegVideoBuffer(const FFmpegVideoBuffer &) = delete;
    FFmpegVideoBuffer &operator=(const FFmpegVideoBuffer &) = delete;
    FFmpegVideoBuffer(AVFrame *frame, const QVideoFrameFormat &format) : m_frame(av_frame_alloc()), m_format(format) { av_frame_ref(m_frame, frame); }

    ~FFmpegVideoBuffer() override { av_frame_free(&m_frame); }

    QVideoFrameFormat format() const override { return m_format; }

    MapData map(QVideoFrame::MapMode) override {
        MapData d;
        d.planeCount = 0;
        for (int i = 0; i < AV_NUM_DATA_POINTERS && m_frame->data[i]; ++i) {
            d.data[i] = m_frame->data[i];
            d.bytesPerLine[i] = m_frame->linesize[i];
            d.dataSize[i] = m_frame->linesize[i] * (i == 0 ? m_frame->height : m_frame->height / 2);
            ++d.planeCount;
        }
        return d;
    }

    void unmap() override {}

  private:
    AVFrame *m_frame = nullptr;
    QVideoFrameFormat m_format;
};

} // namespace Rina::Core
