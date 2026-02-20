#pragma once
#include "audio_plugin_host.hpp"
#include <QHash>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#endif
#include <lilv/lilv.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <lv2/options/options.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>
#include <vector>

namespace Rina::Engine::Plugin {

class Lv2Plugin : public IAudioPlugin {
  public:
    ~Lv2Plugin() override { release(); }

    bool load(const QString &uri, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;

    QString name() const override;
    QString format() const override { return "LV2"; }
    int paramCount() const override;
    QString paramName(int i) const override;
    float getParam(int i) const override;
    void setParam(int i, float v) override;
    ParamInfo getParamInfo(int i) const override;

  private:
    // URID Mapper
    static LV2_URID map_uri(LV2_URID_Map_Handle handle, const char *uri);
    LV2_URID_Map m_uridMap = {this, map_uri};
    QHash<QString, LV2_URID> m_uriToId;

    // Worker Schedule
    static LV2_Worker_Status schedule_work(LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data);
    static LV2_Worker_Status worker_respond(LV2_Worker_Respond_Handle handle, uint32_t size, const void *data);
    LV2_Worker_Schedule m_workerSchedule = {this, schedule_work};
    LV2_Worker_Status scheduleWork(uint32_t size, const void *data);
    LV2_Worker_Status workerRespond(uint32_t size, const void *data);

    LilvWorld *m_world = nullptr;
    LilvInstance *m_instance = nullptr;
    const LilvPlugin *m_plugin = nullptr;

    // Ports
    std::vector<int> m_audioInPorts;
    std::vector<int> m_audioOutPorts;
    std::vector<int> m_ctrlPorts;
    std::vector<float> m_ctrlValues;
    std::vector<ParamInfo> m_paramInfos;

    // Buffers (Multi-channel)
    std::vector<std::vector<float>> m_inBufs;
    std::vector<std::vector<float>> m_outBufs;
};

} // namespace Rina::Engine::Plugin