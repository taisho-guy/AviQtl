#include "audio_decoder.hpp"
#include "settings_manager.hpp"
#include <QAudioBuffer>
#include <QDebug>

namespace Rina::Core {

AudioDecoder::AudioDecoder(int clipId, const QUrl &source, QObject *parent) : QObject(parent), m_clipId(clipId) {

    // プロジェクト設定からサンプリングレートを取得
    m_sampleRate = SettingsManager::instance().value("_runtime_projectSampleRate", 48000).toInt();

    m_decoder = new QAudioDecoder(this);
    m_decoder->setSource(source);

    startDecoding();
}

void AudioDecoder::startDecoding() {
    m_isReady = false;
    m_fullAudioData.clear();

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);

    m_decoder->setAudioFormat(format);

    // 修正: ループ時のデコーダリセット処理を削除。
    // AudioDecoderはファイルを一度だけ全読み込みし、メモリ上に保持し続ける設計とします。
    // これにより、デコードが遅い場合でもリセットされず、完了次第音が鳴るようになります。

    connect(m_decoder, &QAudioDecoder::bufferReady, this, &AudioDecoder::onBufferReady);
    connect(m_decoder, &QAudioDecoder::finished, this, &AudioDecoder::onFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this, [this](QAudioDecoder::Error error) {
        qWarning() << "[AudioDecoder] Error occurred:" << error;
        onError(error);
    });

    qDebug() << "[AudioDecoder] Decoding started for clip" << m_clipId << "Rate:" << m_sampleRate;
    m_decoder->start();
}

void AudioDecoder::setSampleRate(int sampleRate) {
    if (m_sampleRate == sampleRate)
        return;

    m_sampleRate = sampleRate;
    m_decoder->stop();
    // 再デコード開始
    startDecoding();
}

void AudioDecoder::seek(qint64 ms) { emit seekRequested(ms); }

void AudioDecoder::onBufferReady() {
    QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid())
        return;

    const int sampleCount = buffer.sampleCount();
    const auto fmt = buffer.format();

    // 初回のみフォーマットをログ出力
    static bool loggedFormat = false;
    if (!loggedFormat) {
        qDebug() << "[AudioDecoder] Received buffer format:" << fmt;
        loggedFormat = true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // フォーマットに応じてFloatへ変換して格納
    if (fmt.sampleFormat() == QAudioFormat::Float) {
        const float *data = buffer.constData<float>();
        if (data)
            m_fullAudioData.insert(m_fullAudioData.end(), data, data + sampleCount);
    } else if (fmt.sampleFormat() == QAudioFormat::Int16) {
        const qint16 *data = buffer.constData<qint16>();
        if (data) {
            m_fullAudioData.reserve(m_fullAudioData.size() + sampleCount);
            for (int i = 0; i < sampleCount; ++i) {
                // 16bit int -> float
                m_fullAudioData.push_back(static_cast<float>(data[i]) / 32768.0f);
            }
        }
    } else {
        qWarning() << "[AudioDecoder] Unsupported sample format:" << fmt.sampleFormat();
    }
}

void AudioDecoder::onFinished() {
    m_isReady = true;
    qDebug() << "[AudioDecoder] Decoding finished. Total samples:" << m_fullAudioData.size();
    emit ready();
}

void AudioDecoder::onError(QAudioDecoder::Error error) {
    qWarning() << "[AudioDecoder] Error occurred:" << error << m_decoder->errorString();
    // エラー時もReadyとしてマークし、無限待ちを回避する
    m_isReady = true;
    emit ready();
}

std::vector<float> AudioDecoder::getSamples(double startTime, int count) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // m_sampleRate, Stereo (2ch)
    // startTime (秒) * m_sampleRate (サンプル/秒) * 2 (チャンネル)
    size_t startIdx = static_cast<size_t>(startTime * m_sampleRate * 2);

    // 偶数アライメント（ステレオのL/Rがずれないように補正）
    if (startIdx % 2 != 0)
        startIdx--;

    if (startIdx >= m_fullAudioData.size()) {
        return std::vector<float>(count, 0.0f);
    }

    size_t available = m_fullAudioData.size() - startIdx;
    size_t actualCount = std::min(static_cast<size_t>(count), available);

    std::vector<float> result(m_fullAudioData.begin() + startIdx, m_fullAudioData.begin() + startIdx + actualCount);

    // 足りない分は0埋め（無音）
    if (result.size() < static_cast<size_t>(count)) {
        result.resize(count, 0.0f);
    }

    return result;
}

} // namespace Rina::Core