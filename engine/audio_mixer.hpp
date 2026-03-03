#pragma once
#include "plugin/audio_plugin_chain.hpp"
#include <QAudioFormat>
#include <QAudioSink>
#include <QHash>
#include <QIODevice>
#include <QObject>
#include <memory>
#include <unordered_map>

namespace Rina::Core {
class AudioDecoder;
}

namespace Rina::Engine {

class AudioMixer : public QObject {
    Q_OBJECT
  public:
    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer();

    void registerDecoder(int clipId, Rina::Core::AudioDecoder *decoder);
    void unregisterDecoder(int clipId);

    // 全てのデコーダーが読み込み完了しているか確認
    bool isReady() const;

    void processFrame(int currentFrame, double fps, int samplesPerFrame);
    void reset();

    void processChain(float *buffer, int samples, const Plugin::AudioPluginChain &chain);
    void mix(float *output, const float *input, float volume, int samples);

    // エクスポート用に生データを取得するメソッド
    std::vector<float> mix(int currentFrame, double fps, int samplesPerFrame);

    // クリップID → プラグインチェーン
    Plugin::AudioPluginChain &getChain(int clipId);
    void clearChain(int clipId);

    void setPlaybackSpeed(double speed) { m_playbackSpeed = speed; }
    void setSampleRate(int sampleRate);

  private:
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioOutput = nullptr;
    QAudioFormat m_format;
    std::unordered_map<int, Rina::Core::AudioDecoder *> m_decoders;
    QHash<int, std::shared_ptr<Plugin::AudioPluginChain>> m_chains;
    int m_lastFrame = -1;
    double m_playbackSpeed = 1.0;
    QHash<int, double> m_clipPhase;
    QHash<int, int> m_clipLastFrame;
};

} // namespace Rina::Engine