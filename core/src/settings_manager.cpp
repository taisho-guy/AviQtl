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

SettingsManager &SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
    // Default settings
    m_settings = {{"maxImageSize", "1920x1080"}, {"cacheSize", 4096},   {"undoCount", 32},     {"renderThreads", 0}, {"theme", "Dark"},       {"showConfirmOnClose", true},
                  {"enableAutoBackup", true},    {"backupInterval", 5}, {"timeUnit", "frame"}, {"enableSnap", true}, {"splitAtCursor", true}, {"showLayerRange", true}};
    load();
}

QString SettingsManager::getSettingsFilePath() const {
    // 1. Try executable directory (Portable mode)
    QString exeDir = QCoreApplication::applicationDirPath();
    QString portablePath = exeDir + "/rina_settings.json";

    // Check if writable
    QFile file(portablePath);
    if (file.exists()) {
        if (!file.permissions().testFlag(QFile::WriteUser)) {
            qWarning() << "Portable settings file found but not writable. Falling back.";
        } else {
            return portablePath;
        }
    } else {
        // If it doesn't exist, check directory permissions
        QFileInfo dirInfo(exeDir);
        if (dirInfo.isWritable()) {
            return portablePath;
        }
    }

    // 2. Fallback to AppLocalDataLocation
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/settings.json";
}

void SettingsManager::setSettings(const QVariantMap &settings) {
    if (m_settings != settings) {
        m_settings = settings;
        emit settingsChanged();
        save(); // Auto-save on change
    }
}

void SettingsManager::load() {
    QString path = getSettingsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No settings file found at" << path << ", using defaults.";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
        QVariantMap loaded = doc.object().toVariantMap();
        // Merge with defaults (preserve unknown keys, overwrite defaults)
        for (auto it = loaded.begin(); it != loaded.end(); ++it) {
            m_settings[it.key()] = it.value();
        }
        emit settingsChanged();
        qDebug() << "Settings loaded from" << path;
    }
}

void SettingsManager::save() {
    QString path = getSettingsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save settings to" << path;
        return;
    }

    QJsonObject obj = QJsonObject::fromVariantMap(m_settings);
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    qDebug() << "Settings saved to" << path;
}

} // namespace Rina::Core