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

#include <CarlaBackend.h>
#include <CarlaHost.h>

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
            m_inL.resize(frames);
            m_inR.resize(frames);
            m_outL.resize(frames);
            m_outR.resize(frames);
        }
    }

    void deinterleave(const float *src, int frames) {
        ensureBuffers(frames);
        for (int i = 0; i < frames; ++i) {
            m_inL[i] = src[i * 2 + 0];
            m_inR[i] = src[i * 2 + 1];
            m_outL[i] = m_inL[i];
            m_outR[i] = m_inR[i];
        }
    }

    void interleave(float *dst, int frames) const {
        for (int i = 0; i < frames; ++i) {
            dst[i * 2 + 0] = m_outL[i];
            dst[i * 2 + 1] = m_outR[i];
        }
    }

    bool load(const QString &path, int index = 0) override {
        Q_UNUSED(index)

        // 1プラグインにつき1つの独立したCarlaHostを生成
        m_host = carla_standalone_host_init();
        if (m_host == nullptr) {
            qWarning() << "[CarlaHostedPlugin] carla_standalone_host_init failed for" << m_info.name;
            return false;
        }

        CarlaBackend::PluginType ptype = CarlaBackend::PLUGIN_NONE;
        if (!mapFormatToCarlaType(m_info.format, ptype)) {
            qWarning() << "[CarlaHostedPlugin] 未対応フォーマット:" << m_info.format << m_info.name;
            return false;
        }

        const QByteArray filename = path.toUtf8();
        const QByteArray name = m_info.name.toUtf8();
        const QByteArray label = m_info.label.toUtf8();

        m_loaded = carla_add_plugin(m_host, CarlaBackend::BINARY_POSIX64, ptype, filename.isEmpty() ? nullptr : filename.constData(), name.isEmpty() ? nullptr : name.constData(), label.isEmpty() ? nullptr : label.constData(), m_info.uniqueId, nullptr,
                                    CarlaBackend::PLUGIN_OPTIONS_NULL);

        if (!m_loaded) {
            qWarning() << "[CarlaHostedPlugin] carla_add_plugin failed:" << m_info.name << m_info.path;
            carla_engine_close(m_host);
            m_host = nullptr;
        }

        return m_loaded;
    }

    void prepare(double sampleRate, int maxBlockSize) override {
        m_sampleRate = sampleRate > 1.0 ? sampleRate : 48000.0;
        m_maxBlockSize = maxBlockSize > 0 ? maxBlockSize : 512;
        ensureBuffers(m_maxBlockSize);

        if (m_host == nullptr)
            return;

        carla_set_engine_option(m_host, CarlaBackend::ENGINE_OPTION_PROCESS_MODE, CarlaBackend::ENGINE_PROCESS_MODE_CONTINUOUS_RACK, nullptr);
        carla_set_engine_option(m_host, CarlaBackend::ENGINE_OPTION_FORCE_STEREO, 1, nullptr);
        carla_set_engine_option(m_host, CarlaBackend::ENGINE_OPTION_AUDIO_BUFFER_SIZE, m_maxBlockSize, nullptr);
        carla_set_engine_option(m_host, CarlaBackend::ENGINE_OPTION_AUDIO_SAMPLE_RATE, static_cast<int>(m_sampleRate), nullptr);
    }

    void process(float *buf, int frameCount) override {
        if (!m_loaded || m_host == nullptr || buf == nullptr || frameCount <= 0)
            return;

        deinterleave(buf, frameCount);

        // Stage 2 安全版:
        // ここではアダプタ層のみ確定実装する。
        // m_inL/m_inR -> Carla処理 -> m_outL/m_outR の実呼び出しは
        // 次段で Carla 側の process シグネチャ確定後に接続する。

        interleave(buf, frameCount);
    }

    bool active() const override { return m_loaded; }

    void release() override {
        m_loaded = false;
        if (m_host != nullptr) {
            carla_engine_close(m_host);
            m_host = nullptr;
        }
        m_inL.clear();
        m_inR.clear();
        m_outL.clear();
        m_outR.clear();
    }

    QString name() const override { return m_info.name; }
    QString format() const override { return m_info.format; }
    int paramCount() const override {
        if (!m_loaded || m_host == nullptr)
            return 0;
        return static_cast<int>(carla_get_parameter_count(m_host, m_pluginId));
    }

    QString paramName(int i) const override {
        if (!m_loaded || m_host == nullptr || i < 0)
            return {};
        const CarlaParameterInfo *info = carla_get_parameter_info(m_host, m_pluginId, static_cast<uint32_t>(i));
        if (info == nullptr)
            return {};
        return safeQString(info->name);
    }

    float getParam(int i) const override {
        if (!m_loaded || m_host == nullptr || i < 0)
            return 0.0f;
        return carla_get_current_parameter_value(m_host, m_pluginId, static_cast<uint32_t>(i));
    }

    void setParam(int i, float v) override {
        if (!m_loaded || m_host == nullptr || i < 0)
            return;
        carla_set_parameter_value(m_host, m_pluginId, static_cast<uint32_t>(i), clamp01(v));
    }

    ParamInfo getParamInfo(int i) const override {
        ParamInfo out;
        if (!m_loaded || m_host == nullptr || i < 0)
            return out;

        const uint32_t pid = static_cast<uint32_t>(i);
        const CarlaParameterInfo *info = carla_get_parameter_info(m_host, m_pluginId, pid);
        if (info != nullptr)
            out.name = safeQString(info->name);
        out.defaultValue = carla_get_default_parameter_value(m_host, m_pluginId, pid);
        out.min = 0.0f;
        out.max = 1.0f;
        return out;
    }

  private:
    CarlaHostHandle m_host = nullptr;
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
        return "Unknown";
    }
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

        if (key == "begin") {
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
            current.category = toCategoryStr(val.toInt());
        } else if (key == "audio.ins") {
            current.audioIns = val.toInt();
        } else if (key == "audio.outs") {
            current.audioOuts = val.toInt();
        } else if (key == "end") {
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
    if (!errOutput.isEmpty() && output.isEmpty() && !errOutput.contains("invalid string type")) {
        qDebug() << "[Discovery] エラー:" << target << errOutput.trimmed();
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

QList<PluginInfo> discoverFormat(const QString &tool, const FormatConfig &cfg, std::atomic<bool> &stopFlag) {
    QStringList targets;

    if (cfg.type == "lv2") {
        QProcess proc;
        proc.start("lv2ls", {});
        if (proc.waitForFinished()) {
            targets = QString::fromUtf8(proc.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
            qDebug() << "[AudioPluginManager]" << cfg.format << "URI" << targets.size() << "個を検出";
        } else {
            qWarning() << "[AudioPluginManager] lv2lsの実行に失敗しました";
        }
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

    (void)QtConcurrent::run([this] {
        scanPlugins();
        emit pluginsReady(m_plugins.size());
    });
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
    QSet<QString> cats;
    for (const auto &info : m_plugins)
        cats.insert(info.category);
    QVariantList list;
    for (const auto &c : cats)
        list.append(c);
    return list;
}

QVariantList AudioPluginManager::getPluginsInCategory(const QString &category) const {
    QMutexLocker lock(&m_pluginsMutex);
    QVariantList list;
    for (const auto &info : m_plugins) {
        if (info.category == category) {
            QVariantMap map;
            map["id"] = info.id;
            map["name"] = info.name;
            map["format"] = info.format;
            map["maker"] = info.maker;
            list.append(map);
        }
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
