#include "audio_plugin_manager.hpp"
#include "../../core/include/settings_manager.hpp"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <cstring>
#include <vector>

#include <CarlaNativePlugin.h>

namespace Rina::Engine::Plugin {

namespace {

static bool mapFormatToCarlaType(const QString &format, CarlaBackend::PluginType &ptype) {
    if (format == "LADSPA") {
        ptype = CarlaBackend::PLUGIN_LADSPA;
        return true;
    }
    if (format == "DSSI") {
        ptype = CarlaBackend::PLUGIN_DSSI;
        return true;
    }
    if (format == "LV2") {
        ptype = CarlaBackend::PLUGIN_LV2;
        return true;
    }
    if (format == "VST2") {
        ptype = CarlaBackend::PLUGIN_VST2;
        return true;
    }
    if (format == "VST3") {
        ptype = CarlaBackend::PLUGIN_VST3;
        return true;
    }
    return false;
}

static QString safeQString(const char *s) { return s ? QString::fromUtf8(s) : QString(); }

static float clamp01(float v) {
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

class CarlaHostedPlugin final : public IAudioPlugin {
  public:
    CarlaHostedPlugin(const PluginInfo &info) : m_info(info) {}

    ~CarlaHostedPlugin() override { release(); }

    void ensureBuffers(int frames) {
        if (frames <= 0)
            return;
        if ((int)m_inL.size() < frames) {
            m_inL.resize(frames, 0.0f);
            m_inR.resize(frames, 0.0f);
            m_outL.resize(frames, 0.0f);
            m_outR.resize(frames, 0.0f);
        }
    }

    void deinterleave(const float *src, int frames) {
        ensureBuffers(frames);
        for (int i = 0; i < frames; ++i) {
            m_inL[i] = src[i * 2 + 0];
            m_inR[i] = src[i * 2 + 1];
        }
    }

    void interleave(float *dst, int frames) const {
        for (int i = 0; i < frames; ++i) {
            dst[i * 2 + 0] = m_outL[i];
            dst[i * 2 + 1] = m_outR[i];
        }
    }

    // NativeHostDescriptor コールバック (static)
    static uint32_t s_getBufferSize(NativeHostHandle h) { return static_cast<uint32_t>(static_cast<CarlaHostedPlugin *>(h)->m_maxBlockSize); }
    static double s_getSampleRate(NativeHostHandle h) { return static_cast<CarlaHostedPlugin *>(h)->m_sampleRate; }
    static bool s_isOffline(NativeHostHandle) { return false; }
    static const NativeTimeInfo *s_getTimeInfo(NativeHostHandle h) { return &static_cast<CarlaHostedPlugin *>(h)->m_timeInfo; }
    static bool s_writeMidiEvent(NativeHostHandle, const NativeMidiEvent *) { return false; }
    static void s_uiParameterChanged(NativeHostHandle, uint32_t, float) {}
    static void s_uiMidiProgramChanged(NativeHostHandle, uint8_t, uint32_t, uint32_t) {}
    static void s_uiCustomDataChanged(NativeHostHandle, const char *, const char *) {}
    static void s_uiClosed(NativeHostHandle) {}
    static const char *s_uiOpenFile(NativeHostHandle, bool, const char *, const char *) { return nullptr; }
    static const char *s_uiSaveFile(NativeHostHandle, bool, const char *, const char *) { return nullptr; }
    static intptr_t s_dispatcher(NativeHostHandle, NativeHostDispatcherOpcode, int32_t, intptr_t, void *, float) { return 0; }

    bool load(const QString &path, int index = 0) override {
        Q_UNUSED(index)

        CarlaBackend::PluginType ptype = CarlaBackend::PLUGIN_NONE;
        if (!mapFormatToCarlaType(m_info.format, ptype)) {
            qWarning() << "[CarlaHostedPlugin] 未対応フォーマット:" << m_info.format;
            return false;
        }

        const auto &sm = Rina::Core::SettingsManager::instance();
        if (m_sampleRate <= 1.0)
            m_sampleRate = sm.value("defaultProjectSampleRate", 48000).toDouble();
        if (m_maxBlockSize <= 0)
            m_maxBlockSize = sm.value("audioPluginMaxBlockSize", 512).toInt();

        m_uiNameBuf = m_info.name.toUtf8();

        m_hostDesc.handle = static_cast<NativeHostHandle>(this);
        m_hostDesc.resourceDir = "/usr/share/carla/resources";
        m_hostDesc.uiName = m_uiNameBuf.constData();
        m_hostDesc.uiParentId = 0;
        m_hostDesc.get_buffer_size = s_getBufferSize;
        m_hostDesc.get_sample_rate = s_getSampleRate;
        m_hostDesc.is_offline = s_isOffline;
        m_hostDesc.get_time_info = s_getTimeInfo;
        m_hostDesc.write_midi_event = s_writeMidiEvent;
        m_hostDesc.ui_parameter_changed = s_uiParameterChanged;
        m_hostDesc.ui_midi_program_changed = s_uiMidiProgramChanged;
        m_hostDesc.ui_custom_data_changed = s_uiCustomDataChanged;
        m_hostDesc.ui_closed = s_uiClosed;
        m_hostDesc.ui_open_file = s_uiOpenFile;
        m_hostDesc.ui_save_file = s_uiSaveFile;
        m_hostDesc.dispatcher = s_dispatcher;

        m_descriptor = carla_get_native_rack_plugin();
        if (m_descriptor == nullptr) {
            qWarning() << "[CarlaHostedPlugin] carla_get_native_rack_plugin が nullptr:" << m_info.name;
            return false;
        }

        m_nativeHandle = m_descriptor->instantiate(&m_hostDesc);
        if (m_nativeHandle == nullptr) {
            qWarning() << "[CarlaHostedPlugin] instantiate() が nullptr:" << m_info.name;
            m_descriptor = nullptr;
            return false;
        }

        m_hostHandle = carla_create_native_plugin_host_handle(m_descriptor, m_nativeHandle);
        if (m_hostHandle == nullptr) {
            qWarning() << "[CarlaHostedPlugin] carla_create_native_plugin_host_handle が nullptr:" << m_info.name;
            m_descriptor->cleanup(m_nativeHandle);
            m_nativeHandle = nullptr;
            m_descriptor = nullptr;
            return false;
        }

        // Native Rack はデフォルトで CONTINUOUS_RACK かつステレオ強制のため設定不要
        const QByteArray filename = path.toUtf8();
        const QByteArray name = m_info.name.toUtf8();

        // LV2: carla-discovery は label を "bundle.lv2/http://..." 形式で出力する。
        // carla_add_plugin に渡す label は純粋な URI でなければならないため抽出する。
        QString lv2UriStr = m_info.label;
        if (ptype == CarlaBackend::PLUGIN_LV2) {
            const int dotLv2 = lv2UriStr.indexOf(QLatin1String(".lv2/"));
            if (dotLv2 >= 0)
                lv2UriStr = lv2UriStr.mid(dotLv2 + 5);
        }
        const QByteArray label = lv2UriStr.toUtf8();

        m_loaded = carla_add_plugin(m_hostHandle, CarlaBackend::BINARY_POSIX64, ptype, filename.isEmpty() ? nullptr : filename.constData(), name.isEmpty() ? nullptr : name.constData(), label.isEmpty() ? nullptr : label.constData(), m_info.uniqueId,
                                    nullptr, CarlaBackend::PLUGIN_OPTIONS_NULL);

        if (!m_loaded) {
            qWarning() << "[CarlaHostedPlugin] carla_add_plugin failed:" << m_info.name << m_info.path;
            carla_host_handle_free(m_hostHandle);
            m_hostHandle = nullptr;
            m_descriptor->cleanup(m_nativeHandle);
            m_nativeHandle = nullptr;
            m_descriptor = nullptr;
            return false;
        }

        carla_set_active(m_hostHandle, m_pluginId, true);
        m_descriptor->activate(m_nativeHandle);
        ensureBuffers(m_maxBlockSize);
        qDebug() << "[CarlaHostedPlugin] NativePlugin ロード完了:" << m_info.name;
        return true;
    }

    void prepare(double sampleRate, int maxBlockSize) override {
        m_sampleRate = sampleRate > 1.0 ? sampleRate : 48000.0;
        m_maxBlockSize = maxBlockSize > 0 ? maxBlockSize : 512;
        ensureBuffers(m_maxBlockSize);
        // サンプルレート/バッファサイズは NativeHostDescriptor コールバックで動的に返す
    }

    void process(float *buf, int frameCount) override {
        if (!m_loaded || m_descriptor == nullptr || m_nativeHandle == nullptr || buf == nullptr || frameCount <= 0)
            return;

        deinterleave(buf, frameCount);

        std::fill(m_outL.begin(), m_outL.begin() + frameCount, 0.0f);
        std::fill(m_outR.begin(), m_outR.begin() + frameCount, 0.0f);

        float *inBufs[2] = {m_inL.data(), m_inR.data()};
        float *outBufs[2] = {m_outL.data(), m_outR.data()};

        m_descriptor->process(m_nativeHandle, inBufs, outBufs, static_cast<uint32_t>(frameCount), nullptr, 0);

        interleave(buf, frameCount);
    }

    bool active() const override { return m_loaded; }

    void release() override {
        if (m_descriptor != nullptr && m_nativeHandle != nullptr)
            m_descriptor->deactivate(m_nativeHandle);

        if (m_hostHandle != nullptr) {
            carla_host_handle_free(m_hostHandle);
            m_hostHandle = nullptr;
        }

        if (m_descriptor != nullptr && m_nativeHandle != nullptr) {
            m_descriptor->cleanup(m_nativeHandle);
            m_nativeHandle = nullptr;
        }

        m_descriptor = nullptr;
        m_loaded = false;
        m_inL.clear();
        m_inR.clear();
        m_outL.clear();
        m_outR.clear();
    }

    QString name() const override { return m_info.name; }
    QString format() const override { return m_info.format; }

    int paramCount() const override {
        if (!m_loaded || m_hostHandle == nullptr)
            return 0;
        return static_cast<int>(carla_get_parameter_count(m_hostHandle, m_pluginId));
    }

    QString paramName(int i) const override {
        if (!m_loaded || m_hostHandle == nullptr || i < 0)
            return {};
        const CarlaParameterInfo *info = carla_get_parameter_info(m_hostHandle, m_pluginId, static_cast<uint32_t>(i));
        return info ? safeQString(info->name) : QString{};
    }

    float getParam(int i) const override {
        if (!m_loaded || m_hostHandle == nullptr || i < 0)
            return 0.0f;
        return carla_get_current_parameter_value(m_hostHandle, m_pluginId, static_cast<uint32_t>(i));
    }

    void setParam(int i, float v) override {
        if (!m_loaded || m_hostHandle == nullptr || i < 0)
            return;
        carla_set_parameter_value(m_hostHandle, m_pluginId, static_cast<uint32_t>(i), clamp01(v));
    }

    ParamInfo getParamInfo(int i) const override {
        ParamInfo out;
        if (!m_loaded || m_hostHandle == nullptr || i < 0)
            return out;
        const uint32_t pid = static_cast<uint32_t>(i);
        const CarlaParameterInfo *info = carla_get_parameter_info(m_hostHandle, m_pluginId, pid);
        if (info)
            out.name = safeQString(info->name);
        out.defaultValue = carla_get_default_parameter_value(m_hostHandle, m_pluginId, pid);
        out.min = 0.0f;
        out.max = 1.0f;
        return out;
    }

  private:
    const NativePluginDescriptor *m_descriptor = nullptr;
    NativePluginHandle m_nativeHandle = nullptr;
    CarlaHostHandle m_hostHandle = nullptr;
    NativeHostDescriptor m_hostDesc = {};
    NativeTimeInfo m_timeInfo = {};
    QByteArray m_uiNameBuf;
    uint m_pluginId = 0;
    PluginInfo m_info;
    bool m_loaded = false;
    double m_sampleRate = 48000.0;
    int m_maxBlockSize = 512;
    std::vector<float> m_inL;
    std::vector<float> m_inR;
    std::vector<float> m_outL;
    std::vector<float> m_outR;
};

const QStringList kDiscoverySearchPaths = {
    "/usr/lib/carla/carla-discovery-native", "/usr/local/lib/carla/carla-discovery-native", "/usr/lib64/carla/carla-discovery-native", "/usr/bin/carla-discovery-native", "/usr/local/bin/carla-discovery-native",
};

struct FormatConfig {
    QString type;
    QString format;
    QStringList envVars;
    QStringList defaultPaths;
    QString fileFilter;
    bool bundleDir;
};

const QList<FormatConfig> kFormats = {{"ladspa", "LADSPA", {"LADSPA_PATH"}, {"/usr/lib/ladspa", "/usr/local/lib/ladspa"}, "*.so", false},
                                      {"lv2", "LV2", {"LV2_PATH"}, {"/usr/lib/lv2", "/usr/local/lib/lv2"}, "*.lv2", true},
                                      {"vst2", "VST2", {"VST_PATH"}, {"/usr/lib/vst", "/usr/lib/vst2", "/usr/local/lib/vst", "/usr/local/lib/vst2"}, "*.so", false},
                                      {"vst3", "VST3", {"VST3_PATH"}, {"/usr/lib/vst3", "/usr/local/lib/vst3"}, "*.vst3", true},
                                      {"clap", "CLAP", {"CLAP_PATH"}, {"/usr/lib/clap", "/usr/local/lib/clap"}, "*.clap", false},
                                      {"dssi", "DSSI", {"DSSI_PATH"}, {"/usr/lib/dssi", "/usr/local/lib/dssi"}, "*.so", false},
                                      {"sf2", "SF2", {"SF2_PATH"}, {"/usr/share/soundfonts", "/usr/share/sounds/sf2"}, "*.sf2", false},
                                      {"sfz", "SFZ", {"SFZ_PATH"}, {"/usr/share/sounds/sfz"}, "*.sfz", false},
                                      {"jsfx", "JSFX", {"JSFX_PATH"}, {"/opt/REAPER/Plugins/FX", "~/.config/REAPER/Effects"}, "*.jsfx", false}};

QString toCategoryStr(int cat) {
    switch (static_cast<CarlaBackend::PluginCategory>(cat)) {
    case CarlaBackend::PLUGIN_CATEGORY_SYNTH:
        return "Synth";
    case CarlaBackend::PLUGIN_CATEGORY_DELAY:
        return "Delay";
    case CarlaBackend::PLUGIN_CATEGORY_EQ:
        return "EQ";
    case CarlaBackend::PLUGIN_CATEGORY_FILTER:
        return "Filter";
    case CarlaBackend::PLUGIN_CATEGORY_DISTORTION:
        return "Distortion";
    case CarlaBackend::PLUGIN_CATEGORY_DYNAMICS:
        return "Dynamics";
    case CarlaBackend::PLUGIN_CATEGORY_MODULATOR:
        return "Modulator";
    case CarlaBackend::PLUGIN_CATEGORY_UTILITY:
        return "Utility";
    case CarlaBackend::PLUGIN_CATEGORY_OTHER:
        return "Other";
    default:
        return "Other";
    }
}

QString normalizeCategoryTitle(QString category) {
    category = category.trimmed();
    if (category.isEmpty())
        return "Other";
    const QString lower = category.toLower();
    if (lower == "synth" || lower == "instrument")
        return "Synth";
    if (lower == "delay" || lower == "reverb")
        return "Delay";
    if (lower == "eq")
        return "EQ";
    if (lower == "filter")
        return "Filter";
    if (lower == "distortion")
        return "Distortion";
    if (lower == "dynamics")
        return "Dynamics";
    if (lower == "modulator" || lower == "modulation")
        return "Modulator";
    if (lower == "utility" || lower == "tools" || lower == "tool")
        return "Utility";
    if (lower == "other" || lower == "unknown" || lower == "misc" || lower == "none" || lower == "null")
        return "Other";
    return category;
}

QString normalizePluginName(QString name, const QString &label, const QString &filePath) {
    name = name.trimmed();
    if (!name.isEmpty())
        return name;
    const QString l = label.trimmed();
    if (!l.isEmpty())
        return l;
    return QFileInfo(filePath).completeBaseName().trimmed();
}

QString normalizePluginLabel(QString label, const QString &name) {
    label = label.trimmed();
    return label.isEmpty() ? name.trimmed() : label;
}

int categoryRank(const QString &category) {
    const QString c = normalizeCategoryTitle(category);
    if (c == "Filter")
        return 0;
    if (c == "EQ")
        return 1;
    if (c == "Dynamics")
        return 2;
    if (c == "Delay")
        return 3;
    if (c == "Distortion")
        return 4;
    if (c == "Modulator")
        return 5;
    if (c == "Utility")
        return 6;
    if (c == "Synth")
        return 7;
    return 100;
}

QList<PluginInfo> parseDiscoveryOutput(const QString &output, const QString &format, const QString &filePath) {
    QList<PluginInfo> results;
    PluginInfo current;
    bool inBlock = false;

    for (const QString &rawLine : output.split('\n')) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith("carla-discovery::"))
            continue;
        const QStringList parts = line.split("::");
        if (parts.size() < 2)
            continue;
        const QString &key = parts.at(1);
        const QString val = parts.size() >= 3 ? parts.mid(2).join("::") : "";

        if (key == "begin" || key == "init") {
            current = PluginInfo{};
            current.format = format;
            current.path = filePath;
            inBlock = true;
        } else if (!inBlock) {
            continue;
        } else if (key == "name") {
            current.name = val;
        } else if (key == "label") {
            current.label = val;
        } else if (key == "maker") {
            current.maker = val;
        } else if (key == "uniqueId") {
            current.uniqueId = val.toLongLong();
        } else if (key == "category") {
            // 旧APIは整数、新APIは文字列("none","filter"等)で返す
            bool catIsInt = false;
            const int catInt = val.toInt(&catIsInt);
            current.category = (catIsInt && val.trimmed() != "0") ? toCategoryStr(catInt) : normalizeCategoryTitle(val);
        } else if (key == "audio.ins") {
            current.audioIns = val.toInt();
        } else if (key == "audio.outs") {
            current.audioOuts = val.toInt();
        } else if (key == "end") {
            current.name = normalizePluginName(current.name, current.label, filePath);
            current.label = normalizePluginLabel(current.label, current.name);
            current.category = normalizeCategoryTitle(current.category);
            if (!current.name.isEmpty()) {
                current.id = QString("%1:%2:%3").arg(current.format, current.label, QString::number(current.uniqueId));
                results.append(current);
            }
            inBlock = false;
        }
    }
    return results;
}

// 1ファイルに対してディスカバリを実行
// stdout の逐次読み出しでバッファ詰まりデッドロックを回避
// stdin を /dev/null に向けて子プロセスによる端末状態の汚染を防ぐ
QList<PluginInfo> runDiscovery(const QString &tool, const QString &type, const QString &format, const QString &target, std::atomic<bool> &stopFlag) {
    if (stopFlag)
        return {};

    QProcess proc;
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(tool, {type, target});

    if (!proc.waitForStarted(Rina::Core::SettingsManager::instance().value("pluginDiscoveryWaitStartedMs", 3000).toInt())) {
        qWarning() << "[Discovery] 起動失敗:" << target;
        return {};
    }

    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    const int kTimeoutMs = Rina::Core::SettingsManager::instance().value("pluginDiscoveryTimeoutMs", 5000).toInt();

    while (!stopFlag) {
        proc.waitForReadyRead(Rina::Core::SettingsManager::instance().value("pluginDiscoveryWaitReadyReadMs", 200).toInt());
        output += proc.readAllStandardOutput();
        if (proc.state() == QProcess::NotRunning)
            break;
        if (timer.elapsed() > kTimeoutMs) {
            qWarning() << "[Discovery] タイムアウト:" << target;
            proc.kill();
            proc.waitForFinished(Rina::Core::SettingsManager::instance().value("pluginDiscoveryWaitFinishedMs", 1000).toInt());
            break;
        }
    }
    output += proc.readAllStandardOutput();

    QByteArray errOutput = proc.readAllStandardError();
    if (output.isEmpty() && errOutput.contains("carla-discovery::")) {
        output = errOutput;
    } else if (!errOutput.isEmpty() && output.isEmpty()) {
        qDebug() << "[Discovery] エラー出力:" << target << errOutput.left(200).trimmed();
    }
    return parseDiscoveryOutput(QString::fromUtf8(output), format, target);
}

QStringList collectSearchPaths(const FormatConfig &cfg) {
    QStringList paths;
    for (const QString &envKey : cfg.envVars) {
        const QByteArray val = qgetenv(envKey.toUtf8().constData());
        if (!val.isEmpty())
            paths << QString::fromLocal8Bit(val).split(':');
    }
    paths << (QDir::homePath() + "/." + cfg.type);
    paths << cfg.defaultPaths;
    return paths;
}

bool isDiscoveryTypeSupported(const QString &tool, const QString &type) {
    QProcess probe;
    probe.setStandardInputFile(QProcess::nullDevice());
    probe.setProcessChannelMode(QProcess::SeparateChannels);
    probe.start(tool, {type, "__probe__"});
    if (!probe.waitForFinished(3000))
        probe.kill();
    return !probe.readAllStandardError().contains("invalid string type");
}

QList<PluginInfo> discoverFormat(const QString &tool, const FormatConfig &cfg, std::atomic<bool> &stopFlag) {
    if (!isDiscoveryTypeSupported(tool, cfg.type)) {
        qWarning() << "[AudioPluginManager]" << cfg.format << "はインストール済みCarlaバージョンで未対応のためスキップ";
        return {};
    }
    QStringList targets;

    if (cfg.type == "lv2") {
        QStringList lv2SearchPaths;
        const QByteArray lv2PathEnv = qgetenv("LV2_PATH");
        if (!lv2PathEnv.isEmpty())
            lv2SearchPaths << QString::fromLocal8Bit(lv2PathEnv).split(':', Qt::SkipEmptyParts);
        lv2SearchPaths << QDir::homePath() + "/.lv2" << "/usr/lib/lv2" << "/usr/local/lib/lv2";
        QSet<QString> visited;
        for (const QString &searchPath : lv2SearchPaths) {
            QDir dir(searchPath);
            if (!dir.exists())
                continue;
            for (const QFileInfo &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QString bundlePath = fi.absoluteFilePath();
                if (fi.suffix() == "lv2" && !visited.contains(bundlePath)) {
                    visited.insert(bundlePath);
                    targets.append(bundlePath);
                }
            }
        }
        qDebug() << "[AudioPluginManager]" << cfg.format << "バンドル" << targets.size() << "個を検出";
    } else {
        QStringList searchPaths = collectSearchPaths(cfg);
        QStringList customPaths = Rina::Core::SettingsManager::instance().value("pluginPaths" + cfg.format, QStringList()).toStringList();
        for (const QString &cp : customPaths) {
            if (!cp.trimmed().isEmpty() && !searchPaths.contains(cp)) {
                searchPaths.append(cp.trimmed());
            }
        }
        QSet<QString> visited;

        for (const QString &dirPath : searchPaths) {
            if (stopFlag)
                break;
            QDir d(dirPath);
            if (!d.exists())
                continue;

            const QString canonical = d.canonicalPath();
            if (visited.contains(canonical))
                continue;
            visited.insert(canonical);

            if (cfg.bundleDir) {
                const QFileInfoList entries = d.entryInfoList({cfg.fileFilter}, QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &fi : entries) {
                    targets << fi.absoluteFilePath();
                }
            } else {
                QDirIterator it(dirPath, {cfg.fileFilter}, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    targets << it.next();
                }
            }
        }
        qDebug() << "[AudioPluginManager]" << cfg.format << "ファイル" << targets.size() << "個を検出";
    }

    QList<PluginInfo> all;
    QMutex mutex;

    QThreadPool pool;
    pool.setMaxThreadCount(Rina::Core::SettingsManager::instance().value("pluginDiscoveryThreads", std::max(2, QThread::idealThreadCount() - 1)).toInt());

    QtConcurrent::blockingMap(&pool, targets, [&](const QString &target) {
        if (stopFlag)
            return;
        QList<PluginInfo> res = runDiscovery(tool, cfg.type, cfg.format, target, stopFlag);
        if (!res.isEmpty()) {
            QMutexLocker lock(&mutex);
            all += res;
        }
    });

    return all;
}

} // namespace

