#pragma once
#include <QMutex>
#include <QObject>
#include <QUrl>
#include <vector>

namespace AviQtl::Core {

class MediaDecoder : public QObject {
    Q_OBJECT
  public:
    explicit MediaDecoder(int clipId, QUrl source, QObject *parent = nullptr);
    virtual ~MediaDecoder() = default;

    // 共通 API
    void scheduleStart();
    virtual void setSampleRate(int sampleRate) {}
    virtual void seek(qint64 ms) = 0;
    virtual void setPlaying(bool playing) = 0;
    virtual void setPlaybackRate(double rate) {}

    // Audio-specific method, with a default implementation for video.
    virtual std::vector<float> getSamples(double startTime, int count) { return {}; }

    bool isReady() const { return m_isReady; }
    int clipId() const { return m_clipId; }
    QUrl source() const { return m_source; }

  signals:
    void ready();
    void seekRequested(qint64 ms);
    void frameReady(int frameNum);
    void frameError(int frameNum);

  protected:
    // 派生クラスで実装: scheduleStart() から QueuedConnection で呼ばれる
    virtual void startDecoding() = 0;

    int m_clipId;
    QUrl m_source;
    int m_sampleRate = 48000;
    std::atomic<bool> m_isReady{false};
    mutable QMutex m_mutex;
};

} // namespace AviQtl::Core