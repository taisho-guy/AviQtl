#include "audio_plugin_chain.hpp"

namespace Rina::Engine::Plugin {

void AudioPluginChain::add(std::unique_ptr<IAudioPlugin> plugin) {
    plugin->prepare(m_sampleRate, m_maxBlockSize);
    m_plugins.push_back(std::move(plugin));
}

void AudioPluginChain::remove(int index) {
    if (index >= 0 && index < (int)m_plugins.size())
        m_plugins.erase(m_plugins.begin() + index);
}

void AudioPluginChain::clear() { m_plugins.clear(); }

void AudioPluginChain::prepare(double sr, int bs) {
    m_sampleRate = sr;
    m_maxBlockSize = bs;
    for (auto &p : m_plugins)
        p->prepare(sr, bs);
}

void AudioPluginChain::process(float *buf, int frameCount) {
    for (auto &p : m_plugins)
        p->process(buf, frameCount);
}

int AudioPluginChain::count() const { return (int)m_plugins.size(); }

IAudioPlugin *AudioPluginChain::get(int index) const { return (index >= 0 && index < (int)m_plugins.size()) ? m_plugins[index].get() : nullptr; }

} // namespace Rina::Engine::Plugin