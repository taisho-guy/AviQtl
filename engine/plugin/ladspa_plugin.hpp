#pragma once
#include "audio_plugin_host.hpp"
#include <QLibrary>
#include <ladspa.h>
#include <vector>

namespace Rina::Engine::Plugin {

// LADSPA記述子取得関数の型定義
typedef const LADSPA_Descriptor *(*LADSPA_Descriptor_Function)(unsigned long Index);

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
    ParamInfo getParamInfo(int i) const override;

  private:
    QLibrary m_lib;
    const LADSPA_Descriptor *m_desc = nullptr;
    std::vector<LADSPA_Handle> m_handles;

    // ポートインデックスの分類結果
    std::vector<int> m_audioInPorts;
    std::vector<int> m_audioOutPorts;
    std::vector<int> m_ctrlPorts;
    std::vector<float> m_ctrlValues;
    std::vector<ParamInfo> m_paramInfos;

    // process 用の作業バッファ
    std::vector<float> m_inBufL, m_inBufR;
    std::vector<float> m_outBufL, m_outBufR;
    int m_blockSize = 0;
};

} // namespace Rina::Engine::Plugin