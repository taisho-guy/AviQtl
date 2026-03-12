#pragma once
#include "audio_plugin_host.hpp"
#include <CarlaHost.h>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QVariantList>
#include <atomic>
#include <memory>

namespace Rina::Engine::Plugin {

struct PluginInfo {
    QString id;
    QString name;
    QString format;
    QString category;
    QString path;
    QString label;
    QString maker;
    int64_t uniqueId = 0;
    int index = 0;
    int audioIns = 0;
    int audioOuts = 0;
};

class AudioPluginManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(AudioPluginManager)
  public:
    static AudioPluginManager &instance();
    ~AudioPluginManager();

    void initialize();
    void scanPlugins();
    void stopScan();

    Q_INVOKABLE QVariantList getPluginList() const;
    Q_INVOKABLE QVariantList getCategories() const;
    Q_INVOKABLE QVariantList getPluginsInCategory(const QString &category) const;

    std::unique_ptr<IAudioPlugin> createPlugin(const QString &id);

  Q_SIGNALS:
    void pluginsReady(int count);

  private:
    AudioPluginManager();
    CarlaHostHandle m_carlaHost = nullptr;
    bool m_initialized = false;
    std::atomic<bool> m_scanning{false};
    std::atomic<bool> m_stopRequested{false};
    mutable QMutex m_pluginsMutex;
    QList<PluginInfo> m_plugins;
    QHash<QString, PluginInfo> m_pluginMap;
};

} // namespace Rina::Engine::Plugin
