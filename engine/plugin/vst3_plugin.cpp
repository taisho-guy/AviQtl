#include "vst3_plugin.hpp"
#include <QDebug>
#include <vector>

// VST3 SDKの実装ファイルを直接インクルードしてシンボル未定義エラーを解消
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#include <pluginterfaces/base/funknown.cpp>
#include <pluginterfaces/base/ustring.cpp>
#include <public.sdk/source/vst/vstinitiids.cpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Rina::Engine::Plugin {

bool Vst3Plugin::load(const QString &path, int index) {
    m_lib.setFileName(path);
    if (!m_lib.load()) {
        qWarning() << "[VST3] Failed to load library:" << path << m_lib.errorString();
        return false;
    }

    // GetPluginFactory entry point
    typedef Steinberg::IPluginFactory *(PLUGIN_API * GetFactoryProc)();
    auto getFactory = (GetFactoryProc)m_lib.resolve("GetPluginFactory");
    if (!getFactory) {
        qWarning() << "[VST3] GetPluginFactory symbol not found in" << path;
        return false;
    }

    m_factory = getFactory();
    if (!m_factory) {
        qWarning() << "[VST3] Factory is null";
        return false;
    }

    // Find Audio Effect Class
    Steinberg::PClassInfo classInfo;
    Steinberg::FUID componentId;
    bool found = false;
    int audioEffectCount = 0;

    for (int i = 0; i < m_factory->countClasses(); ++i) {
        if (m_factory->getClassInfo(i, &classInfo) == Steinberg::kResultOk) {
            if (strcmp(classInfo.category, kVstAudioEffectClass) == 0) {
                if (audioEffectCount == index) {
                    componentId = Steinberg::FUID::fromTUID(classInfo.cid);
                    m_name = QString::fromUtf8(classInfo.name);
                    found = true;
                    break;
                }
                audioEffectCount++;
            }
        }
    }

    if (!found) {
        qWarning() << "[VST3] No Audio Effect class found at index" << index << "in" << path;
        return false;
    }

    return initialize(componentId);
}

bool Vst3Plugin::initialize(const Steinberg::FUID &classId) {
    // Create Component
    if (m_factory->createInstance(classId, Steinberg::Vst::IComponent::iid, (void **)&m_component) != Steinberg::kResultOk) {
        qWarning() << "[VST3] Failed to create component instance";
        return false;
    }

    // Initialize Component
    if (m_component->initialize(nullptr) != Steinberg::kResultOk) {
        qWarning() << "[VST3] Failed to initialize component";
        return false;
    }

    // Get AudioProcessor
    if (m_component->queryInterface(Steinberg::Vst::IAudioProcessor::iid, (void **)&m_processor) != Steinberg::kResultOk) {
        qWarning() << "[VST3] Component does not implement IAudioProcessor";
        return false;
    }

    // Get EditController
    // 1. Try queryInterface from component (Single Component)
    if (m_component->queryInterface(Steinberg::Vst::IEditController::iid, (void **)&m_controller) != Steinberg::kResultOk) {
        // 2. Try creating separate controller (Split Component)
        Steinberg::TUID controllerCID;
        if (m_component->getControllerClassId(controllerCID) == Steinberg::kResultOk) {
            Steinberg::FUID cid = Steinberg::FUID::fromTUID(controllerCID);
            if (m_factory->createInstance(cid, Steinberg::Vst::IEditController::iid, (void **)&m_controller) == Steinberg::kResultOk) {
                if (m_controller->initialize(nullptr) != Steinberg::kResultOk) {
                    m_controller = nullptr; // Failed to init controller
                }
            }
        }
    }

    // Connect Component and Controller if needed (IConnectionPoint)
#ifdef RINA_VST3_HAS_CONNECTION_POINT
    if (m_component && m_controller) {
        Steinberg::Vst::IConnectionPoint *componentCP = nullptr;
        Steinberg::Vst::IConnectionPoint *controllerCP = nullptr;

        m_component->queryInterface(Steinberg::Vst::IConnectionPoint::iid, (void **)&componentCP);
        m_controller->queryInterface(Steinberg::Vst::IConnectionPoint::iid, (void **)&controllerCP);

        if (componentCP && controllerCP) {
            componentCP->connect(controllerCP);
            controllerCP->connect(componentCP);
        }

        if (componentCP)
            componentCP->release();
        if (controllerCP)
            controllerCP->release();
    }
#endif

    qInfo() << "[VST3] Loaded:" << m_name;
    return true;
}

