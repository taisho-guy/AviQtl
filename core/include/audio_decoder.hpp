#pragma once

#include <QAudioDecoder>
#include <QObject>
#include <QUrl>
#include <mutex>
#include <vector>

namespace Rina::Core {

class AudioDecoder : public QObject {
    Q_OBJECT
  public:
    explicit AudioDecoder(int clipId, const QUrl &source, QObject *parent = nullptr);

    // 指定された開始時間（秒）から count サンプル分を取得
    // ステレオ(2ch)インターリーブされたデータを返す想定
    // count は (フレーム数 * チャンネル数)
    std::vector<float> getSamples(double startTime, int count);

    bool isReady() const { return m_isReady; }

    void setSampleRate(int sampleRate);
  public slots:
    void seek(qint64 ms);

  signals:
    void ready();
    void seekRequested(qint64 ms);

  private slots:
    void onBufferReady();
    void onFinished();
    void onError(QAudioDecoder::Error error);

  private:
    void startDecoding();
    int m_clipId;
    QAudioDecoder *m_decoder;
    std::vector<float> m_fullAudioData; // インターリーブされたPCMデータ (L, R, L, R...)
    bool m_isReady = false;
    mutable std::mutex m_mutex;
    int m_sampleRate = 48000;
};

} // namespace Rina::Core