#include "ladspa_plugin.hpp"
#include <QDebug>
#include <algorithm> // for std::copy
#include <cmath>

namespace Rina::Engine::Plugin {

bool LadspaPlugin::load(const QString &path, int index) {
    m_lib.setFileName(path);
    if (!m_lib.load()) {
        qWarning() << "[LADSPA] ロード失敗:" << m_lib.errorString();
        return false;
    }

    auto fn = (LADSPA_Descriptor_Function)m_lib.resolve("ladspa_descriptor");
    if (!fn) {
        qWarning() << "[LADSPA] ladspa_descriptor が見つかりません";
        return false;
    }

    m_desc = fn(index);
    if (!m_desc) {
        qWarning() << "[LADSPA] インデックス" << index << "のプラグインが存在しません";
        return false;
    }

    // ポートの分類
    m_audioInPorts.clear();
    m_audioOutPorts.clear();
    m_ctrlPorts.clear();
    m_ctrlValues.clear();
    m_paramInfos.clear();

    for (unsigned long i = 0; i < m_desc->PortCount; ++i) {
        LADSPA_PortDescriptor pd = m_desc->PortDescriptors[i];
        LADSPA_PortRangeHint hint = m_desc->PortRangeHints[i];

        if (LADSPA_IS_PORT_AUDIO(pd)) {
            if (LADSPA_IS_PORT_INPUT(pd))
                m_audioInPorts.push_back((int)i);
            if (LADSPA_IS_PORT_OUTPUT(pd))
                m_audioOutPorts.push_back((int)i);
        } else if (LADSPA_IS_PORT_CONTROL(pd) && LADSPA_IS_PORT_INPUT(pd)) {
            m_ctrlPorts.push_back((int)i);

            // パラメータ情報の解析
            ParamInfo info;
            info.name = QString(m_desc->PortNames[i]);
            info.min = LADSPA_IS_HINT_BOUNDED_BELOW(hint.HintDescriptor) ? hint.LowerBound : 0.0f;
            info.max = LADSPA_IS_HINT_BOUNDED_ABOVE(hint.HintDescriptor) ? hint.UpperBound : 1.0f;
            info.isLog = LADSPA_IS_HINT_LOGARITHMIC(hint.HintDescriptor);
            info.isInteger = LADSPA_IS_HINT_INTEGER(hint.HintDescriptor);
            info.isToggle = LADSPA_IS_HINT_TOGGLED(hint.HintDescriptor);

            // デフォルト値の計算
            float def = info.min;
            if (LADSPA_IS_HINT_HAS_DEFAULT(hint.HintDescriptor)) {
                if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hint.HintDescriptor))
                    def = info.min;
                else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hint.HintDescriptor))
                    def = info.max;
                else if (LADSPA_IS_HINT_DEFAULT_LOW(hint.HintDescriptor))
                    def = info.isLog ? exp(log(info.min) * 0.75 + log(info.max) * 0.25) : info.min + (info.max - info.min) * 0.25;
                else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hint.HintDescriptor))
                    def = info.isLog ? exp(log(info.min) * 0.5 + log(info.max) * 0.5) : info.min + (info.max - info.min) * 0.5;
                else if (LADSPA_IS_HINT_DEFAULT_HIGH(hint.HintDescriptor))
                    def = info.isLog ? exp(log(info.min) * 0.25 + log(info.max) * 0.75) : info.min + (info.max - info.min) * 0.75;
                else if (LADSPA_IS_HINT_DEFAULT_0(hint.HintDescriptor))
                    def = 0.0f;
                else if (LADSPA_IS_HINT_DEFAULT_1(hint.HintDescriptor))
                    def = 1.0f;
                else if (LADSPA_IS_HINT_DEFAULT_100(hint.HintDescriptor))
                    def = 100.0f;
                else if (LADSPA_IS_HINT_DEFAULT_440(hint.HintDescriptor))
                    def = 440.0f;
            }
            info.defaultValue = def;
            m_paramInfos.push_back(info);
            m_ctrlValues.push_back(def);
        }
    }

    qInfo() << "[LADSPA] ロード成功:" << m_desc->Name;
    return true;
}

