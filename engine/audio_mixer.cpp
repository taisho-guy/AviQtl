#include "audio_mixer.hpp"
#include "core/include/audio_decoder.hpp"
#include "core/include/settings_manager.hpp"
#include "engine/timeline/ecs.hpp"
#include <QAudioFormat>
#include <QDebug>
#include <QMediaDevices>
#include <vector>

namespace Rina::Engine {

void AudioMixer::processChain(float *buffer, int samples, const Plugin::AudioPluginChain &chain) {
    for (int i = 0; i < chain.count(); ++i) {
        Plugin::IAudioPlugin *plugin = chain.get(i);
        if (plugin && plugin->active()) {
            plugin->process(buffer, samples);
        }
    }
}

AudioMixer::AudioMixer(QObject *parent) : QObject(parent) {
    int sampleRate = Rina::Core::SettingsManager::instance().value("_runtime_projectSampleRate", 48000).toInt();
    m_format.setSampleRate(sampleRate);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Float);

    // 4.1 すべてのclipIdに対してPluginChainを初期化
    auto state = Timeline::ECS::instance().getSnapshot();
    if (state) {
        const auto &audioStates = state->audioStates;
        for (const auto &[clipId, audio] : audioStates) {
            if (!m_chains.contains(clipId)) {
                m_chains[clipId] = std::make_shared<Plugin::AudioPluginChain>();
            }
        }
    }

    // 4.2 既存のデコーダーを登録
    for (const auto &[clipId, decoder] : m_decoders) {
        registerDecoder(clipId, decoder);
    }

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(m_format)) {
        qWarning() << "Default audio format not supported, using preferred format.";
        m_format = device.preferredFormat();
    }

    m_audioSink = new QAudioSink(device, m_format, this);
    // 低レイテンシを目指しつつ、音飛びしない程度のバッファサイズ (例: 100ms)
    m_audioSink->setBufferSize(sampleRate * 2 * sizeof(float) / 10);
    m_audioOutput = m_audioSink->start();
    if (!m_audioOutput) {
        qWarning() << "[AudioMixer] Failed to start audio output! Device:" << device.description();
    }
}

void AudioMixer::mix(float *output, const float *input, float volume, int samples) {
    for (int i = 0; i < samples; ++i) {
        output[i] += input[i] * volume;
    }
}

void AudioMixer::setSampleRate(int sampleRate) {
    if (m_format.sampleRate() == sampleRate)
        return;

    qDebug() << "[AudioMixer] Changing sample rate to" << sampleRate;
    m_format.setSampleRate(sampleRate);

    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
    }

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    m_audioSink = new QAudioSink(device, m_format, this);
    m_audioSink->setBufferSize(sampleRate * 2 * sizeof(float) / 10);
    m_audioOutput = m_audioSink->start();
}

AudioMixer::~AudioMixer() {
    if (m_audioSink)
        m_audioSink->stop();
}

void AudioMixer::registerDecoder(int clipId, Rina::Core::AudioDecoder *decoder) { m_decoders[clipId] = decoder; }

void AudioMixer::unregisterDecoder(int clipId) { m_decoders.erase(clipId); }

bool AudioMixer::isReady() const {
    for (const auto &[id, decoder] : m_decoders) {
        if (!decoder || !decoder->isReady())
            return false;
    }
    return true;
}

