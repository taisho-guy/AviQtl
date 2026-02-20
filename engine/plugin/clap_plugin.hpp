#pragma once
#include "audio_plugin_host.hpp"
#include <QLibrary>
#include <clap/clap.h>
#include <clap/ext/thread-pool.h>
#include <mutex>
#include <vector>

namespace Rina::Engine::Plugin {

// イベントリストのヘルパー
class ClapInputEventList {
  public:
    ClapInputEventList();
    const clap_input_events_t *clap_input_events() const { return &m_interface; }
    void clear();

    // パラメータ変更イベントを追加
    void pushParamValue(uint32_t paramId, double value, uint32_t noteId = -1);

  private:
    static uint32_t size(const clap_input_events_t *list);
    static const clap_event_header_t *get(const clap_input_events_t *list, uint32_t index);
    clap_input_events_t m_interface;
    std::vector<const clap_event_header_t *> m_events;
    std::vector<clap_event_param_value_t> m_paramEvents; // イベント実体の保持用
};

class ClapOutputEventList {
  public:
    ClapOutputEventList();
    const clap_output_events_t *clap_output_events() const { return &m_interface; }
    void clear() { m_events.clear(); }

  private:
    static bool try_push(const clap_output_events_t *list, const clap_event_header_t *event);
    clap_output_events_t m_interface;
    std::vector<const clap_event_header_t *> m_events;
};

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

    // スレッドプール実行用ヘルパー
    void executeTask(uint32_t taskIndex);

  private:
    // ホストコールバック
    static const void *host_get_extension(const clap_host_t *host, const char *extension_id);
    static void host_request_restart(const clap_host_t *host);
    static void host_request_process(const clap_host_t *host);
    static void host_request_callback(const clap_host_t *host);
    static bool host_thread_pool_request_exec(const clap_host_t *host, uint32_t num_tasks);

    QLibrary m_lib;
    const clap_plugin_t *m_plugin = nullptr;
    const clap_plugin_entry_t *m_entry = nullptr; // deinit用
    clap_host_t m_host;                           // インスタンスごとのホスト構造体

    clap_audio_buffer_t m_inBuf{}, m_outBuf{};
    std::vector<float> m_inL, m_inR, m_outL, m_outR;
    float *m_inPtrs[2] = {};
    float *m_outPtrs[2] = {};

    QString m_name;
    int m_blockSize = 0;
    double m_sampleRate = 48000.0;
    int64_t m_currentFrame = 0;

    const clap_plugin_params_t *m_paramsExt = nullptr;
    const clap_plugin_thread_pool_t *m_threadPoolExt = nullptr;

    // イベントキュー
    ClapInputEventList m_inEvents;
    ClapOutputEventList m_outEvents;
    std::mutex m_eventMutex; // イベントキュー保護用
};

} // namespace Rina::Engine::Plugin