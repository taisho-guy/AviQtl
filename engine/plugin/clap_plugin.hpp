#pragma once
#include "audio_plugin_host.hpp"
#include <QLibrary>
#include <clap/clap.h>
#include <vector>

namespace Rina::Engine::Plugin {

class ClapPlugin : public IAudioPlugin {
  public:
    ~ClapPlugin() override { release(); }

    bool load(const QString &path, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;

    QString name() const override { return m_name; }
    QString format() const override { return "CLAP"; }
    int paramCount() const override;
    QString paramName(int i) const override;
    float getParam(int i) const override;
    void setParam(int i, float v) override;

  private:
    // CLAPホストコールバック (最小実装)
    static const clap_host_t s_host;

    QLibrary m_lib;
    const clap_plugin_t *m_plugin = nullptr;
    clap_audio_buffer_t m_inBuf{}, m_outBuf{};
    std::vector<float> m_inL, m_inR, m_outL, m_outR;
    float *m_inPtrs[2] = {};
    float *m_outPtrs[2] = {};

    QString m_name;
    int m_blockSize = 0;
    double m_sampleRate = 48000.0;

    const clap_plugin_params_t *m_paramsExt = nullptr;
    clap_input_events_t m_emptyIn{};
    clap_output_events_t m_emptyOut{};
};

} // namespace Rina::Engine::Plugin