void LadspaPlugin::prepare(double sampleRate, int maxBlockSize) {
    if (!m_desc)
        return;
    for (auto h : m_handles) {
        m_desc->cleanup(h);
    }
    m_handles.clear();

    m_blockSize = maxBlockSize;
    m_inBufL.assign(maxBlockSize, 0.0f);
    m_inBufR.assign(maxBlockSize, 0.0f);
    m_outBufL.assign(maxBlockSize, 0.0f);
    m_outBufR.assign(maxBlockSize, 0.0f);

    // Dual Mono 判定: 入出力が1つ以下の場合はL/R独立処理のために2つ生成
    bool isDualMono = (m_audioInPorts.size() <= 1 && m_audioOutPorts.size() == 1);
    int instanceCount = isDualMono ? 2 : 1;

    for (int i = 0; i < instanceCount; ++i) {
        LADSPA_Handle h = m_desc->instantiate(m_desc, (unsigned long)sampleRate);
        if (!h)
            continue;

        // コントロールポートの接続 (全インスタンスで共有)
        for (size_t k = 0; k < m_ctrlPorts.size(); ++k) {
            m_desc->connect_port(h, m_ctrlPorts[k], &m_ctrlValues[k]);
        }
        m_handles.push_back(h);
        if (m_desc->activate)
            m_desc->activate(h);
    }
}

void LadspaPlugin::process(float *buf, int frameCount) {
    if (m_handles.empty())
        return;

    // De-interleave
    for (int i = 0; i < frameCount; ++i) {
        m_inBufL[i] = buf[i * 2];
        m_inBufR[i] = buf[i * 2 + 1];
    }

    if (m_handles.size() == 2) {
        // --- Dual Mono ---
        // Left Instance
        if (!m_audioInPorts.empty())
            m_desc->connect_port(m_handles[0], m_audioInPorts[0], m_inBufL.data());
        if (!m_audioOutPorts.empty())
            m_desc->connect_port(m_handles[0], m_audioOutPorts[0], m_outBufL.data());
        m_desc->run(m_handles[0], frameCount);

        // Right Instance
        if (!m_audioInPorts.empty())
            m_desc->connect_port(m_handles[1], m_audioInPorts[0], m_inBufR.data());
        if (!m_audioOutPorts.empty())
            m_desc->connect_port(m_handles[1], m_audioOutPorts[0], m_outBufR.data());
        m_desc->run(m_handles[1], frameCount);
    } else {
        // --- Stereo / Multi-channel ---
        // Map In[0]->L, In[1]->R
        if (m_audioInPorts.size() > 0)
            m_desc->connect_port(m_handles[0], m_audioInPorts[0], m_inBufL.data());
        if (m_audioInPorts.size() > 1)
            m_desc->connect_port(m_handles[0], m_audioInPorts[1], m_inBufR.data());

        // Map Out[0]->L, Out[1]->R
        if (m_audioOutPorts.size() > 0)
            m_desc->connect_port(m_handles[0], m_audioOutPorts[0], m_outBufL.data());
        if (m_audioOutPorts.size() > 1)
            m_desc->connect_port(m_handles[0], m_audioOutPorts[1], m_outBufR.data());

        m_desc->run(m_handles[0], frameCount);

        // モノラル出力の場合はLをRにコピー
        if (m_audioOutPorts.size() == 1) {
            std::copy(m_outBufL.begin(), m_outBufL.begin() + frameCount, m_outBufR.begin());
        }
    }

    // Interleave
    for (int i = 0; i < frameCount; ++i) {
        buf[i * 2] = m_outBufL[i];
        buf[i * 2 + 1] = m_outBufR[i];
    }
}

void LadspaPlugin::release() {
    if (!m_desc)
        return;
    for (auto h : m_handles) {
        if (m_desc->deactivate)
            m_desc->deactivate(h);
        m_desc->cleanup(h);
    }
    m_handles.clear();
    if (m_lib.isLoaded())
        m_lib.unload();
}

ParamInfo LadspaPlugin::getParamInfo(int i) const {
    if (i >= 0 && i < (int)m_paramInfos.size())
        return m_paramInfos[i];
    return {};
}

QString LadspaPlugin::name() const { return m_desc ? QString(m_desc->Name) : ""; }

int LadspaPlugin::paramCount() const { return (int)m_ctrlPorts.size(); }

QString LadspaPlugin::paramName(int i) const {
    if (!m_desc || i < 0 || i >= (int)m_ctrlPorts.size())
        return {};
    return QString(m_desc->PortNames[m_ctrlPorts[i]]);
}

float LadspaPlugin::getParam(int i) const {
    if (i >= 0 && i < (int)m_ctrlValues.size())
        return m_ctrlValues[i];
    return 0.0f;
}

void LadspaPlugin::setParam(int i, float v) {
    if (i >= 0 && i < (int)m_ctrlValues.size())
        m_ctrlValues[i] = v;
}

} // namespace Rina::Engine::Plugin