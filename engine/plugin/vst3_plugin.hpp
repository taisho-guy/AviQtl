#pragma once
#include "audio_plugin_host.hpp"
#include <QLibrary>
#include <vector>

// VST3 SDK Headers
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
// IConnectionPoint, int32などの基本型定義を含むヘッダ
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstunits.h>

// IConnectionPoint definition (VST 3.7+)
#if __has_include(<pluginterfaces/vst/ivstconnection.h>)
#include <pluginterfaces/vst/ivstconnection.h>
#define RINA_VST3_HAS_CONNECTION_POINT
#endif
#if __has_include(<pluginterfaces/vst/ivstmessage.h>)
#include <pluginterfaces/vst/ivstmessage.h>
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Rina::Engine::Plugin {

class Vst3Plugin : public IAudioPlugin {
  public:
    ~Vst3Plugin() override { release(); }

    bool load(const QString &path, int index = 0) override;
    void prepare(double sampleRate, int maxBlockSize) override;
    void process(float *buf, int frameCount) override;
    void release() override;
    QString name() const override { return m_name; }
    QString format() const override { return "VST3"; }
    int paramCount() const override;
    QString paramName(int i) const override;
    float getParam(int i) const override;
    void setParam(int i, float v) override;
    ParamInfo getParamInfo(int i) const override;

  private:
    QString m_name;
    bool initialize(const Steinberg::FUID &classId);

    QLibrary m_lib;
    Steinberg::IPluginFactory *m_factory = nullptr;

    // COM-like Smart Pointers
    Steinberg::IPtr<Steinberg::Vst::IComponent> m_component;
    Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> m_processor;
    Steinberg::IPtr<Steinberg::Vst::IEditController> m_controller;

    // オーディオバス
    Steinberg::Vst::AudioBusBuffers m_inBusBuffers;
    Steinberg::Vst::AudioBusBuffers m_outBusBuffers;

    // Buffers
    std::vector<float> m_inL, m_inR, m_outL, m_outR;
    float *m_inPtrs[2] = {};
    float *m_outPtrs[2] = {};

    double m_sampleRate = 48000.0;
    int m_maxBlockSize = 512;
};

} // namespace Rina::Engine::Plugin