#include "audio_plugin_manager.hpp"
#include "clap_plugin.hpp"
#include "ladspa_plugin.hpp"
#include "lv2_plugin.hpp"
#include "vst3_plugin.hpp"
#include <QDebug>
#include <QDirIterator>
#include <QLibrary>
#include <QSet>
#include <algorithm> // for std::sort
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#endif
#include <lilv/lilv.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

// LADSPAヘッダー定義 (スキャン用)
typedef const LADSPA_Descriptor *(*LADSPA_Descriptor_Function)(unsigned long Index);

// CLAPヘッダー定義 (スキャン用)
#include <clap/clap.h>

namespace Rina::Engine::Plugin {

AudioPluginManager &AudioPluginManager::instance() {
    static AudioPluginManager inst;
    return inst;
}

AudioPluginManager::AudioPluginManager() { scanPlugins(); }

void AudioPluginManager::registerPlugin(const PluginInfo &info) {
    m_plugins.append(info);
    m_pluginMap.insert(info.id, info);
}

QVariantList AudioPluginManager::getPluginList() const {
    QVariantList list;
    for (const auto &p : m_plugins) {
        QVariantMap m;
        m["id"] = p.id;
        m["name"] = p.name;
        m["format"] = p.format;
        list.append(m);
    }
    return list;
}

QVariantList AudioPluginManager::getCategories() const {
    QSet<QString> categories;
    for (const auto &plugin : m_plugins) {
        if (!plugin.category.isEmpty()) {
            categories.insert(plugin.category);
        } else {
            categories.insert(plugin.format);
        }
    }

    QStringList sortedCategories = categories.values();
    std::sort(sortedCategories.begin(), sortedCategories.end());

    QVariantList list;
    for (const auto &cat : sortedCategories) {
        list.append(cat);
    }
    return list;
}

QVariantList AudioPluginManager::getPluginsInCategory(const QString &category) const {
    QVariantList list;
    for (const auto &plugin : m_plugins) {
        if (plugin.category == category || (plugin.category.isEmpty() && plugin.format == category)) {
            list.append(QVariantMap{{"id", plugin.id}, {"name", plugin.name}});
        }
    }
    return list;
}

std::unique_ptr<IAudioPlugin> AudioPluginManager::createPlugin(const QString &id) {
    if (!m_pluginMap.contains(id))
        return nullptr;

    const auto &info = m_pluginMap[id];
    std::unique_ptr<IAudioPlugin> plugin;

    if (info.format == "LADSPA") {
        plugin = std::make_unique<LadspaPlugin>();
        plugin->load(info.path, info.index);
    } else if (info.format == "LV2") {
        plugin = std::make_unique<Lv2Plugin>();
        plugin->load(info.uri);
    } else if (info.format == "CLAP") {
        plugin = std::make_unique<ClapPlugin>();
        plugin->load(info.path, info.index);
    } else if (info.format == "VST3") {
#ifdef RINA_USE_VST3
        plugin = std::make_unique<Vst3Plugin>();
        plugin->load(info.path);
#endif
    }

    return plugin;
}

void AudioPluginManager::scanPlugins() {
    m_plugins.clear();
    m_pluginMap.clear();
    qInfo() << "[AudioPluginManager] Scanning plugins...";

    scanLv2();

    scanVst3();

    qInfo() << "[AudioPluginManager] Scan complete. Found" << m_plugins.size() << "plugins.";
}

void AudioPluginManager::scanLv2() {
    LilvWorld *world = lilv_world_new();
    lilv_world_load_all(world);
    const LilvPlugins *plugins = lilv_world_get_all_plugins(world);

    LILV_FOREACH(plugins, i, plugins) {
        const LilvPlugin *p = lilv_plugins_get(plugins, i);
        const LilvNode *n = lilv_plugin_get_name(p);
        const LilvNode *u = lilv_plugin_get_uri(p);

        // Get Category using standard Lilv API
        const LilvPluginClass *cls = lilv_plugin_get_class(p);
        const LilvNode *cat_node = cls ? lilv_plugin_class_get_label(cls) : nullptr;

        PluginInfo info;
        if (n)
            info.name = QString::fromUtf8(lilv_node_as_string(n));
        if (u) {
            info.uri = QString::fromUtf8(lilv_node_as_uri(u));
            info.id = "lv2:" + info.uri;
        }
        info.format = "LV2";

        if (cat_node) {
            info.category = QString::fromUtf8(lilv_node_as_string(cat_node));
        } else {
            info.category = "LV2";
        }

        if (!info.id.isEmpty()) {
            registerPlugin(info);
        }
    }
    lilv_world_free(world);
}

void AudioPluginManager::scanClap() {
    QStringList paths = {"/usr/lib/clap", QDir::homePath() + "/.clap"};
    if (qEnvironmentVariableIsSet("CLAP_PATH")) {
        paths = QString::fromLocal8Bit(qgetenv("CLAP_PATH")).split(':');
    }

    for (const auto &path : paths) {
        QDir dir(path);
        if (!dir.exists())
            continue;

        QDirIterator it(path, {"*.clap"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QLibrary lib(filePath);
            if (!lib.load())
                continue;

            auto entry = (const clap_plugin_entry_t *)lib.resolve("clap_entry");
            if (!entry)
                continue;

            entry->init(filePath.toUtf8().constData());
            auto factory = (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
            if (factory) {
                uint32_t count = factory->get_plugin_count(factory);
                for (uint32_t i = 0; i < count; ++i) {
                    auto desc = factory->get_plugin_descriptor(factory, i);
                    if (!desc)
                        continue;

                    PluginInfo info;
                    info.name = desc->name;
                    info.path = filePath;
                    info.index = (int)i;
                    info.format = "CLAP";
                    info.id = QString("clap:%1:%2").arg(info.path).arg(i);

                    // Simple category heuristic for CLAP
                    if (info.name.startsWith("Cardinal")) {
                        info.category = "Cardinal";
                    } else {
                        info.category = "CLAP";
                    }

                    registerPlugin(info);
                }
            }
        }
    }
}

void AudioPluginManager::scanVst3() {
#ifdef RINA_USE_VST3
    QStringList paths = {"/usr/lib/vst3", "/usr/local/lib/vst3", QDir::homePath() + "/.vst3"};
    if (qEnvironmentVariableIsSet("VST3_PATH")) {
        paths = QString::fromLocal8Bit(qgetenv("VST3_PATH")).split(':');
    }

    for (const auto &path : paths) {
        QDir dir(path);
        if (!dir.exists())
            continue;

        // VST3は通常ディレクトリ（バンドル）として配布される
        QDirIterator it(path, {"*.vst3"}, QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString bundlePath = it.next();
            QFileInfo fi(bundlePath);

            // Linux VST3 Bundle構造: Plugin.vst3/Contents/x86_64-linux/Plugin.so
            QString binaryPath = bundlePath + "/Contents/x86_64-linux/" + fi.baseName() + ".so";

            if (QFile::exists(binaryPath)) {
                PluginInfo info;
                info.name = fi.baseName(); // 仮の名前（ロード時に正式名取得）
                info.path = binaryPath;    // ローダーにはバイナリの実パスを渡す
                info.format = "VST3";
                info.id = "vst3:" + binaryPath;
                info.category = "VST3";
                registerPlugin(info);
            }
        }
    }
#else
    qInfo() << "[AudioPluginManager] VST3 support is disabled (SDK not found at build time).";
#endif
}

} // namespace Rina::Engine::Plugin