#include "audio_decoder.hpp"
#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

namespace Rina::Core {

AudioDecoder::AudioDecoder(int clipId, const QUrl &source, QObject *parent) : MediaDecoder(clipId, source, parent) {
    // 生成とデコード開始を分離: 呼び出し元が scheduleStart() を呼ぶ
}

AudioDecoder::~AudioDecoder() {
    m_closing.store(true, std::memory_order_release);
    if (m_decodeFuture.isRunning()) {
        m_decodeFuture.waitForFinished();
    }
    closeFFmpeg();
}

void AudioDecoder::startDecoding() {
    // UIスレッドをブロックしないようバックグラウンドで全デコード
    m_decodeFuture = QtConcurrent::run([this] {
        closeFFmpeg();
        m_isReady = false;

        QString path = m_source.toLocalFile();
        if (path.isEmpty())
            path = m_source.toString();

        if (avformat_open_input(&m_fmtCtx, path.toStdString().c_str(), nullptr, nullptr) < 0) {
            qWarning() << "[AudioDecoder] avformat_open_input failed:" << path;
            m_isReady = true;
            emit ready();
            return;
        }

        avformat_find_stream_info(m_fmtCtx, nullptr);

        m_streamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (m_streamIdx < 0) {
            qWarning() << "[AudioDecoder] 音声ストリームが見つかりません:" << path;
            m_isReady = true;
            emit ready();
            return;
        }
        m_stream = m_fmtCtx->streams[m_streamIdx];

        const AVCodec *codec = avcodec_find_decoder(m_stream->codecpar->codec_id);
        if (!codec) {
            qWarning() << "[AudioDecoder] 対応コーデックが見つかりません";
            m_isReady = true;
            emit ready();
            return;
        }

        m_decCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(m_decCtx, m_stream->codecpar);
        if (avcodec_open2(m_decCtx, codec, nullptr) < 0) {
            qWarning() << "[AudioDecoder] avcodec_open2 failed";
            m_isReady = true;
            emit ready();
            return;
        }

        // swresample: デコード出力 → Float32 ステレオ へ変換
        m_swrCtx = swr_alloc();
        av_opt_set_chlayout(m_swrCtx, "in_chlayout", &m_decCtx->ch_layout, 0);
        av_opt_set_int(m_swrCtx, "in_sample_rate", m_decCtx->sample_rate, 0);
        av_opt_set_sample_fmt(m_swrCtx, "in_sample_fmt", m_decCtx->sample_fmt, 0);
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_opt_set_chlayout(m_swrCtx, "out_chlayout", &stereo, 0);
        av_opt_set_int(m_swrCtx, "out_sample_rate", m_sampleRate, 0);
        av_opt_set_sample_fmt(m_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        swr_init(m_swrCtx);

        m_frame = av_frame_alloc();
        m_pkt = av_packet_alloc();

        // インターリーブ Float32 ステレオ バッファ
        std::vector<float> convertBuf;

        while (av_read_frame(m_fmtCtx, m_pkt) >= 0) {
            if (m_closing.load(std::memory_order_acquire)) {
                av_packet_unref(m_pkt);
                break;
            }
            if (m_pkt->stream_index != m_streamIdx) {
                av_packet_unref(m_pkt);
                continue;
            }

            if (avcodec_send_packet(m_decCtx, m_pkt) < 0) {
                av_packet_unref(m_pkt);
                continue;
            }
            av_packet_unref(m_pkt);

            while (avcodec_receive_frame(m_decCtx, m_frame) == 0) {
                // 変換後サンプル数を算出（切り上げ）
                int outSamples = static_cast<int>(av_rescale_rnd(m_frame->nb_samples, m_sampleRate, m_decCtx->sample_rate, AV_ROUND_UP));

                // Float32 ステレオ = 1サンプル × 2ch
                convertBuf.resize(static_cast<size_t>(outSamples) * 2);
                uint8_t *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());

                int converted = swr_convert(m_swrCtx, &outPtr, outSamples, const_cast<const uint8_t **>(m_frame->data), m_frame->nb_samples);

                if (converted > 0) {
                    QMutexLocker locker(&m_mutex);
                    m_fullAudioData.insert(m_fullAudioData.end(), convertBuf.begin(), convertBuf.begin() + static_cast<ptrdiff_t>(converted) * 2);
                }
                av_frame_unref(m_frame);
            }
        }

        // デコーダフラッシュ（末尾フレームを取りこぼさない）
        avcodec_send_packet(m_decCtx, nullptr);
        while (avcodec_receive_frame(m_decCtx, m_frame) == 0) {
            int outSamples = static_cast<int>(av_rescale_rnd(m_frame->nb_samples, m_sampleRate, m_decCtx->sample_rate, AV_ROUND_UP));
            convertBuf.resize(static_cast<size_t>(outSamples) * 2);
            uint8_t *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());
            int converted = swr_convert(m_swrCtx, &outPtr, outSamples, const_cast<const uint8_t **>(m_frame->data), m_frame->nb_samples);
            if (converted > 0) {
                QMutexLocker locker(&m_mutex);
                m_fullAudioData.insert(m_fullAudioData.end(), convertBuf.begin(), convertBuf.begin() + static_cast<ptrdiff_t>(converted) * 2);
            }
            av_frame_unref(m_frame);
        }

        // swrフラッシュ（リサンプラー内部バッファを吐き出す）
        int remaining = static_cast<int>(swr_get_delay(m_swrCtx, m_sampleRate) + 1);
        if (remaining > 0) {
            convertBuf.resize(static_cast<size_t>(remaining) * 2);
            uint8_t *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());
            int flushed = swr_convert(m_swrCtx, &outPtr, remaining, nullptr, 0);
            if (flushed > 0) {
                QMutexLocker locker(&m_mutex);
                m_fullAudioData.insert(m_fullAudioData.end(), convertBuf.begin(), convertBuf.begin() + static_cast<ptrdiff_t>(flushed) * 2);
            }
        }

        qDebug() << "[AudioDecoder] clip" << m_clipId << "decoded. total samples:" << m_fullAudioData.size();
        m_isReady = true;
        emit ready();
    });
}

