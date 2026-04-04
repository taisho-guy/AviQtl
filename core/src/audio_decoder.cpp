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
    m_decodeFuture = QtConcurrent::run([this] -> void {
        closeFFmpeg();
        m_isReady = false;

        QString path = m_source.toLocalFile();
        if (path.isEmpty()) {
            path = m_source.toString();
        }

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
                auto *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());

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
            auto *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());
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
            auto *outPtr = reinterpret_cast<uint8_t *>(convertBuf.data());
            int flushed = swr_convert(m_swrCtx, &outPtr, remaining, nullptr, 0);
            if (flushed > 0) {
                QMutexLocker locker(&m_mutex);
                m_fullAudioData.insert(m_fullAudioData.end(), convertBuf.begin(), convertBuf.begin() + static_cast<ptrdiff_t>(flushed) * 2);
            }
        }

        buildPeakCache();

        qDebug() << "[AudioDecoder] clip" << m_clipId << "decoded. total samples:" << m_fullAudioData.size();
        m_isReady = true;
        emit ready();
    });
}

void AudioDecoder::closeFFmpeg() {
    if (m_frame != nullptr) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
    if (m_pkt != nullptr) {
        av_packet_free(&m_pkt);
        m_pkt = nullptr;
    }
    if (m_swrCtx != nullptr) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    if (m_decCtx != nullptr) {
        avcodec_free_context(&m_decCtx);
        m_decCtx = nullptr;
    }
    if (m_fmtCtx != nullptr) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    QMutexLocker locker(&m_mutex);
    m_fullAudioData.clear();
    m_peakPyramid.clear();
}

void AudioDecoder::setSampleRate(int sampleRate) {
    if (m_sampleRate == sampleRate) {
        return;
    }
    m_sampleRate = sampleRate;
    scheduleStart(); // サンプルレート変更時は再デコード
}

void AudioDecoder::seek(qint64 ms) { emit seekRequested(ms); }

auto AudioDecoder::getSamples(double startTime, int count) -> std::vector<float> { // NOLINT(bugprone-easily-swappable-parameters)
    QMutexLocker locker(&m_mutex);

    // startTimeが負数の場合のアンダーフローを防ぐ（size_tへのキャスト前にクランプ）
    startTime = std::max(startTime, 0.0);

    // startTime (秒) × m_sampleRate (サンプル/秒) × 2 (ステレオ 2ch)
    auto startIdx = static_cast<size_t>(startTime * m_sampleRate * 2);

    // 偶数アライメント（L/R がずれないように補正）
    if (startIdx % 2 != 0 && startIdx > 0) {
        startIdx--;
    }

    if (startIdx >= m_fullAudioData.size()) {
        return std::vector<float>(count, 0.0F);
    }

    size_t available = m_fullAudioData.size() - startIdx;
    size_t actualCount = std::min(static_cast<size_t>(count), available);

    std::vector<float> result(m_fullAudioData.begin() + static_cast<ptrdiff_t>(startIdx), m_fullAudioData.begin() + static_cast<ptrdiff_t>(startIdx + actualCount));

    // 足りない分は無音で埋める
    if (result.size() < static_cast<size_t>(count)) {
        result.resize(count, 0.0F);
    }

    return result;
}

