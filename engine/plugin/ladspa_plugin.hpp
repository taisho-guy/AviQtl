#pragma once
#include "audio_plugin_host.hpp"
#include <QLibrary>
#include <ladspa.h>
#include <vector>

namespace Rina::Engine::Plugin {

class LadspaPlugin : public IAudioPlugin {
  public:
    ~LadspaPlugin() override { release(); }

    bool load(const QString &path, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;

    QString name() const override;
    QString format() const override { return "LADSPA"; }
    int paramCount() const override;
    QString paramName(int i) const override;
    float getParam(int i) const override;
    void setParam(int i, float v) override;

  private:
    QLibrary m_lib;
    const LADSPA_Descriptor *m_desc = nullptr;
    LADSPA_Handle m_handle = nullptr;

    // ポートインデックスの分類結果
    int m_audioInPort = -1;
    int m_audioOutPort = -1;
    std::vector<int> m_ctrlPorts;
    std::vector<float> m_ctrlValues;

    // process 用の作業バッファ
    std::vector<float> m_inBuf;
    std::vector<float> m_outBuf;
    int m_blockSize = 0;
};

} // namespace Rina::Engine::Plugin