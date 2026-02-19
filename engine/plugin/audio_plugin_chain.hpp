#pragma once
#include "audio_plugin_host.hpp"
#include <memory>
#include <vector>

namespace Rina::Engine::Plugin {

class AudioPluginChain {
  public:
    void add(std::unique_ptr<IAudioPlugin> plugin);
    void remove(int index);
    void clear();

    void prepare(double sampleRate, int maxBlockSize);
    // mix() から呼ばれる：バッファをチェーン内の全プラグインに通す
    void process(float *buf, int frameCount);

    int count() const;
    IAudioPlugin *get(int index) const;

  private:
    std::vector<std::unique_ptr<IAudioPlugin>> m_plugins;
    double m_sampleRate = 48000.0;
    int m_maxBlockSize = 4096;
};

} // namespace Rina::Engine::Plugin
