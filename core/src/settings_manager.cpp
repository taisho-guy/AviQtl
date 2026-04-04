#include "settings_manager.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace Rina::Core {

auto SettingsManager::instance() -> SettingsManager & {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
    m_settings = {{"pluginEnableLADSPA", true},
                  {"pluginPathsLADSPA", QStringList()},
                  {"pluginEnableDSSI", true},
                  {"pluginPathsDSSI", QStringList()},
                  {"pluginEnableLV2", true},
                  {"pluginPathsLV2", QStringList()},
                  {"pluginEnableVST2", true},
                  {"pluginPathsVST2", QStringList()},
                  {"pluginEnableVST3", true},
                  {"pluginPathsVST3", QStringList()},
                  {"pluginEnableCLAP", true},
                  {"pluginPathsCLAP", QStringList()},
                  {"pluginEnableSF2", true},
                  {"pluginPathsSF2", QStringList()},
                  {"pluginEnableSFZ", true},
                  {"pluginPathsSFZ", QStringList()},
                  {"pluginEnableJSFX", true},
                  {"pluginPathsJSFX", QStringList()},
                  {"maxImageSize", "1920x1080"},
                  {"cacheSize", 512},
                  {"undoCount", 32},
                  {"renderThreads", 0},
                  {"theme", "Dark"},
                  {"showConfirmOnClose", true},
                  {"enableAutoBackup", true},
                  {"backupInterval", 5},
                  {"defaultProjectWidth", 1920},
                  {"defaultProjectHeight", 1080},
                  {"defaultProjectFps", 60.0},
                  {"defaultProjectFrames", 3600},
                  {"defaultProjectSampleRate", 48000},
                  {"defaultClipDuration", 100},
                  {"timeUnit", "frame"},
                  {"enableSnap", true},
                  {"splitAtCursor", true},
                  {"showLayerRange", true},
                  {"timelineTrackHeight", 30},
                  {"timelineHeaderHeight", 28},
                  {"timelineRulerHeight", 32},
                  {"timelineMaxLayers", 128},
                  {"timelineLayerHeaderWidth", 60},
                  {"timelineRulerTimeWidth", 70},
                  {"timelineClipResizeHandleWidth", 10},
                  {"splashDuration", 1000},
                  {"splashSize", 512},
                  {"appStartupDelay", 1000},
                  {"exportImageQuality", 95},
                  {"exportSequencePadding", 6},
                  {"minClipDurationFrames", 5},
                  {"magneticSnapRange", 10},
                  {"timelineZoomMin", 10},
                  {"timelineZoomMax", 400},
                  {"timelineZoomStep", 10},
                  {"videoDecoderIndexReserve", 108000},
                  {"videoDecoderMinCacheMB", 64},
                  {"hwFramePoolSize", 32},
                  {"exportDefaultCodec", "h264_vaapi"},
                  {"exportDefaultBitrateMbps", 15},
                  {"exportDefaultCrf", 20},
                  {"exportDefaultAudioCodec", "aac"},
                  {"exportDefaultAudioBitrateKbps", 192},
                  {"exportFrameGrabTimeoutMs", 2000},
                  {"exportProgressInterval", 5},
                  {"audioChannels", 2},
                  {"audioPluginMaxBlockSize", 4096},
                  {"sceneWidthMax", 8000},
                  {"sceneHeightMax", 8000},
                  {"sceneFramesMin", 100},
                  {"sceneFramesMax", 24000},
                  {"sceneFramesStep", 100},
                  {"recentProjectMaxCount", 10},
                  {"luaHookIntervalMs", 16},
                  {"textPaddingMultiplier", 4.0},
                  {"shortcuts", defaultShortcutSettings()}};
    load();
}

