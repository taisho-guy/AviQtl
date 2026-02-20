#include "clap_plugin.hpp"
#include <QDebug>
#include <QtConcurrent>
#include <cstring>

namespace Rina::Engine::Plugin {

// --- Event List Implementations ---
ClapInputEventList::ClapInputEventList() { m_interface = {this, size, get}; }
uint32_t ClapInputEventList::size(const clap_input_events_t *list) { return static_cast<const ClapInputEventList *>(list->ctx)->m_events.size(); }
const clap_event_header_t *ClapInputEventList::get(const clap_input_events_t *list, uint32_t index) {
    auto *self = static_cast<const ClapInputEventList *>(list->ctx);
    if (index >= self->m_events.size())
        return nullptr;
    return self->m_events[index];
}

void ClapInputEventList::pushParamValue(uint32_t paramId, double value, uint32_t noteId) {
    clap_event_param_value_t event = {};
    event.header.size = sizeof(event);
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.header.time = 0; // ブロック先頭で適用
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.flags = 0;
    event.param_id = paramId;
    event.cookie = nullptr;
    event.note_id = noteId;
    event.value = value;

    m_paramEvents.push_back(event);
    m_events.push_back(&m_paramEvents.back().header);
}

void ClapInputEventList::clear() {
    m_events.clear();
    m_paramEvents.clear();
}

ClapOutputEventList::ClapOutputEventList() { m_interface = {this, try_push}; }
bool ClapOutputEventList::try_push(const clap_output_events_t *list, const clap_event_header_t *event) {
    // 出力イベントは今回は無視またはログ出力のみ（必要に応じて実装）
    return true;
}

// --- ClapPlugin Implementation ---

bool ClapPlugin::load(const QString &path, int index) {
    m_lib.setFileName(path);
    if (!m_lib.load()) {
        qWarning() << "[CLAP] ロード失敗:" << m_lib.errorString();
        return false;
    }

    m_entry = (const clap_plugin_entry_t *)m_lib.resolve("clap_entry");
    if (!m_entry) {
        qWarning() << "[CLAP] clap_entry が見つかりません";
        return false;
    }

    m_entry->init(path.toUtf8().constData());
    auto *factory = (const clap_plugin_factory_t *)m_entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
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

    // ホスト構造体の初期化
    m_host.clap_version = CLAP_VERSION;
    m_host.host_data = this;
    m_host.name = "Rina";
    m_host.vendor = "RinaProject";
    m_host.url = "https://codeberg.org/taisho-guy/Rina";
    m_host.version = "0.1";
    m_host.get_extension = host_get_extension;
    m_host.request_restart = host_request_restart;
    m_host.request_process = host_request_process;
    m_host.request_callback = host_request_callback;

    m_plugin = factory->create_plugin(factory, &m_host, desc->id);
    if (!m_plugin) {
        qWarning() << "[CLAP] プラグイン生成失敗";
        return false;
    }
    if (!m_plugin->init(m_plugin)) {
        qWarning() << "[CLAP] init 失敗";
        return false;
    }

    m_paramsExt = (const clap_plugin_params_t *)m_plugin->get_extension(m_plugin, CLAP_EXT_PARAMS);
    m_threadPoolExt = (const clap_plugin_thread_pool_t *)m_plugin->get_extension(m_plugin, CLAP_EXT_THREAD_POOL);

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

    m_inBuf = clap_audio_buffer_t{m_inPtrs, nullptr, 2, 0, 0};
    m_outBuf = clap_audio_buffer_t{m_outPtrs, nullptr, 2, 0, 0};

    m_plugin->activate(m_plugin, sr, 1, (uint32_t)bs);
    m_plugin->start_processing(m_plugin);
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
    proc.frames_count = (uint32_t)frameCount;
    proc.transport = nullptr;
    proc.audio_inputs = &m_inBuf;
    proc.audio_outputs = &m_outBuf;
    proc.audio_inputs_count = 1;
    proc.audio_outputs_count = 1;
    proc.in_events = m_inEvents.clap_input_events();
    proc.out_events = m_outEvents.clap_output_events();

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_plugin->process(m_plugin, &proc);
    m_inEvents.clear();
    m_outEvents.clear();

    // 分離 L/R → インターリーブ
    for (int i = 0; i < frameCount; ++i) {
        buf[i * 2] = m_outL[i];
        buf[i * 2 + 1] = m_outR[i];
    }

    m_currentFrame += frameCount;
}

void ClapPlugin::release() {
    if (!m_plugin)
        return;
    m_plugin->stop_processing(m_plugin);
    m_plugin->deactivate(m_plugin);
    m_plugin->destroy(m_plugin);
    m_plugin = nullptr;

    if (m_entry) {
        m_entry->deinit();
        m_entry = nullptr;
    }
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
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_inEvents.pushParamValue(info.id, v);
}

// --- Host Callbacks ---

const void *ClapPlugin::host_get_extension(const clap_host_t *host, const char *extension_id) {
    if (strcmp(extension_id, CLAP_EXT_THREAD_POOL) == 0) {
        static const clap_host_thread_pool_t thread_pool = {host_thread_pool_request_exec};
        return &thread_pool;
    }
    return nullptr;
}

void ClapPlugin::host_request_restart(const clap_host_t *) {}
void ClapPlugin::host_request_process(const clap_host_t *) {}
void ClapPlugin::host_request_callback(const clap_host_t *) {}

bool ClapPlugin::host_thread_pool_request_exec(const clap_host_t *host, uint32_t num_tasks) {
    auto *self = static_cast<ClapPlugin *>(host->host_data);
    if (!self || !self->m_threadPoolExt)
        return false;

    // QtConcurrent を使用してタスクを並列実行
    // CLAPの仕様上、この関数内で完了を待つ必要がある（ブロッキング）
    QList<uint32_t> tasks;
    tasks.reserve(num_tasks);
    for (uint32_t i = 0; i < num_tasks; ++i)
        tasks.push_back(i);

    QtConcurrent::blockingMap(tasks, [self](uint32_t taskIndex) { self->executeTask(taskIndex); });

    return true;
}

void ClapPlugin::executeTask(uint32_t taskIndex) {
    if (m_plugin && m_threadPoolExt) {
        m_threadPoolExt->exec(m_plugin, taskIndex);
    }
}

} // namespace Rina::Engine::Plugin