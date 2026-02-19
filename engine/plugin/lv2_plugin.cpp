#include "lv2_plugin.hpp"
#include <QDebug>
#include <lilv/lilv.h>
#include <lv2/options/options.h>

namespace Rina::Engine::Plugin {

bool Lv2Plugin::load(const QString &uri, int) {
    m_world = lilv_world_new();
    lilv_world_load_all(m_world); // /usr/lib/lv2 を自動スキャン

    LilvNode *uriNode = lilv_new_uri(m_world, uri.toUtf8().constData());
    const LilvPlugins *all = lilv_world_get_all_plugins(m_world);
    m_plugin = lilv_plugins_get_by_uri(all, uriNode);
    lilv_node_free(uriNode);

    if (!m_plugin) {
        qWarning() << "[LV2] プラグインが見つかりません:" << uri;
        return false;
    }

    // ポート分類
    LilvNode *audioClass = lilv_new_uri(m_world, LILV_URI_AUDIO_PORT);
    LilvNode *inputClass = lilv_new_uri(m_world, LILV_URI_INPUT_PORT);
    LilvNode *outputClass= lilv_new_uri(m_world, LILV_URI_OUTPUT_PORT);
    LilvNode *ctrlClass  = lilv_new_uri(m_world, LILV_URI_CONTROL_PORT);

    uint32_t numPorts = lilv_plugin_get_num_ports(m_plugin);
    m_ctrlValues.clear();
    for (uint32_t i = 0; i < numPorts; ++i) {
        const LilvPort *port = lilv_plugin_get_port_by_index(m_plugin, i);
        if (lilv_port_is_a(m_plugin, port, audioClass)) {
            if (lilv_port_is_a(m_plugin, port, inputClass)  && m_audioInPort  < 0) m_audioInPort  = (int)i;
            if (lilv_port_is_a(m_plugin, port, outputClass) && m_audioOutPort < 0) m_audioOutPort = (int)i;
        } else if (lilv_port_is_a(m_plugin, port, ctrlClass)) {
            m_ctrlPorts.push_back((int)i);
            m_ctrlValues.push_back(0.0f);
        }
    }
    lilv_node_free(audioClass);
    lilv_node_free(inputClass);
    lilv_node_free(outputClass);
    lilv_node_free(ctrlClass);

    LilvNode *n = lilv_plugin_get_name(m_plugin);
    qInfo() << "[LV2] ロード成功:" << lilv_node_as_string(n);
    lilv_node_free(n);
    return true;
}

void Lv2Plugin::prepare(double sampleRate, int maxBlockSize) {
    if (!m_plugin) return;
    if (m_instance) { lilv_instance_deactivate(m_instance); lilv_instance_free(m_instance); }

    m_inBuf.assign(maxBlockSize, 0.0f);
    m_outBuf.assign(maxBlockSize, 0.0f);

    // Options Feature の準備
    // 一部のプラグイン（ZynAddSubFXなど）はこれを必須とする
    const LV2_Options_Option options[] = {
        {LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL} // 終端
    };
    const LV2_Feature options_feature = {LV2_OPTIONS__options, (void *)options};
    const LV2_Feature *features[] = {&options_feature, nullptr};

    m_instance = lilv_plugin_instantiate(m_plugin, sampleRate, features);

    if (!m_instance) { qWarning() << "[LV2] instantiate 失敗"; return; }

    lilv_instance_connect_port(m_instance, m_audioInPort,  m_inBuf.data());
    lilv_instance_connect_port(m_instance, m_audioOutPort, m_outBuf.data());
    for (int k = 0; k < (int)m_ctrlPorts.size(); ++k)
        lilv_instance_connect_port(m_instance, m_ctrlPorts[k], &m_ctrlValues[k]);

    lilv_instance_activate(m_instance);
}

void Lv2Plugin::process(float *buf, int frameCount) {
    if (!m_instance || m_audioInPort < 0 || m_audioOutPort < 0) return;
    for (int i = 0; i < frameCount; ++i) m_inBuf[i] = buf[i * 2];
    lilv_instance_run(m_instance, (uint32_t)frameCount);
    for (int i = 0; i < frameCount; ++i) buf[i * 2] = buf[i * 2 + 1] = m_outBuf[i];
}

void Lv2Plugin::release() {
    if (m_instance) {
        lilv_instance_deactivate(m_instance);
        lilv_instance_free(m_instance);
        m_instance = nullptr;
    }
    if (m_world) { lilv_world_free(m_world); m_world = nullptr; }
}

QString Lv2Plugin::name() const {
    if (!m_plugin) return {};
    LilvNode *n = lilv_plugin_get_name(m_plugin);
    QString s = lilv_node_as_string(n);
    lilv_node_free(n);
    return s;
}
int     Lv2Plugin::paramCount() const { return (int)m_ctrlPorts.size(); }
QString Lv2Plugin::paramName(int i) const {
    if (!m_plugin || i < 0 || i >= (int)m_ctrlPorts.size()) return {};
    const LilvPort *p = lilv_plugin_get_port_by_index(m_plugin, m_ctrlPorts[i]);
    LilvNode *n = lilv_port_get_name(m_plugin, p);
    QString s = lilv_node_as_string(n);
    lilv_node_free(n);
    return s;
}
float Lv2Plugin::getParam(int i)  const { return (i >= 0 && i < (int)m_ctrlValues.size()) ? m_ctrlValues[i] : 0.0f; }
void  Lv2Plugin::setParam(int i, float v) { if (i >= 0 && i < (int)m_ctrlValues.size()) m_ctrlValues[i] = v; }

} // namespace Rina::Engine::Plugin