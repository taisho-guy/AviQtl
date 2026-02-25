#include "lv2_plugin.hpp"
#include "audio_plugin_manager.hpp"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <lv2/core/lv2.h>
#include <lv2/options/options.h>

namespace Rina::Engine::Plugin {

bool Lv2Plugin::load(const QString &uri, int) {
    m_world = AudioPluginManager::instance().getLilvWorld();

    LilvNode *uriNode = lilv_new_uri(m_world, uri.toUtf8().constData());
    const LilvPlugins *all = lilv_world_get_all_plugins(m_world);
    m_plugin = lilv_plugins_get_by_uri(all, uriNode);
    lilv_node_free(uriNode);

    if (!m_plugin) {
        qWarning() << "[LV2] プラグインが見つかりません:" << uri;
        return false;
    }

    // ポート分類
    m_audioInPorts.clear();
    m_audioOutPorts.clear();
    m_ctrlPorts.clear();
    m_ctrlValues.clear();
    m_paramInfos.clear();

    LilvNode *audioClass = lilv_new_uri(m_world, LILV_URI_AUDIO_PORT);
    LilvNode *inputClass = lilv_new_uri(m_world, LILV_URI_INPUT_PORT);
    LilvNode *outputClass = lilv_new_uri(m_world, LILV_URI_OUTPUT_PORT);
    LilvNode *ctrlClass = lilv_new_uri(m_world, LILV_URI_CONTROL_PORT);

    // プロパティ判定用ノード
    LilvNode *toggledProp = lilv_new_uri(m_world, "http://lv2plug.in/ns/ext/port-props#toggled");
    LilvNode *integerProp = lilv_new_uri(m_world, "http://lv2plug.in/ns/ext/port-props#integer");
    LilvNode *logProp = lilv_new_uri(m_world, "http://lv2plug.in/ns/ext/port-props#logarithmic");

    uint32_t numPorts = lilv_plugin_get_num_ports(m_plugin);
    for (uint32_t i = 0; i < numPorts; ++i) {
        const LilvPort *port = lilv_plugin_get_port_by_index(m_plugin, i);
        if (lilv_port_is_a(m_plugin, port, audioClass)) {
            if (lilv_port_is_a(m_plugin, port, inputClass))
                m_audioInPorts.push_back((int)i);
            else if (lilv_port_is_a(m_plugin, port, outputClass))
                m_audioOutPorts.push_back((int)i);
        } else if (lilv_port_is_a(m_plugin, port, ctrlClass)) {
            if (lilv_port_is_a(m_plugin, port, inputClass)) {
                m_ctrlPorts.push_back((int)i);

                // パラメータ情報の取得
                ParamInfo info;
                LilvNode *n = lilv_port_get_name(m_plugin, port);
                info.name = QString::fromUtf8(lilv_node_as_string(n));
                lilv_node_free(n);

                LilvNode *defNode = nullptr, *minNode = nullptr, *maxNode = nullptr;
                lilv_port_get_range(m_plugin, port, &defNode, &minNode, &maxNode);

                if (minNode)
                    info.min = lilv_node_as_float(minNode);
                if (maxNode)
                    info.max = lilv_node_as_float(maxNode);
                if (defNode)
                    info.defaultValue = lilv_node_as_float(defNode);

                lilv_node_free(defNode);
                lilv_node_free(minNode);
                lilv_node_free(maxNode);

                // プロパティ確認
                if (lilv_port_has_property(m_plugin, port, toggledProp))
                    info.isToggle = true;
                if (lilv_port_has_property(m_plugin, port, integerProp))
                    info.isInteger = true;
                if (lilv_port_has_property(m_plugin, port, logProp))
                    info.isLog = true;

                // 安全策: 範囲がない場合は 0.0 - 1.0
                if (info.min == info.max) {
                    info.min = 0.0f;
                    info.max = 1.0f;
                }

                m_paramInfos.push_back(info);
                m_ctrlValues.push_back(info.defaultValue);
            }
        }
    }
    lilv_node_free(audioClass);
    lilv_node_free(inputClass);
    lilv_node_free(outputClass);
    lilv_node_free(ctrlClass);
    lilv_node_free(toggledProp);
    lilv_node_free(integerProp);
    lilv_node_free(logProp);

    // URIDマップの初期化
    m_uriToId.clear();
    // 0 は無効なIDとして予約されているため、1から開始する実装が多いが、
    // map_uriの実装でサイズ+1を返すようにする。

    LilvNode *n = lilv_plugin_get_name(m_plugin);
    qInfo() << "[LV2] ロード成功:" << lilv_node_as_string(n);
    lilv_node_free(n);
    return true;
}

void Lv2Plugin::prepare(double sampleRate, int maxBlockSize) {
    if (!m_plugin)
        return;
    if (m_instance) {
        lilv_instance_deactivate(m_instance);
        lilv_instance_free(m_instance);
    }

    // バッファの確保
    m_inBufs.resize(m_audioInPorts.size());
    for (auto &b : m_inBufs)
        b.assign(maxBlockSize, 0.0f);

    m_outBufs.resize(m_audioOutPorts.size());
    for (auto &b : m_outBufs)
        b.assign(maxBlockSize, 0.0f);

    // Options Feature の準備
    const LV2_Options_Option options[] = {
        {LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL} // 終端
    };
    const LV2_Feature options_feature = {LV2_OPTIONS__options, (void *)options};

    // URID Feature
    const LV2_Feature urid_feature = {LV2_URID__map, &m_uridMap};

    // Worker Feature
    const LV2_Feature worker_feature = {LV2_WORKER__schedule, &m_workerSchedule};

    const LV2_Feature *features[] = {&options_feature, &urid_feature, &worker_feature, nullptr};

    m_instance = lilv_plugin_instantiate(m_plugin, sampleRate, features);

    if (!m_instance) {
        qWarning() << "[LV2] instantiate 失敗 (Features不足の可能性あり)";
        return;
    }

    // ポート接続
    for (size_t i = 0; i < m_audioInPorts.size(); ++i) {
        lilv_instance_connect_port(m_instance, m_audioInPorts[i], m_inBufs[i].data());
    }
    for (size_t i = 0; i < m_audioOutPorts.size(); ++i) {
        lilv_instance_connect_port(m_instance, m_audioOutPorts[i], m_outBufs[i].data());
    }
    for (int k = 0; k < (int)m_ctrlPorts.size(); ++k)
        lilv_instance_connect_port(m_instance, m_ctrlPorts[k], &m_ctrlValues[k]);

    lilv_instance_activate(m_instance);
}

void Lv2Plugin::process(float *buf, int frameCount) {
    if (!m_instance)
        return;

    // De-interleave (L,R,L,R... -> LLL..., RRR...)
    // 入力ポート数に合わせて分配。1つならLのみ、2つならL/R。
    for (size_t ch = 0; ch < m_inBufs.size(); ++ch) {
        float *dst = m_inBufs[ch].data();
        // 入力がステレオ(2ch)前提なので、ch 0=L, ch 1=R, それ以上は無音
        if (ch < 2) {
            for (int i = 0; i < frameCount; ++i) {
                dst[i] = buf[i * 2 + ch];
            }
        } else {
            std::fill(dst, dst + frameCount, 0.0f);
        }
    }

    lilv_instance_run(m_instance, (uint32_t)frameCount);

    // Interleave (LLL..., RRR... -> L,R,L,R...)
    // 出力ポート数に合わせて合成。1つならL=R=Out[0]、2つならL=Out[0], R=Out[1]
    if (m_outBufs.size() == 1) {
        float *src = m_outBufs[0].data();
        for (int i = 0; i < frameCount; ++i)
            buf[i * 2] = buf[i * 2 + 1] = src[i];
    } else if (m_outBufs.size() >= 2) {
        float *srcL = m_outBufs[0].data();
        float *srcR = m_outBufs[1].data();
        for (int i = 0; i < frameCount; ++i) {
            buf[i * 2] = srcL[i];
            buf[i * 2 + 1] = srcR[i];
        }
    }
}

void Lv2Plugin::release() {
    if (m_instance) {
        lilv_instance_deactivate(m_instance);
        lilv_instance_free(m_instance);
        m_instance = nullptr;
    }
    m_world = nullptr;
}

QString Lv2Plugin::name() const {
    if (!m_plugin)
        return {};
    LilvNode *n = lilv_plugin_get_name(m_plugin);
    QString s = lilv_node_as_string(n);
    lilv_node_free(n);
    return s;
}
int Lv2Plugin::paramCount() const { return (int)m_ctrlPorts.size(); }
QString Lv2Plugin::paramName(int i) const {
    if (!m_plugin || i < 0 || i >= (int)m_ctrlPorts.size())
        return {};
    const LilvPort *p = lilv_plugin_get_port_by_index(m_plugin, m_ctrlPorts[i]);
    LilvNode *n = lilv_port_get_name(m_plugin, p);
    QString s = lilv_node_as_string(n);
    lilv_node_free(n);
    return s;
}
float Lv2Plugin::getParam(int i) const { return (i >= 0 && i < (int)m_ctrlValues.size()) ? m_ctrlValues[i] : 0.0f; }
void Lv2Plugin::setParam(int i, float v) {
    if (i >= 0 && i < (int)m_ctrlValues.size())
        m_ctrlValues[i] = v;
}

ParamInfo Lv2Plugin::getParamInfo(int i) const {
    if (i >= 0 && i < (int)m_paramInfos.size())
        return m_paramInfos[i];
    return {};
}

// --- URID Mapper ---
LV2_URID Lv2Plugin::map_uri(LV2_URID_Map_Handle handle, const char *uri) {
    auto *plugin = (Lv2Plugin *)handle;
    QString key = QString::fromUtf8(uri);
    if (!plugin->m_uriToId.contains(key)) {
        // IDは1から始まる必要がある
        plugin->m_uriToId[key] = plugin->m_uriToId.size() + 1;
    }
    return plugin->m_uriToId[key];
}

// --- Worker ---
LV2_Worker_Status Lv2Plugin::schedule_work(LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data) {
    auto *plugin = (Lv2Plugin *)handle;
    return plugin->scheduleWork(size, data);
}

LV2_Worker_Status Lv2Plugin::worker_respond(LV2_Worker_Respond_Handle handle, uint32_t size, const void *data) {
    auto *plugin = (Lv2Plugin *)handle;
    return plugin->workerRespond(size, data);
}

LV2_Worker_Status Lv2Plugin::scheduleWork(uint32_t size, const void *data) {
    // 簡易実装: リアルタイムスレッドで同期実行してしまう（音切れのリスクはあるが機能は動く）
    // 本来は別スレッドのリングバッファに積むべき
    const LV2_Worker_Interface *worker = (const LV2_Worker_Interface *)lilv_instance_get_extension_data(m_instance, LV2_WORKER__interface);
    if (worker && worker->work) {
        LV2_Handle lv2_handle = lilv_instance_get_handle(m_instance);
        worker->work(lv2_handle, worker_respond, this, size, data);
    }
    return LV2_WORKER_SUCCESS;
}

LV2_Worker_Status Lv2Plugin::workerRespond(uint32_t size, const void *data) {
    const LV2_Worker_Interface *worker = (const LV2_Worker_Interface *)lilv_instance_get_extension_data(m_instance, LV2_WORKER__interface);
    if (worker && worker->work_response) {
        LV2_Handle lv2_handle = lilv_instance_get_handle(m_instance);
        return worker->work_response(lv2_handle, size, data);
    }
    return LV2_WORKER_SUCCESS;
}
} // namespace Rina::Engine::Plugin