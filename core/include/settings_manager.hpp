#pragma once
#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

namespace AviQtl::Core {

class SettingsManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings WRITE setSettings NOTIFY settingsChanged)

  public:
    static SettingsManager &instance();

    QVariantMap settings() const { return m_settings; }
    void setSettings(const QVariantMap &settings);

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();

    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE QVariantMap shortcuts() const;
    Q_INVOKABLE QString shortcut(const QString &actionId, const QString &fallbackValue = QString()) const;

  signals:
    void settingsChanged();

  private:
    explicit SettingsManager(QObject *parent = nullptr);
    static QString getSettingsFilePath();
    static QVariantMap defaultShortcutSettings();

    QVariantMap m_settings;
};

} // namespace AviQtl::Core