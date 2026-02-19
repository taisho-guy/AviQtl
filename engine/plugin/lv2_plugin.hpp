#pragma once
#include "audio_plugin_host.hpp"
#include <lilv/lilv.h>
#include <vector>

namespace Rina::Engine::Plugin {

class Lv2Plugin : public IAudioPlugin {
  public:
    ~Lv2Plugin() override { release(); }

    bool load(const QString &uri, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;

    QString name() const override;
    QString format() const override { return "LV2"; }
    int paramCount() const override;
    QString paramName(int i) const override;
    float getParam(int i) const override;
    void setParam(int i, float v) override;

  private:
    LilvWorld *m_world = nullptr;
    LilvInstance *m_instance = nullptr;
    const LilvPlugin *m_plugin = nullptr;

    int m_audioInPort = -1;
    int m_audioOutPort = -1;
    std::vector<int> m_ctrlPorts;
    std::vector<float> m_ctrlValues;
    std::vector<float> m_inBuf, m_outBuf;
};

} // namespace Rina::Engine::Plugin