#include "clap_plugin.hpp"
#include <QDebug>
#include <cstring>

namespace Rina::Engine::Plugin {

// ホストコールバック最小実装
const clap_host_t ClapPlugin::s_host = {
    CLAP_VERSION,
    nullptr, // host_data
    "Rina",
    "RinaProject",
    "0.1",
    "0.1",
    [](const clap_host_t *, const char *) -> const void * { return nullptr; }, // get_extension
    [](const clap_host_t *) {},                                                // request_restart
    [](const clap_host_t *) {},                                                // request_process
    [](const clap_host_t *) {},                                                // request_callback
};

bool ClapPlugin::load(const QString &path, int index) {
    m_lib.setFileName(path);
    if (!m_lib.load()) {
        qWarning() << "[CLAP] ロード失敗:" << m_lib.errorString();
        return false;
    }

    auto *entry = (const clap_plugin_entry_t *)m_lib.resolve("clap_entry");
    if (!entry) {
        qWarning() << "[CLAP] clap_entry が見つかりません";
        return false;
    }

    entry->init(path.toUtf8().constData());
    auto *factory = (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory) {
        qWarning() << "[CLAP] factory 取得失敗";
        return false;
    }

    auto count = factory->get_plugin_count(factory);
    if (index >= (int)count) {
        qWarning() << "[CLAP] インデックス範囲外";
        return false;
    }

    auto *desc = factory->get_plugin_descriptor(factory, index);
    m_name = QString(desc->name);

    m_plugin = factory->create_plugin(factory, &s_host, desc->id);
    if (!m_plugin) {
        qWarning() << "[CLAP] プラグイン生成失敗";
        return false;
    }
    if (!m_plugin->init(m_plugin)) {
        qWarning() << "[CLAP] init 失敗";
        return false;
    }

    m_paramsExt = (const clap_plugin_params_t *)m_plugin->get_extension(m_plugin, CLAP_EXT_PARAMS);

    qInfo() << "[CLAP] ロード成功:" << m_name;
    return true;
}

void ClapPlugin::prepare(double sr, int bs) {
    if (!m_plugin)
        return;
    m_sampleRate = sr;
    m_blockSize = bs;

    m_inL.assign(bs, 0.0f);
    m_inR.assign(bs, 0.0f);
    m_outL.assign(bs, 0.0f);
    m_outR.assign(bs, 0.0f);
    m_inPtrs[0] = m_inL.data();
    m_inPtrs[1] = m_inR.data();
    m_outPtrs[0] = m_outL.data();
    m_outPtrs[1] = m_outR.data();

    // プロセス中の空イベントリスト (null渡しはプラグインによってはクラッシュする)
    static const clap_input_events_t s_emptyIn = {
        nullptr,
        [](const clap_input_events_t *) -> uint32_t { return 0; },
        [](const clap_input_events_t *, uint32_t) -> const clap_event_header_t * { return nullptr; },
    };
    static const clap_output_events_t s_emptyOut = {
        nullptr,
        [](const clap_output_events_t *, const clap_event_header_t *) -> bool { return true; },
    };

    m_inBuf = clap_audio_buffer_t{m_inPtrs, nullptr, 2, 0, 0};
    m_outBuf = clap_audio_buffer_t{m_outPtrs, nullptr, 2, 0, 0};

    m_plugin->activate(m_plugin, sr, 1, (uint32_t)bs);
    m_plugin->start_processing(m_plugin);
    m_emptyIn = s_emptyIn;
    m_emptyOut = s_emptyOut;
}

void ClapPlugin::process(float *buf, int frameCount) {
    if (!m_plugin)
        return;

    // インターリーブ → 分離 L/R
    for (int i = 0; i < frameCount; ++i) {
        m_inL[i] = buf[i * 2];
        m_inR[i] = buf[i * 2 + 1];
    }

    clap_process_t proc{};
    proc.steady_time = -1;
    proc.frames_count = (uint32_t)frameCount;
    proc.transport = nullptr;
    proc.audio_inputs = &m_inBuf;
    proc.audio_outputs = &m_outBuf;
    proc.audio_inputs_count = 1;
    proc.audio_outputs_count = 1;
    proc.in_events = &m_emptyIn;
    proc.out_events = &m_emptyOut;

    m_plugin->process(m_plugin, &proc);

    // 分離 L/R → インターリーブ
    for (int i = 0; i < frameCount; ++i) {
        buf[i * 2] = m_outL[i];
        buf[i * 2 + 1] = m_outR[i];
    }
}

void ClapPlugin::release() {
    if (!m_plugin)
        return;
    m_plugin->stop_processing(m_plugin);
    m_plugin->deactivate(m_plugin);
    m_plugin->destroy(m_plugin);
    m_plugin = nullptr;
    if (m_lib.isLoaded())
        m_lib.unload();
}

int ClapPlugin::paramCount() const {
    if (!m_paramsExt || !m_plugin)
        return 0;
    return (int)m_paramsExt->count(m_plugin);
}
QString ClapPlugin::paramName(int i) const {
    if (!m_paramsExt || !m_plugin)
        return {};
    clap_param_info_t info{};
    m_paramsExt->get_info(m_plugin, (uint32_t)i, &info);
    return QString(info.name);
}
float ClapPlugin::getParam(int i) const {
    if (!m_paramsExt || !m_plugin)
        return 0.0f;
    clap_param_info_t info{};
    m_paramsExt->get_info(m_plugin, (uint32_t)i, &info);
    double v = 0.0;
    m_paramsExt->get_value(m_plugin, info.id, &v);
    return (float)v;
}
void ClapPlugin::setParam(int i, float v) {
    if (!m_paramsExt || !m_plugin)
        return;
    clap_param_info_t info{};
    m_paramsExt->get_info(m_plugin, (uint32_t)i, &info);
    // CLAP のパラメータ変更はイベントキュー経由が正規手順
    // ここでは簡易実装としてリセット＆再設定は省略、
    // 実用時は clap_input_events_t 経由でキューに積む
    (void)info;
    (void)v;
}

} // namespace Rina::Engine::Plugin