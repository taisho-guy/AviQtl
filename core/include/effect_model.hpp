#pragma once
#include <QObject>
#include <QVariant>
#include <QQmlEngine>

namespace Rina::UI {
    class EffectModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString id READ id CONSTANT)
        Q_PROPERTY(QString name READ name CONSTANT)
        Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
        Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)

    public:
        explicit EffectModel(const QString& id, const QString& name, const QVariantMap& params = {}, QObject* parent = nullptr)
            : QObject(parent), m_id(id), m_name(name), m_enabled(true), m_params(params) {}

        QString id() const { return m_id; }
        QString name() const { return m_name; }
        bool isEnabled() const { return m_enabled; }
        QVariantMap params() const { return m_params; }

        void setEnabled(bool e) {
            if (m_enabled != e) {
                m_enabled = e;
                emit enabledChanged();
            }
        }
        
        Q_INVOKABLE void setParam(const QString& key, const QVariant& val) {
            if (m_params[key] != val) {
                m_params[key] = val;
                emit paramsChanged();
                emit paramChanged(key, val);
            }
        }

    signals:
        void enabledChanged();
        void paramsChanged();
        void paramChanged(const QString& key, const QVariant& value);

    private:
        QString m_id;
        QString m_name;
        bool m_enabled;
        QVariantMap m_params;
    };
}