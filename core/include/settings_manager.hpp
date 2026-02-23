#pragma once
#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

namespace Rina::Core {

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

  signals:
    void settingsChanged();

  private:
    explicit SettingsManager(QObject *parent = nullptr);
    QString getSettingsFilePath() const;

    QVariantMap m_settings;
};

} // namespace Rina::Core