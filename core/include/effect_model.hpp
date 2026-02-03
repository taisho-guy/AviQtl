#pragma once
#include "../../scripting/lua_host.hpp"
#include <QObject>
#include <QQmlEngine>
#include <QVariant>
#include <QVariantList>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>

namespace Rina::UI {

// イージング関数シグネチャ: double function(t, params)
using EasingFunction = std::function<double(double, const std::array<double, 4> &)>;

class EffectModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)
    Q_PROPERTY(QString qmlSource READ qmlSource CONSTANT)
    Q_PROPERTY(QVariantMap keyframeTracks READ keyframeTracks NOTIFY keyframeTracksChanged)
    Q_PROPERTY(QVariantMap uiDefinition READ uiDefinition CONSTANT)

  public:
    explicit EffectModel(const QString &id, const QString &name, const QVariantMap &params = {}, const QString &qmlSource = "", const QVariantMap &uiDef = {}, QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_name(name), m_enabled(true), m_params(params), m_qmlSource(qmlSource), m_uiDefinition(uiDef) {}

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    bool isEnabled() const { return m_enabled; }
    QVariantMap params() const { return m_params; }
    QString qmlSource() const { return m_qmlSource; }
    QVariantMap keyframeTracks() const { return m_keyframeTracks; }
    QVariantMap uiDefinition() const { return m_uiDefinition; }

    // Must be public to be invokable from QML
    Q_INVOKABLE QStringList availableEasings() const {
        QStringList keys;
        for (const auto &[k, v] : easingFunctions())
            keys << k;
        return keys;
    }

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

    Q_INVOKABLE void setKeyframe(const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
        QVariantList track = m_keyframeTracks.value(paramName).toList();
        QVariantMap kf;
        kf["frame"] = frame;
        kf["value"] = value;
        kf["interp"] = options.value("interp", "linear");

        // Add bezier params if provided
        if (options.contains("bz_x1"))
            kf["bz_x1"] = options.value("bz_x1");
        if (options.contains("bz_y1"))
            kf["bz_y1"] = options.value("bz_y1");
        if (options.contains("bz_x2"))
            kf["bz_x2"] = options.value("bz_x2");
        if (options.contains("bz_y2"))
            kf["bz_y2"] = options.value("bz_y2");
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

    Q_INVOKABLE QVariantMap evaluatedParams(int frame) const {
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

        // 1. キーフレームからベース値を計算
        QVariant baseValue = evaluateTrack(m_keyframeTracks.value(paramName).toList(), frame, fallback);

        // 2. パラメータ自体がエクスプレッション定義かチェック（簡易実装）
        QString strVal = m_params.value(paramName).toString();
        if (strVal.startsWith("=")) {
            // "=time*100" -> "time*100"
            std::string expr = strVal.mid(1).toStdString();
            // time: current frame / fps (needs context, passing frame/60.0 for now)
            // TODO: 正確なFPSを取得するにはContextが必要
            double time = frame / 60.0;
            return Rina::Scripting::LuaHost::instance().evaluate(expr, time, 0, baseValue.toDouble());
        }

        return baseValue;
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
    // Static easing function registry
    static const std::map<QString, EasingFunction> &easingFunctions() {
        static const std::map<QString, EasingFunction> funcs = {{"linear", [](double t, const auto &) { return t; }},
                                                                {"ease_in", [](double t, const auto &) { return t * t; }},
                                                                {"ease_out", [](double t, const auto &) { return t * (2.0 - t); }},
                                                                {"ease_in_out", [](double t, const auto &) { return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t; }},
                                                                {"bezier", [](double x, const auto &p) {
                                                                     // p = {x1, y1, x2, y2} 制御点
                                                                     const double t = solveBezierT(x, p[0], p[2]);
                                                                     const double one_minus_t = 1.0 - t;
                                                                     // y(t) = 3(1-t)^2 * t * y1 + 3(1-t) * t^2 * y2 + t^3  (3次ベジェ曲線のY座標)
                                                                     return 3 * one_minus_t * one_minus_t * t * p[1] + 3 * one_minus_t * t * t * p[3] + t * t * t;
                                                                 }}};
        return funcs;
    }

    static QVariant evaluateTrack(const QVariantList &track, int frame, const QVariant &fallback) {
        if (track.isEmpty())
            return fallback;

        auto getFrame = [](const QVariant &v) { return v.toMap().value("frame").toInt(); };
        auto getValue = [](const QVariant &v) { return v.toMap().value("value"); };
        auto getInterp = [](const QVariant &v) { return v.toMap().value("interp").toString(); };
        // ベジェ制御点の取得ヘルパー
        auto getBezierParams = [](const QVariant &v) -> std::array<double, 4> {
            const auto map = v.toMap();
            return {map.value("bz_x1", 0.33).toDouble(), map.value("bz_y1", 0.0).toDouble(), map.value("bz_x2", 0.66).toDouble(), map.value("bz_y2", 1.0).toDouble()};
        };

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

            QString type = getInterp(track[i]);
            if (!easingFunctions().count(type))
                type = "linear";

            std::array<double, 4> params = {0, 0, 0, 0};
            if (type == "bezier") {
                params = getBezierParams(track[i]);
            }

            const double t = easingFunctions().at(type)(tRaw, params);
            return a + (b - a) * t;
        }

        return getValue(track.back());
    }

    // 3次ベジェ曲線のX座標(時間)からT(パラメータ)を求める - ニュートン法
    static double solveBezierT(double x, double x1, double x2) {
        // 線形に近い場合は早期リターン
        if (x1 == x2 && x1 == x)
            return x;

        double t = x;                 // 初期推定値
        for (int i = 0; i < 8; ++i) { // 最大8回反復（通常は十分な精度が得られる）
            const double one_minus_t = 1.0 - t;
            // x(t) = 3(1-t)^2 * t * x1 + 3(1-t) * t^2 * x2 + t^3
            const double current_x = 3 * one_minus_t * one_minus_t * t * x1 + 3 * one_minus_t * t * t * x2 + t * t * t;
            const double error = current_x - x;
            if (std::abs(error) < 1e-5)
                return t;

            // dx/dt = 3(1-t)^2*x1 + 6(1-t)t(x2-x1) + 3t^2(1-x2)
            const double dx_dt = 3 * one_minus_t * one_minus_t * x1 + 6 * one_minus_t * t * (x2 - x1) + 3 * t * t * (1.0 - x2);
            if (std::abs(dx_dt) < 1e-6)
                break; // 傾き0回避
            t -= error / dx_dt;
        }
        return std::clamp(t, 0.0, 1.0);
    }

    QString m_id;
    QString m_name;
    bool m_enabled;
    QVariantMap m_params;
    QString m_qmlSource;
    QVariantMap m_uiDefinition;
    QVariantMap m_keyframeTracks; // パラメータ名 -> QVariantList[{frame,value,interp}]
};
} // namespace Rina::UI