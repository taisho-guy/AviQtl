#include "vst3_plugin.hpp"
#include <QDebug>

namespace Rina::Engine::Plugin {
// VST3はvst3sdk の IPluginFactory / IComponent / IAudioProcessor
// の COM チェーンが必要。Phase 2 で実装予定
bool Vst3Plugin::load(const QString &p, int) {
    qWarning() << "[VST3] 未実装:" << p;
    return false;
}
void Vst3Plugin::prepare(double, int) {}
void Vst3Plugin::process(float *, int) {}
void Vst3Plugin::release() {}
} // namespace Rina::Engine::Plugin