AudioPluginManager &AudioPluginManager::instance() {
    static AudioPluginManager inst;
    return inst;
}

AudioPluginManager::AudioPluginManager() {}

AudioPluginManager::~AudioPluginManager() { stopScan(); }

void AudioPluginManager::stopScan() { m_stopRequested = true; }

void AudioPluginManager::initialize() {
    if (m_initialized)
        return;
    m_initialized = true;

    auto future = QtConcurrent::run([this] {
        scanPlugins();
        emit pluginsReady(m_plugins.size());
    });
    future.waitForFinished();
}

void AudioPluginManager::scanPlugins() {
    bool expected = false;
    if (!m_scanning.compare_exchange_strong(expected, true)) {
        qDebug() << "[AudioPluginManager] スキャンは既に実行中";
        return;
    }
    m_stopRequested = false;

    QString tool;
    for (const QString &p : kDiscoverySearchPaths) {
        if (QFile::exists(p)) {
            tool = p;
            break;
        }
    }
    if (tool.isEmpty())
        tool = QStandardPaths::findExecutable("carla-discovery-native");
    if (tool.isEmpty()) {
        qWarning() << "[AudioPluginManager] carla-discovery-native が見つかりません";
        qWarning() << "[AudioPluginManager] 検索パス:" << kDiscoverySearchPaths;
        m_scanning = false;
        return;
    }
    qDebug() << "[AudioPluginManager] ディスカバリツール:" << tool;

    QList<PluginInfo> newPlugins;
    QHash<QString, PluginInfo> newMap;

    for (const FormatConfig &cfg : kFormats) {
        if (m_stopRequested)
            break;
        bool isEnabled = Rina::Core::SettingsManager::instance().value("pluginEnable" + cfg.format, true).toBool();
        if (!isEnabled)
            continue;

        qDebug() << "[AudioPluginManager] スキャン中:" << cfg.format;
        const QList<PluginInfo> found = discoverFormat(tool, cfg, m_stopRequested);
        qDebug() << "[AudioPluginManager]" << cfg.format << "→" << found.size() << "個";
        for (const PluginInfo &p : found) {
            if (!newMap.contains(p.id)) {
                newPlugins.append(p);
                newMap.insert(p.id, p);
            }
        }
    }

    {
        QMutexLocker lock(&m_pluginsMutex);
        m_plugins = std::move(newPlugins);
        m_pluginMap = std::move(newMap);
    }
    qDebug() << "[AudioPluginManager] 検出プラグイン数:" << m_plugins.size();
    m_scanning = false;
}