void AudioDecoder::closeFFmpeg() {
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
    if (m_pkt) {
        av_packet_free(&m_pkt);
        m_pkt = nullptr;
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
        m_decCtx = nullptr;
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    QMutexLocker locker(&m_mutex);
    m_fullAudioData.clear();
}

void AudioDecoder::setSampleRate(int sampleRate) {
    if (m_sampleRate == sampleRate)
        return;
    m_sampleRate = sampleRate;
    scheduleStart(); // サンプルレート変更時は再デコード
}

void AudioDecoder::seek(qint64 ms) { emit seekRequested(ms); }

void AudioDecoder::getSamples(double startTime, int count, float *outBuffer) {
    QMutexLocker locker(&m_mutex);

    // startTimeが負数の場合のアンダーフローを防ぐ（size_tへのキャスト前にクランプ）
    if (startTime < 0.0)
        startTime = 0.0;

    size_t startIdx = static_cast<size_t>(startTime * m_sampleRate * 2);

    // 偶数アライメント（L/R がずれないように補正）
    if (startIdx % 2 != 0 && startIdx > 0)
        startIdx--;

    // 範囲外の場合は無音で埋めて終了
    if (startIdx >= m_fullAudioData.size()) {
        std::fill(outBuffer, outBuffer + count, 0.0f);
        return;
    }

    size_t available = m_fullAudioData.size() - startIdx;
    size_t actualCount = std::min(static_cast<size_t>(count), available);

    // 有効な音声データをバッファへコピーする
    std::copy(m_fullAudioData.begin() + startIdx, m_fullAudioData.begin() + startIdx + actualCount, outBuffer);

    // 足りない分は無音で埋める
    if (actualCount < static_cast<size_t>(count))
        std::fill(outBuffer + actualCount, outBuffer + count, 0.0f);
}

double AudioDecoder::totalDurationSec() const {
    QMutexLocker locker(&m_mutex);
    if (m_sampleRate <= 0)
        return 0.0;
    const double frames = static_cast<double>(m_fullAudioData.size()) / 2.0;
    return frames / static_cast<double>(m_sampleRate);
}

void AudioDecoder::setPlaying(bool playing) {
    // 再生状態をスレッドセーフに更新
    m_isPlaying.store(playing, std::memory_order_release);
}

std::vector<float> AudioDecoder::calculateWaveformPeaks(int pixelWidth, int samplesPerPixel) const {
    // データ保護のため一度だけロックを取得する (constメソッド内でのmutex操作のためconst_castを使用)
    QMutexLocker locker(const_cast<QMutex *>(&m_mutex));
    std::vector<float> peaks;
    peaks.reserve(pixelWidth);

    for (int px = 0; px < pixelWidth; ++px) {
        size_t startIdx = static_cast<size_t>(px * samplesPerPixel * 2);
        // ステレオデータの左チャンネルに合わせる
        if (startIdx % 2 != 0 && startIdx > 0)
            startIdx--;
        size_t endIdx = startIdx + samplesPerPixel * 2;

        float peak = 0.0f;
        for (size_t i = startIdx; i < endIdx && i < m_fullAudioData.size(); ++i) {
            peak = std::max(peak, std::abs(m_fullAudioData[i]));
        }
        peaks.push_back(std::min(peak, 1.0f));
    }
    return peaks;
}

} // namespace Rina::Core