void Vst3Plugin::prepare(double sampleRate, int maxBlockSize) {
    if (!m_component || !m_processor)
        return;

    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;

    Steinberg::Vst::ProcessSetup setup;
    setup.processMode = Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize = Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock = maxBlockSize;
    setup.sampleRate = sampleRate;

    m_processor->setupProcessing(setup);

    // Set Speaker Arrangement (Stereo)
    Steinberg::Vst::SpeakerArrangement inputArr = Steinberg::Vst::SpeakerArr::kStereo;
    Steinberg::Vst::SpeakerArrangement outputArr = Steinberg::Vst::SpeakerArr::kStereo;
    m_processor->setBusArrangements(&inputArr, 1, &outputArr, 1);

    m_component->setActive(true);
    m_processor->setProcessing(true);

    // Allocate Buffers
    m_inL.resize(maxBlockSize);
    m_inR.resize(maxBlockSize);
    m_outL.resize(maxBlockSize);
    m_outR.resize(maxBlockSize);

    m_inPtrs[0] = m_inL.data();
    m_inPtrs[1] = m_inR.data();
    m_outPtrs[0] = m_outL.data();
    m_outPtrs[1] = m_outR.data();

    // Setup BusBuffers
    m_inBusBuffers.numChannels = 2;
    m_inBusBuffers.silenceFlags = 0;
    m_inBusBuffers.channelBuffers32 = m_inPtrs;

    m_outBusBuffers.numChannels = 2;
    m_outBusBuffers.silenceFlags = 0;
    m_outBusBuffers.channelBuffers32 = m_outPtrs;
}

void Vst3Plugin::process(float *buf, int frameCount) {
    if (!m_processor)
        return;

    // バッファサイズ不足時の安全策 (動的リサイズ)
    if (frameCount > (int)m_inL.size()) {
        m_inL.resize(frameCount);
        m_inR.resize(frameCount);
        m_outL.resize(frameCount);
        m_outR.resize(frameCount);

        m_inPtrs[0] = m_inL.data();
        m_inPtrs[1] = m_inR.data();
        m_outPtrs[0] = m_outL.data();
        m_outPtrs[1] = m_outR.data();
    }

    // De-interleave
    for (int i = 0; i < frameCount; ++i) {
        m_inL[i] = buf[i * 2];
        m_inR[i] = buf[i * 2 + 1];
    }

    // 【重要】構造体をゼロ初期化しないと、未定義のポインタ(eventHandlers等)にアクセスされクラッシュする
    Steinberg::Vst::ProcessData data = {};
    data.processMode = Steinberg::Vst::kRealtime;
    data.symbolicSampleSize = Steinberg::Vst::kSample32;
    data.numSamples = frameCount;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &m_inBusBuffers;
    data.outputs = &m_outBusBuffers;

    m_processor->process(data);

    // Interleave
    for (int i = 0; i < frameCount; ++i) {
        buf[i * 2] = m_outL[i];
        buf[i * 2 + 1] = m_outR[i];
    }
}

void Vst3Plugin::release() {
    if (m_processor) {
        m_processor->setProcessing(false);
    }
    if (m_component) {
        m_component->setActive(false);
    }

    if (m_controller) {
        m_controller->terminate();
        m_controller = nullptr;
    }
    if (m_component) {
        m_component->terminate();
        m_component = nullptr;
    }
    m_processor = nullptr;
    m_factory = nullptr;

    if (m_lib.isLoaded()) {
        m_lib.unload();
    }
}

int Vst3Plugin::paramCount() const { return m_controller ? m_controller->getParameterCount() : 0; }

QString Vst3Plugin::paramName(int i) const {
    if (!m_controller)
        return "";
    Steinberg::Vst::ParameterInfo info = {};
    if (m_controller->getParameterInfo(i, info) == Steinberg::kResultOk) {
        return QString::fromUtf16((const char16_t *)info.title);
    }
    return "";
}

float Vst3Plugin::getParam(int i) const {
    if (!m_controller)
        return 0.0f;
    return (float)m_controller->getParamNormalized(i);
}

void Vst3Plugin::setParam(int i, float v) {
    if (!m_controller)
        return;
    m_controller->setParamNormalized(i, (double)v);
}

ParamInfo Vst3Plugin::getParamInfo(int i) const {
    ParamInfo pInfo;
    if (!m_controller)
        return pInfo;

    Steinberg::Vst::ParameterInfo info = {};
    if (m_controller->getParameterInfo(i, info) == Steinberg::kResultOk) {
        pInfo.name = QString::fromUtf16((const char16_t *)info.title);
        pInfo.min = 0.0f;
        pInfo.max = 1.0f;
        pInfo.defaultValue = (float)info.defaultNormalizedValue;
        if (info.stepCount > 0) {
            pInfo.isInteger = true;
        }
    }
    return pInfo;
}

} // namespace Rina::Engine::Plugin