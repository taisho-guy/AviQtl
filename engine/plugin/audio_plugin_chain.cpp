#include "audio_plugin_chain.hpp"

#include <utility>

namespace AviQtl::Engine::Plugin {

void AudioPluginChain::add(std::unique_ptr<IAudioPlugin> plugin) {
    plugin->prepare(m_sampleRate, m_maxBlockSize);
    m_plugins.push_back(std::move(plugin));
}

void AudioPluginChain::remove(int index) {
    if (index >= 0 && std::cmp_less(index, m_plugins.size())) {
        m_plugins.erase(m_plugins.begin() + index);
    }
}

void AudioPluginChain::clear() { m_plugins.clear(); }

void AudioPluginChain::prepare(double sr, int bs) {
    m_sampleRate = sr;
    m_maxBlockSize = bs;
    for (const auto &p : std::as_const(m_plugins)) {
        p->prepare(sr, bs);
    }
}

void AudioPluginChain::process(float *buf, int frameCount) {
    for (const auto &p : std::as_const(m_plugins)) {
        p->process(buf, frameCount);
    }
}

auto AudioPluginChain::count() const -> int { return static_cast<int>(m_plugins.size()); }

auto AudioPluginChain::get(int index) const -> IAudioPlugin * { return (index >= 0 && std::cmp_less(index, m_plugins.size())) ? m_plugins[index].get() : nullptr; }

} // namespace AviQtl::Engine::Plugin