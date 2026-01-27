#pragma once
#include <QObject>
#include <QVariant>
#include <QQmlEngine>

namespace Rina::UI {

// 補間タイプの列挙（setKeyframeで使用前に定義）
enum class InterpolationType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bezier
};

    class EffectModel : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString id READ id CONSTANT)
        Q_PROPERTY(QString name READ name CONSTANT)
        Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
        Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)
        Q_PROPERTY(QString qmlSource READ qmlSource CONSTANT)
    Q_PROPERTY(QVariantMap keyframeTracks READ keyframeTracks NOTIFY keyframeTracksChanged)

    public:
        explicit EffectModel(const QString& id, const QString& name, const QVariantMap& params = {}, const QString& qmlSource = "", QObject* parent = nullptr)
            : QObject(parent), m_id(id), m_name(name), m_enabled(true), m_params(params), m_qmlSource(qmlSource) {}

        QString id() const { return m_id; }
        QString name() const { return m_name; }
        bool isEnabled() const { return m_enabled; }
        QVariantMap params() const { return m_params; }
        QString qmlSource() const { return m_qmlSource; }
    QVariantMap keyframeTracks() const { return m_keyframeTracks; }

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

    Q_INVOKABLE void setKeyframe(const QString& paramName, int frame, const QVariant& value, const QString& interpType) {
        InterpolationType interp = stringToInterpolationType(interpType);
        // TODO: KeyframeTrackの実装を完成させる
        emit keyframeTracksChanged();
    }

    signals:
        void enabledChanged();
        void paramsChanged();
        void paramChanged(const QString& key, const QVariant& value);
    void keyframeTracksChanged();

    private:
        InterpolationType stringToInterpolationType(const QString& str) {
            if (str == "ease_in") return InterpolationType::EaseIn;
            if (str == "ease_out") return InterpolationType::EaseOut;
            if (str == "ease_in_out") return InterpolationType::EaseInOut;
            return InterpolationType::Linear;
        }

        QString m_id;
        QString m_name;
        bool m_enabled;
        QVariantMap m_params;
        QString m_qmlSource;
    QVariantMap m_keyframeTracks;  // paramName -> KeyframeTrack (将来的にC++オブジェクトに変更)
    };
}