QVariantList AudioPluginManager::getPluginList() const {
    QMutexLocker lock(&m_pluginsMutex);
    QVariantList list;
    list.reserve(m_plugins.size());
    for (const auto &info : m_plugins) {
        QVariantMap map;
        map["id"] = info.id;
        map["name"] = info.name;
        map["format"] = info.format;
        map["category"] = info.category;
        map["maker"] = info.maker;
        map["audioIns"] = info.audioIns;
        map["audioOuts"] = info.audioOuts;
        list.append(map);
    }
    return list;
}

QVariantList AudioPluginManager::getCategories() const {
    QMutexLocker lock(&m_pluginsMutex);
    QStringList cats;
    for (const auto &info : m_plugins) {
        const QString c = normalizeCategoryTitle(info.category);
        if (!cats.contains(c))
            cats.append(c);
    }
    std::sort(cats.begin(), cats.end(), [](const QString &a, const QString &b) {
        const int ra = categoryRank(a), rb = categoryRank(b);
        return ra != rb ? ra < rb : a.toLower() < b.toLower();
    });
    QVariantList list;
    for (const auto &c : cats)
        list.append(c);
    return list;
}

QVariantList AudioPluginManager::getPluginsInCategory(const QString &category) const {
    QMutexLocker lock(&m_pluginsMutex);
    const QString wanted = normalizeCategoryTitle(category);
    QList<PluginInfo> matched;
    for (const auto &info : m_plugins) {
        if (normalizeCategoryTitle(info.category) == wanted)
            matched.append(info);
    }
    std::sort(matched.begin(), matched.end(), [](const PluginInfo &a, const PluginInfo &b) { return a.name.toLower() < b.name.toLower(); });
    QVariantList list;
    for (const auto &info : matched) {
        QVariantMap map;
        map["id"] = info.id;
        map["name"] = normalizePluginName(info.name, info.label, info.path);
        map["format"] = info.format;
        map["maker"] = info.maker;
        map["category"] = normalizeCategoryTitle(info.category);
        list.append(map);
    }
    return list;
}

std::unique_ptr<IAudioPlugin> AudioPluginManager::createPlugin(const QString &id) {
    QMutexLocker lock(&m_pluginsMutex);
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(), [&](const PluginInfo &info) { return info.id == id; });

    if (it == m_plugins.end()) {
        qWarning() << "[AudioPluginManager] Plugin not found:" << id;
        return nullptr;
    }

    auto plugin = std::make_unique<CarlaHostedPlugin>(*it);
    if (!plugin->load(it->path, it->index)) {
        qWarning() << "[AudioPluginManager] Failed to load plugin:" << it->name << it->path;
        return nullptr;
    }

    qDebug() << "[AudioPluginManager] Loaded plugin via independent Carla instance:" << it->name << it->format << it->path;
    return plugin;
}

} // namespace Rina::Engine::Plugin
