#include "audio_mixer.hpp"
#include "core/include/audio_decoder.hpp"
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
    m_format.setSampleRate(48000);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Float);

    // 4.1 すべてのclipIdに対してPluginChainを初期化
    const auto &audioStates = Timeline::ECS::instance().getAudioComponents();
    for (const auto &[clipId, audio] : audioStates) {
        if (!m_chains.contains(clipId)) {
            m_chains[clipId] = std::make_shared<Plugin::AudioPluginChain>();
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
    m_audioSink->setBufferSize(48000 * 2 * sizeof(float) / 10);
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

AudioMixer::~AudioMixer() {
    if (m_audioSink)
        m_audioSink->stop();
}

void AudioMixer::registerDecoder(int clipId, Rina::Core::AudioDecoder *decoder) { m_decoders[clipId] = decoder; }

void AudioMixer::unregisterDecoder(int clipId) { m_decoders.erase(clipId); }

std::vector<float> AudioMixer::mix(int currentFrame, double fps, int samplesPerFrame) {
    // 1. マスターバッファの初期化（無音）
    std::vector<float> masterBuffer(samplesPerFrame * 2, 0.0f);

    // 2. ECSから現在の音声コンポーネントを取得
    const auto &audioStates = Timeline::ECS::instance().getAudioComponents();

    for (const auto &[clipId, audio] : audioStates) {
        if (audio->mute)
            continue;
        if (m_decoders.find(clipId) == m_decoders.end())
            continue;

        if (currentFrame < audio->startFrame || currentFrame >= audio->startFrame + audio->durationFrames)
            continue;

        double startTime = (double)(currentFrame - audio->startFrame) / fps;
        auto decoder = m_decoders[clipId];
        std::vector<float> clipSamples = decoder->getSamples(startTime, samplesPerFrame * 2);

        // プラグインチェーンを適用
        if (m_chains.contains(clipId))
            m_chains[clipId]->process(clipSamples.data(), samplesPerFrame);

        float leftVol = audio->volume * (audio->pan <= 0 ? 1.0f : 1.0f - audio->pan);
        float rightVol = audio->volume * (audio->pan >= 0 ? 1.0f : 1.0f + audio->pan);

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
    }
    m_lastFrame = currentFrame;

    std::vector<float> buffer = mix(currentFrame, fps, samplesPerFrame);
    m_audioOutput->write(reinterpret_cast<const char *>(buffer.data()), buffer.size() * sizeof(float));
}

void AudioMixer::reset() {
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->reset();
        m_audioOutput = m_audioSink->start();
    }
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