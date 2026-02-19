#pragma once
#include "plugin/audio_plugin_chain.hpp"
#include <QAudioSink>
#include <QIODevice>
#include <QObject>
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

    void processFrame(int currentFrame, double fps, int samplesPerFrame);
    void reset();

    // エクスポート用に生データを取得するメソッド
    std::vector<float> mix(int currentFrame, double fps, int samplesPerFrame);

    // クリップID → プラグインチェーン
    Plugin::AudioPluginChain &getChain(int clipId);
    void clearChain(int clipId);

  private:
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioOutput = nullptr;
    std::unordered_map<int, Rina::Core::AudioDecoder *> m_decoders;
    std::unordered_map<int, Plugin::AudioPluginChain> m_chains;
    int m_lastFrame = -1;
};

} // namespace Rina::Engine