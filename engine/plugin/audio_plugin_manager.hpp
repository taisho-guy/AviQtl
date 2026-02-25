#pragma once
#include "audio_plugin_host.hpp"
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <memory>

// Forward declaration
typedef struct LilvWorldImpl LilvWorld;

namespace Rina::Engine::Plugin {

struct PluginInfo {
    QString id; // 一意の識別子 (例: "ladspa:/usr/lib/ladspa/plugin.so:0")
    QString name;
    QString format;   // "LADSPA", "LV2", "CLAP", "VST3"
    QString category; // Added
    QString path;
    QString uri;   // LV2用
    int index = 0; // LADSPA/CLAP用
};

class AudioPluginManager : public QObject {
    Q_OBJECT
  public:
    static AudioPluginManager &instance();
    ~AudioPluginManager();

    // システム上のプラグインをスキャン
    void scanPlugins();

    // QML向けにプラグインリストを返す (name, id, format)
    Q_INVOKABLE QVariantList getPluginList() const;

    // カテゴリ別プラグインリスト (Added)
    Q_INVOKABLE QVariantList getCategories() const;
    Q_INVOKABLE QVariantList getPluginsInCategory(const QString &category) const;

    // IDからプラグインインスタンスを生成
    std::unique_ptr<IAudioPlugin> createPlugin(const QString &id);

    LilvWorld *getLilvWorld() const { return m_lilvWorld; }

  private:
    AudioPluginManager();
    QList<PluginInfo> m_plugins;
    QHash<QString, PluginInfo> m_pluginMap; // ID -> Info

    void registerPlugin(const PluginInfo &info);
    void scanLadspa();
    void scanLv2();
    void scanClap();
    void scanVst3();

    LilvWorld *m_lilvWorld = nullptr;
};

} // namespace Rina::Engine::Plugin