auto SettingsManager::defaultShortcutSettings() -> QVariantMap {
    return {// Project
            {"project.new", "Ctrl+N"},
            {"project.save", "Ctrl+S"},
            {"project.open", "Ctrl+O"},
            {"project.saveAs", "Ctrl+Shift+S"},
            {"app.quit", "Ctrl+Q"},

            // Edit
            {"edit.undo", "Ctrl+Z"},
            {"edit.redo", "Ctrl+Shift+Z"},
            {"edit.cut", "Ctrl+X"},
            {"edit.copy", "Ctrl+C"},
            {"edit.paste", "Ctrl+V"},
            {"edit.delete", "Delete"},
            {"edit.duplicate", "Ctrl+D"},

            // Transport
            {"transport.playPause", "Space"},
            {"transport.nextFrame", "Right"},
            {"transport.prevFrame", "Left"},
            {"transport.jumpStart", "Home"},
            {"transport.jumpEnd", "End"},

            // View
            {"view.zoomIn", "Ctrl++"},
            {"view.zoomOut", "Ctrl+-"},

            // Timeline
            {"timeline.split", "S"},
            {"timeline.moveUp", "Alt+Up"},
            {"timeline.moveDown", "Alt+Down"},
            {"timeline.nudgeLeft", "Alt+Left"},
            {"timeline.nudgeRight", "Alt+Right"}};
}

auto SettingsManager::getSettingsFilePath() -> QString {
    // 1. 実行ファイルディレクトリを試す (ポータブルモード)
    QString exeDir = QCoreApplication::applicationDirPath();
    QString portablePath = exeDir + "/rina_settings.json";

    // 書き込み可能かチェック
    QFile file(portablePath);
    if (file.exists()) {
        if (!file.permissions().testFlag(QFile::WriteUser)) {
            qWarning() << "ポータブル設定ファイルが見つかりましたが、書き込み不可です。フォールバックします。";
        } else {
            return portablePath;
        }
    } else {
        // 存在しない場合は、ディレクトリの権限をチェック
        QFileInfo dirInfo(exeDir);
        if (dirInfo.isWritable()) {
            return portablePath;
        }
    }

    // 2. AppLocalDataLocationにフォールバック
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/settings.json";
}

void SettingsManager::setSettings(const QVariantMap &settings) {
    if (m_settings != settings) {
        m_settings = settings;
        emit settingsChanged();
        save(); // 変更時に自動保存
    }
}

void SettingsManager::load() {
    QString path = getSettingsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "設定ファイルが見つかりません:" << path << "。デフォルト値を使用します。";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
        QVariantMap loaded = doc.object().toVariantMap();
        for (auto it = loaded.begin(); it != loaded.end(); ++it) {
            if (it.key() == "shortcuts" && it.value().canConvert<QVariantMap>()) {
                QVariantMap mergedShortcuts = m_settings.value(QStringLiteral("shortcuts")).toMap();
                QVariantMap loadedShortcuts = it.value().toMap();
                for (auto shortcutIt = loadedShortcuts.begin(); shortcutIt != loadedShortcuts.end(); ++shortcutIt) {
                    mergedShortcuts[shortcutIt.key()] = shortcutIt.value();
                }
                m_settings[it.key()] = mergedShortcuts;
                continue;
            }
            m_settings[it.key()] = it.value();
        }
        emit settingsChanged();
        qDebug() << "設定をロードしました:" << path;
    }
}

void SettingsManager::save() {
    QString path = getSettingsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "設定の保存に失敗しました:" << path;
        return;
    }

    QJsonObject obj = QJsonObject::fromVariantMap(m_settings);
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    qDebug() << "設定を保存しました:" << path;
}

void SettingsManager::setValue(const QString &key, const QVariant &value) {
    if (m_settings.value(key) != value) {
        m_settings[key] = value;
        emit settingsChanged();
        // Runtime keys starting with "_" are not saved to disk
        if (!key.startsWith(QStringLiteral("_"))) {
            save();
        }
    }
}

auto SettingsManager::value(const QString &key, const QVariant &defaultValue) const -> QVariant { return m_settings.value(key, defaultValue); }

auto SettingsManager::shortcuts() const -> QVariantMap { return m_settings.value(QStringLiteral("shortcuts")).toMap(); }

auto SettingsManager::shortcut(const QString &actionId, const QString &fallbackValue) const -> QString {
    const QVariantMap shortcutMap = shortcuts();
    const QString value = shortcutMap.value(actionId, fallbackValue).toString();
    return value.isEmpty() ? fallbackValue : value;
}

} // namespace Rina::Core