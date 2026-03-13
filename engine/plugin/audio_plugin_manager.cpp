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

#include <CarlaBackend.h>
#include <CarlaHost.h>

namespace Rina::Engine::Plugin {

namespace {

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

AudioPluginManager::~AudioPluginManager() {
    stopScan();
    if (m_carlaHost != nullptr) {
        carla_engine_close(m_carlaHost);
        m_carlaHost = nullptr;
    }
}

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
    Q_UNUSED(id)
    qWarning() << "[AudioPluginManager] createPlugin は次段で carla_add_plugin に接続します";
    return nullptr;
}

} // namespace Rina::Engine::Plugin