void AudioDecoder::buildPeakCache() {
    QMutexLocker locker(&m_mutex);
    m_peakPyramid.clear();
    if (m_fullAudioData.empty()) {
        return;
    }

    int numSamples = static_cast<int>(m_fullAudioData.size() / 2);

    // Level 0: 32サンプルごとの最小・最大値 (高精度)
    PeakLevel base;
    base.samplesPerEntry = 32;
    base.peaks.reserve((numSamples / 32) + 1);

    for (int i = 0; i < numSamples; i += 32) {
        float pMin = 0.0F;
        float pMax = 0.0F;
        for (int j = 0; j < 32 && (i + j) < numSamples; ++j) {
            float l = m_fullAudioData[static_cast<std::size_t>(i + j) * 2];
            float r = m_fullAudioData[(static_cast<std::size_t>(i + j) * 2) + 1];
            pMin = std::min({pMin, l, r});
            pMax = std::max({pMax, l, r});
        }
        base.peaks.push_back({.min = pMin, .max = pMax});
    }
    m_peakPyramid.push_back(std::move(base));

    // Level 1-5: 前のレベルの8要素(8倍速)から最大・最小を抽出
    for (int i = 0; i < 5; ++i) {
        const auto &prev = m_peakPyramid.back();
        if (prev.peaks.size() < 8) {
            break;
        }

        PeakLevel next;
        next.samplesPerEntry = prev.samplesPerEntry * 8;
        next.peaks.reserve((prev.peaks.size() / 8) + 1);

        for (size_t j = 0; j < prev.peaks.size(); j += 8) {
            float pMin = 0.0F;
            float pMax = 0.0F;
            for (size_t k = 0; k < 8 && (j + k) < prev.peaks.size(); ++k) {
                pMin = std::min(pMin, prev.peaks[j + k].min);
                pMax = std::max(pMax, prev.peaks[j + k].max);
            }
            next.peaks.push_back({.min = pMin, .max = pMax});
        }
        m_peakPyramid.push_back(std::move(next));
    }
}

auto AudioDecoder::getPeaks(double startSec, double durationSec, int pixelWidth) -> std::vector<float> {
    QMutexLocker locker(&m_mutex);
    if (pixelWidth <= 0) {
        return {};
    }
    if (m_fullAudioData.empty()) {
        return std::vector<float>(static_cast<std::size_t>(pixelWidth) * 2, 0.0F);
    }

    double totalSamplesInView = durationSec * m_sampleRate;
    double samplesPerPixel = totalSamplesInView / pixelWidth;

    std::vector<float> result;
    result.reserve(static_cast<std::size_t>(pixelWidth) * 2);

    if (samplesPerPixel < 32.0) {
        // 超高精度: キャッシュレベルを超えたズーム時は生データを直接スキャン
        int numSamples = static_cast<int>(m_fullAudioData.size() / 2);
        for (int i = 0; i < pixelWidth; ++i) {
            int idxStart = std::clamp(static_cast<int>((startSec + (durationSec * i / pixelWidth)) * m_sampleRate), 0, numSamples - 1);
            int idxEnd = std::clamp(static_cast<int>((startSec + (durationSec * (i + 1) / pixelWidth)) * m_sampleRate), idxStart + 1, numSamples);
            float pMin = 0.0F;
            float pMax = 0.0F;
            for (int j = idxStart; j < idxEnd; ++j) {
                pMin = std::min({pMin, m_fullAudioData[static_cast<std::size_t>(j) * 2], m_fullAudioData[(static_cast<std::size_t>(j) * 2) + 1]});
                pMax = std::max({pMax, m_fullAudioData[static_cast<std::size_t>(j) * 2], m_fullAudioData[(static_cast<std::size_t>(j) * 2) + 1]});
            }
            result.push_back(pMin);
            result.push_back(pMax);
        }
    } else {
        // 通常・広域表示: ピラミッドキャッシュを使用
        size_t levelIdx = 0;
        for (size_t i = 0; i < m_peakPyramid.size(); ++i) {
            if (m_peakPyramid[i].samplesPerEntry <= samplesPerPixel) {
                levelIdx = i;
            } else {
                break;
            }
        }
        const auto &level = m_peakPyramid[levelIdx];
        for (int i = 0; i < pixelWidth; ++i) {
            auto entryIdx = static_cast<size_t>(((startSec + (durationSec * i / pixelWidth)) * m_sampleRate) / level.samplesPerEntry);
            if (entryIdx < level.peaks.size()) {
                result.push_back(level.peaks[entryIdx].min);
                result.push_back(level.peaks[entryIdx].max);
            } else {
                result.push_back(0.0F);
                result.push_back(0.0F);
            }
        }
    }
    return result;
}

auto AudioDecoder::totalDurationSec() const -> double {
    QMutexLocker locker(&m_mutex);
    if (m_sampleRate <= 0) {
        return 0.0;
    }
    const double frames = static_cast<double>(m_fullAudioData.size()) / 2.0;
    return frames / static_cast<double>(m_sampleRate);
}

void AudioDecoder::setPlaying(bool playing) {
    // 再生状態をスレッドセーフに更新
    m_isPlaying.store(playing, std::memory_order_release);
}

} // namespace Rina::Core
