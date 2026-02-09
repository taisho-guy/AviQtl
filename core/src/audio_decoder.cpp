#include "audio_decoder.hpp"
#include <QAudioBuffer>
#include <QDebug>

namespace Rina::Core {

AudioDecoder::AudioDecoder(int clipId, const QUrl &source, QObject *parent)
    : QObject(parent), m_clipId(clipId) {

    m_decoder = new QAudioDecoder(this);

    // プロジェクト標準の音声フォーマット（48kHz, Stereo, Float）
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);

    m_decoder->setAudioFormat(format);
    m_decoder->setSource(source);

    connect(m_decoder, &QAudioDecoder::bufferReady, this, &AudioDecoder::onBufferReady);
    connect(m_decoder, &QAudioDecoder::finished, this, &AudioDecoder::onFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this, [this](QAudioDecoder::Error error){
        qWarning() << "[AudioDecoder] Error occurred:" << error;
        onError(error);
    });

    qDebug() << "[AudioDecoder] Decoding started for clip" << clipId << source;
    m_decoder->start();
}

void AudioDecoder::onBufferReady() {
    QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid())
        return;

    // 要求したフォーマット(Float)で来ていると仮定
    // Qt6のQAudioDecoderはsetAudioFormatで指定した形式への変換を試みます
    const float *data = buffer.constData<float>();
    int sampleCount = buffer.sampleCount();

    if (data) {
        m_fullAudioData.insert(m_fullAudioData.end(), data, data + sampleCount);
    }
}

void AudioDecoder::onFinished() {
    m_isReady = true;
    qDebug() << "[AudioDecoder] Decoding finished. Total samples:" << m_fullAudioData.size();
    emit ready();
}

void AudioDecoder::onError(QAudioDecoder::Error error) {
    qWarning() << "[AudioDecoder] Error occurred:" << error << m_decoder->errorString();
}

std::vector<float> AudioDecoder::getSamples(double startTime, int count) {
    // 48kHz, Stereo (2ch)
    // startTime (秒) * 48000 (サンプル/秒) * 2 (チャンネル)
    size_t startIdx = static_cast<size_t>(startTime * 48000 * 2);

    // 偶数アライメント（ステレオのL/Rがずれないように補正）
    if (startIdx % 2 != 0) startIdx--;

    if (startIdx >= m_fullAudioData.size()) {
        return std::vector<float>(count, 0.0f);
    }

    size_t available = m_fullAudioData.size() - startIdx;
    size_t actualCount = std::min(static_cast<size_t>(count), available);

    std::vector<float> result(m_fullAudioData.begin() + startIdx,
                              m_fullAudioData.begin() + startIdx + actualCount);

    // 足りない分は0埋め（無音）
    if (result.size() < static_cast<size_t>(count)) {
        result.resize(count, 0.0f);
    }

    return result;
}

} // namespace Rina::Core