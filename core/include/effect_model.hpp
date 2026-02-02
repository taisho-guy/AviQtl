#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QVariant>
#include <QVariantList>
#include <algorithm>
#include <cmath>

namespace Rina::UI {

// 補間タイプの列挙（setKeyframeで使用前に定義）
enum class InterpolationType { Linear, EaseIn, EaseOut, EaseInOut, Bezier };

class EffectModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)
    Q_PROPERTY(QString qmlSource READ qmlSource CONSTANT)
    Q_PROPERTY(QVariantMap keyframeTracks READ keyframeTracks NOTIFY keyframeTracksChanged)

  public:
    explicit EffectModel(const QString &id, const QString &name, const QVariantMap &params = {}, const QString &qmlSource = "", QObject *parent = nullptr)
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

    Q_INVOKABLE void setParam(const QString &key, const QVariant &val) {
        if (m_params[key] != val) {
            m_params[key] = val;
            emit paramsChanged();
            emit paramChanged(key, val);
        }
    }

    Q_INVOKABLE void setKeyframe(const QString &paramName, int frame, const QVariant &value, const QString &interpType) {
        QVariantList track = m_keyframeTracks.value(paramName).toList();
        QVariantMap kf;
        kf["frame"] = frame;
        kf["value"] = value;
        kf["interp"] = interpType;

        bool updated = false;
        for (int i = 0; i < track.size(); ++i) {
            const auto m = track[i].toMap();
            if (m.value("frame").toInt() == frame) {
                track[i] = kf;
                updated = true;
                break;
            }
        }
        if (!updated)
            track.append(kf);

        std::sort(track.begin(), track.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("frame").toInt() < b.toMap().value("frame").toInt(); });

        m_keyframeTracks[paramName] = track;
        emit keyframeTracksChanged();
    }

    Q_INVOKABLE void removeKeyframe(const QString &paramName, int frame) {
        QVariantList track = m_keyframeTracks.value(paramName).toList();
        QVariantList next;
        next.reserve(track.size());
        for (const auto &v : track) {
            const auto m = v.toMap();
            if (m.value("frame").toInt() != frame)
                next.append(v);
        }
        if (next.isEmpty()) {
            m_keyframeTracks.remove(paramName);
        } else {
            m_keyframeTracks[paramName] = next;
        }
        emit keyframeTracksChanged();
    }

    QVariantMap evaluatedParams(int frame) const {
        QVariantMap out = m_params;
        for (auto it = m_keyframeTracks.begin(); it != m_keyframeTracks.end(); ++it) {
            const QString paramName = it.key();
            out[paramName] = evaluatedParam(paramName, frame);
        }
        return out;
    }

    Q_INVOKABLE QVariant evaluatedParam(const QString &paramName, int frame) const {
        const QVariant fallback = m_params.value(paramName);
        if (!m_keyframeTracks.contains(paramName))
            return fallback;

        return evaluateTrack(m_keyframeTracks.value(paramName).toList(), frame, fallback);
    }

    void setKeyframeTracks(const QVariantMap &tracks) {
        m_keyframeTracks = tracks;
        emit keyframeTracksChanged();
    }

  signals:
    void enabledChanged();
    void paramsChanged();
    void paramChanged(const QString &key, const QVariant &value);
    void keyframeTracksChanged();

  private:
    static InterpolationType stringToInterpolationType(const QString &s) {
        if (s == "ease_in")
            return InterpolationType::EaseIn;
        if (s == "ease_out")
            return InterpolationType::EaseOut;
        if (s == "ease_in_out")
            return InterpolationType::EaseInOut;
        if (s == "bezier")
            return InterpolationType::Bezier;
        return InterpolationType::Linear;
    }

    static QVariant evaluateTrack(const QVariantList &track, int frame, const QVariant &fallback) {
        if (track.isEmpty())
            return fallback;

        auto getFrame = [](const QVariant &v) { return v.toMap().value("frame").toInt(); };
        auto getValue = [](const QVariant &v) { return v.toMap().value("value"); };
        auto getInterp = [](const QVariant &v) { return v.toMap().value("interp").toString(); };

        if (frame <= getFrame(track.front()))
            return getValue(track.front());
        if (frame >= getFrame(track.back()))
            return getValue(track.back());

        // 非数値はステップ（直前の値）
        const bool numeric = fallback.canConvert<double>();

        for (int i = 0; i < track.size() - 1; ++i) {
            const int f0 = getFrame(track[i]);
            const int f1 = getFrame(track[i + 1]);

            if (frame < f0 || frame > f1)
                continue;

            const QVariant v0 = getValue(track[i]);
            const QVariant v1 = getValue(track[i + 1]);

            if (!numeric || !v0.canConvert<double>() || !v1.canConvert<double>()) {
                return v0; // step
            }

            const double a = v0.toDouble();
            const double b = v1.toDouble();
            const double tRaw = (frame - f0) / double(f1 - f0);
            const double t = applyInterpolation(stringToInterpolationType(getInterp(track[i])), tRaw);
            return a + (b - a) * t;
        }

        return getValue(track.back());
    }

    static double applyInterpolation(InterpolationType t, double x) {
        x = std::clamp(x, 0.0, 1.0);
        switch (t) {
        case InterpolationType::EaseIn:
            return x * x;
        case InterpolationType::EaseOut: {
            const double u = 1.0 - x;
            return 1.0 - u * u;
        }
        case InterpolationType::EaseInOut:
            if (x < 0.5)
                return 2.0 * x * x;
            return 1.0 - 2.0 * (1.0 - x) * (1.0 - x);
        default:
            return x; // Linear / Bezier(未実装)
        }
    }

    QString m_id;
    QString m_name;
    bool m_enabled;
    QVariantMap m_params;
    QString m_qmlSource;
    QVariantMap m_keyframeTracks; // paramName -> QVariantList[{frame,value,interp}]
};
} // namespace Rina::UI