std::vector<float> AudioMixer::mix(int currentFrame, double fps, int samplesPerFrame) {
    // 1. マスターバッファの初期化（無音）
    std::vector<float> masterBuffer(samplesPerFrame * 2, 0.0f);

    // 2. ECSから現在の音声コンポーネントを取得
    auto state = Timeline::ECS::instance().getSnapshot();
    if (!state)
        return masterBuffer;
    const auto &audioStates = state->audioStates;
    for (const auto &[clipId, audio] : audioStates) {
        if (audio.mute)
            continue;
        if (m_decoders.find(clipId) == m_decoders.end())
            continue;

        if (currentFrame < audio.startFrame || currentFrame >= audio.startFrame + audio.durationFrames)
            continue;

        // 位相と連続再生の管理
        double startTime = (double)(currentFrame - audio.startFrame) / fps;
        if (m_clipLastFrame.contains(clipId) && currentFrame == m_clipLastFrame[clipId] + 1) {
            startTime = m_clipPhase[clipId];
        } else {
            // シークまたは初回再生時
            m_clipPhase[clipId] = startTime;
        }
        m_clipLastFrame[clipId] = currentFrame;

        auto decoder = m_decoders[clipId];
        std::vector<float> clipSamples;

        if (std::abs(m_playbackSpeed - 1.0) > 0.01) {
            // リサンプリングが必要な場合
            // 必要ソースサンプル数を計算（補間用に2サンプル余分に要求）
            int neededSamples = static_cast<int>(std::ceil(samplesPerFrame * m_playbackSpeed)) + 2;
            std::vector<float> rawSamples = decoder->getSamples(startTime, neededSamples * 2); // Stereo

            if (!rawSamples.empty()) {
                clipSamples.resize(samplesPerFrame * 2);
                int availableSrcSamples = static_cast<int>(rawSamples.size() / 2);

                for (int i = 0; i < samplesPerFrame; ++i) {
                    double srcIdx = i * m_playbackSpeed;
                    int idx0 = static_cast<int>(srcIdx);
                    int idx1 = idx0 + 1;

                    // クランプして範囲外アクセス（SIGSEGV）を防止
                    if (idx0 >= availableSrcSamples)
                        idx0 = availableSrcSamples - 1;
                    if (idx1 >= availableSrcSamples)
                        idx1 = availableSrcSamples - 1;
                    if (idx0 < 0)
                        idx0 = 0;
                    if (idx1 < 0)
                        idx1 = 0;

                    double t = srcIdx - idx0;

                    // L ch
                    clipSamples[i * 2] = rawSamples[idx0 * 2] * (1.0 - t) + rawSamples[idx1 * 2] * t;
                    // R ch
                    clipSamples[i * 2 + 1] = rawSamples[idx0 * 2 + 1] * (1.0 - t) + rawSamples[idx1 * 2 + 1] * t;
                }
            }
            // 次のフレームのための開始位置を進める（m_playbackSpeed 分の秒数）
            m_clipPhase[clipId] = startTime + (static_cast<double>(samplesPerFrame) / m_format.sampleRate()) * m_playbackSpeed;
        } else {
            // 1倍速の場合はそのまま取得
            int neededSamples = samplesPerFrame;
            clipSamples = decoder->getSamples(startTime, neededSamples * 2);
            m_clipPhase[clipId] = startTime + (static_cast<double>(samplesPerFrame) / m_format.sampleRate());
        }

        // プラグインチェーンを適用
        if (m_chains.contains(clipId))
            m_chains[clipId]->process(clipSamples.data(), samplesPerFrame);

        float leftVol = audio.volume * (audio.pan <= 0 ? 1.0f : 1.0f - audio.pan);
        float rightVol = audio.volume * (audio.pan >= 0 ? 1.0f : 1.0f + audio.pan);

        for (size_t i = 0; i < clipSamples.size() && i < masterBuffer.size(); i += 2) {
            masterBuffer[i] += clipSamples[i] * leftVol;
            if (i + 1 < clipSamples.size())
                masterBuffer[i + 1] += clipSamples[i + 1] * rightVol;
        }
    }
    return masterBuffer;
}

void AudioMixer::processFrame(int currentFrame, double fps, int samplesPerFrame) {
    if (!m_audioOutput)
        return;

    // 巻き戻し（ループ）検知: 前回のフレームより戻っていたらバッファをリセット
    if (m_lastFrame != -1 && currentFrame < m_lastFrame) {
        reset();
        if (!m_audioOutput)
            return;
    }
    m_lastFrame = currentFrame;

    // 再生速度が変更されている場合、実時間に対する出力バッファ量を補正する。
    // トランスポートのフレーム更新頻度が m_playbackSpeed 倍になっているため、
    // 1回の呼び出しで書き込むべきデータ量は 1/m_playbackSpeed 倍にする必要がある。
    int outputSamples = samplesPerFrame;
    if (m_playbackSpeed > 0.0) {
        outputSamples = static_cast<int>(samplesPerFrame / m_playbackSpeed);
    }

    std::vector<float> buffer = mix(currentFrame, fps, outputSamples);
    m_audioOutput->write(reinterpret_cast<const char *>(buffer.data()), buffer.size() * sizeof(float));
}

void AudioMixer::reset() {
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->reset();
        m_audioOutput = m_audioSink->start();
    }
    m_clipPhase.clear();
    m_clipLastFrame.clear();
}

Plugin::AudioPluginChain &AudioMixer::getChain(int clipId) {
    // m_chains[clipId] will default-construct an AudioPluginChain if it doesn't exist.
    if (!m_chains.contains(clipId)) {
        m_chains[clipId] = std::make_shared<Plugin::AudioPluginChain>();
    }
    return *m_chains[clipId];
}

void AudioMixer::clearChain(int clipId) { m_chains.remove(clipId); }

} // namespace Rina::Engine