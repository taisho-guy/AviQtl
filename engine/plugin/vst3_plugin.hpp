#pragma once
#include "audio_plugin_host.hpp"

namespace Rina::Engine::Plugin {

class Vst3Plugin : public IAudioPlugin {
  public:
    bool load(const QString &path, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;
    QString name() const override { return m_name; }
    QString format() const override { return "VST3"; }
    int paramCount() const override { return 0; }
    QString paramName(int) const override { return {}; }
    float getParam(int) const override { return 0.0f; }
    void setParam(int, float) override {}

  private:
    QString m_name;
};

} // namespace Rina::Engine::Plugin