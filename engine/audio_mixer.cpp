#include "audio_mixer.hpp"
#include "core/include/audio_decoder.hpp"
#include "engine/timeline/ecs.hpp"
#include <QAudioFormat>
#include <QMediaDevices>
#include <QDebug>
#include <vector>

namespace Rina::Engine {

AudioMixer::AudioMixer(QObject *parent) : QObject(parent) {
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format)) {
        qWarning() << "Default audio format not supported, using preferred format.";
        format = device.preferredFormat();
    }

    m_audioSink = new QAudioSink(device, format, this);
    // 低レイテンシを目指しつつ、音飛びしない程度のバッファサイズ (例: 100ms)
    m_audioSink->setBufferSize(48000 * 2 * sizeof(float) / 10);
    m_audioOutput = m_audioSink->start();
}

AudioMixer::~AudioMixer() {
    if (m_audioSink)
        m_audioSink->stop();
}

void AudioMixer::registerDecoder(int clipId, Rina::Core::AudioDecoder *decoder) {
    m_decoders[clipId] = decoder;
}

void AudioMixer::unregisterDecoder(int clipId) {
    m_decoders.erase(clipId);
}

std::vector<float> AudioMixer::mix(int currentFrame, double fps, int samplesPerFrame) {
    // 1. マスターバッファの初期化（無音）
    std::vector<float> masterBuffer(samplesPerFrame * 2, 0.0f);

    // 2. ECSから現在の音声コンポーネントを取得
    const auto &audioStates = Timeline::ECS::instance().getAudioComponents();

    for (const auto &[clipId, audio] : audioStates) {
        if (audio->mute) continue;
        if (m_decoders.find(clipId) == m_decoders.end()) continue;

        if (currentFrame < audio->startFrame || currentFrame >= audio->startFrame + audio->durationFrames)
            continue;

        double startTime = (double)(currentFrame - audio->startFrame) / fps;
        auto decoder = m_decoders[clipId];
        std::vector<float> clipSamples = decoder->getSamples(startTime, samplesPerFrame * 2);

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

    std::vector<float> buffer = mix(currentFrame, fps, samplesPerFrame);
    m_audioOutput->write(reinterpret_cast<const char *>(buffer.data()), buffer.size() * sizeof(float));
}

} // namespace Rina::Engine