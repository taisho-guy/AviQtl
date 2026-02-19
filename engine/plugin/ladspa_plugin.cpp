#include "ladspa_plugin.hpp"
#include <QDebug>

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
    for (unsigned long i = 0; i < m_desc->PortCount; ++i) {
        LADSPA_PortDescriptor pd = m_desc->PortDescriptors[i];
        if (LADSPA_IS_PORT_AUDIO(pd)) {
            if (LADSPA_IS_PORT_INPUT(pd) && m_audioInPort < 0)
                m_audioInPort = (int)i;
            if (LADSPA_IS_PORT_OUTPUT(pd) && m_audioOutPort < 0)
                m_audioOutPort = (int)i;
        } else if (LADSPA_IS_PORT_CONTROL(pd)) {
            m_ctrlPorts.push_back((int)i);
            // デフォルト値は 0 に初期化（HintDescriptor から算出可能だが省略）
            m_ctrlValues.push_back(0.0f);
        }
    }

    qInfo() << "[LADSPA] ロード成功:" << m_desc->Name;
    return true;
}

void LadspaPlugin::prepare(double sampleRate, int maxBlockSize) {
    if (!m_desc)
        return;
    if (m_handle) {
        m_desc->cleanup(m_handle);
        m_handle = nullptr;
    }

    m_blockSize = maxBlockSize;
    m_inBuf.assign(maxBlockSize, 0.0f);
    m_outBuf.assign(maxBlockSize, 0.0f);

    m_handle = m_desc->instantiate(m_desc, (unsigned long)sampleRate);
    if (!m_handle) {
        qWarning() << "[LADSPA] instantiate 失敗";
        return;
    }

    // オーディオポートを接続
    if (m_audioInPort >= 0)
        m_desc->connect_port(m_handle, m_audioInPort, m_inBuf.data());
    if (m_audioOutPort >= 0)
        m_desc->connect_port(m_handle, m_audioOutPort, m_outBuf.data());

    // コントロールポートを接続
    for (int k = 0; k < (int)m_ctrlPorts.size(); ++k)
        m_desc->connect_port(m_handle, m_ctrlPorts[k], &m_ctrlValues[k]);

    if (m_desc->activate)
        m_desc->activate(m_handle);
}

void LadspaPlugin::process(float *buf, int frameCount) {
    if (!m_handle || m_audioInPort < 0 || m_audioOutPort < 0)
        return;

    // インターリーブ → モノ L ch を抽出
    for (int i = 0; i < frameCount; ++i)
        m_inBuf[i] = buf[i * 2];
    m_desc->run(m_handle, (unsigned long)frameCount);
    // モノ出力を L/R 両 ch に書き戻す
    for (int i = 0; i < frameCount; ++i)
        buf[i * 2] = buf[i * 2 + 1] = m_outBuf[i];
}

void LadspaPlugin::release() {
    if (!m_desc || !m_handle)
        return;
    if (m_desc->deactivate)
        m_desc->deactivate(m_handle);
    m_desc->cleanup(m_handle);
    m_handle = nullptr;
    if (m_lib.isLoaded())
        m_lib.unload();
}

QString LadspaPlugin::name() const { return m_desc ? QString(m_desc->Name) : ""; }
int LadspaPlugin::paramCount() const { return (int)m_ctrlPorts.size(); }
QString LadspaPlugin::paramName(int i) const {
    if (!m_desc || i < 0 || i >= (int)m_ctrlPorts.size())
        return {};
    return QString(m_desc->PortNames[m_ctrlPorts[i]]);
}
float LadspaPlugin::getParam(int i) const { return (i >= 0 && i < (int)m_ctrlValues.size()) ? m_ctrlValues[i] : 0.0f; }
void LadspaPlugin::setParam(int i, float v) {
    if (i >= 0 && i < (int)m_ctrlValues.size())
        m_ctrlValues[i] = v;
}

} // namespace Rina::Engine::Plugin