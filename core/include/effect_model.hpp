#pragma once
#include "../../scripting/lua_host.hpp"
#include <QColor>
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
using EasingFunction = std::function<double(double, const std::vector<double> &)>;

class EffectModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString category READ category CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)
    Q_PROPERTY(QString qmlSource READ qmlSource CONSTANT)
    Q_PROPERTY(QVariantMap keyframeTracks READ keyframeTracks NOTIFY keyframeTracksChanged)
    Q_PROPERTY(QVariantMap uiDefinition READ uiDefinition CONSTANT)

  public:
    explicit EffectModel(const QString &id, const QString &name, const QString &category = "filter", const QVariantMap &params = {}, const QString &qmlSource = "", const QVariantMap &uiDef = {}, QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_name(name), m_category(category), m_enabled(true), m_params(params), m_qmlSource(qmlSource), m_uiDefinition(uiDef) {}

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString category() const { return m_category; }
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

        if (options.contains("points"))
            kf["points"] = options.value("points");
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
        // easeOutBounce を先に定義 (easeInBounce/easeInOutBounce が参照)
        static auto easeOutBounce = [](double x) -> double {
            constexpr double n1 = 7.5625, d1 = 2.75;
            if (x < 1.0 / d1)
                return n1 * x * x;
            if (x < 2.0 / d1) {
                x -= 1.5 / d1;
                return n1 * x * x + 0.75;
            }
            if (x < 2.5 / d1) {
                x -= 2.25 / d1;
                return n1 * x * x + 0.9375;
            }
            x -= 2.625 / d1;
            return n1 * x * x + 0.984375;
        };

        static const std::map<QString, EasingFunction> funcs = {
                                                                // ─── 既存 (後方互換) ───────────────────────────────────────
                                                                {"linear", [](double t, const auto &) { return t; }},
                                                                // ─── Sine ─────────────────────────────────────────────────
                                                                {"ease_in_sine", [](double t, const auto &) { return 1.0 - std::cos(t * M_PI / 2.0); }},
                                                                {"ease_out_sine", [](double t, const auto &) { return std::sin(t * M_PI / 2.0); }},
                                                                {"ease_in_out_sine", [](double t, const auto &) { return -(std::cos(M_PI * t) - 1.0) / 2.0; }},
                                                                {"ease_out_in_sine", [](double t, const auto &) {
                                                                     return t < 0.5 ? std::sin(t * M_PI) / 2.0 : (1.0 - std::cos((t * 2.0 - 1.0) * M_PI / 2.0)) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Quad ─────────────────────────────────────────────────
                                                                {"ease_in_quad", [](double t, const auto &) { return t * t; }},
                                                                {"ease_out_quad", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t); }},
                                                                {"ease_in_out_quad", [](double t, const auto &) { return t < 0.5 ? 2.0 * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
                                                                {"ease_out_in_quad", [](double t, const auto &) {
                                                                     return t < 0.5 ? (1.0 - (1.0 - 2.0 * t) * (1.0 - 2.0 * t)) / 2.0 : (2.0 * t - 1.0) * (2.0 * t - 1.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Cubic ────────────────────────────────────────────────
                                                                {"ease_in_cubic", [](double t, const auto &) { return t * t * t; }},
                                                                {"ease_out_cubic", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t); }},
                                                                {"ease_in_out_cubic", [](double t, const auto &) { return t < 0.5 ? 4.0 * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
                                                                {"ease_out_in_cubic", [](double t, const auto &) {
                                                                     return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 3.0)) / 2.0 : std::pow(2.0 * t - 1.0, 3.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Quart ────────────────────────────────────────────────
                                                                {"ease_in_quart", [](double t, const auto &) { return t * t * t * t; }},
                                                                {"ease_out_quart", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t); }},
                                                                {"ease_in_out_quart", [](double t, const auto &) { return t < 0.5 ? 8.0 * t * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
                                                                {"ease_out_in_quart", [](double t, const auto &) {
                                                                     return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 4.0)) / 2.0 : std::pow(2.0 * t - 1.0, 4.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Quint ────────────────────────────────────────────────
                                                                {"ease_in_quint", [](double t, const auto &) { return t * t * t * t * t; }},
                                                                {"ease_out_quint", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t); }},
                                                                {"ease_in_out_quint", [](double t, const auto &) { return t < 0.5 ? 16.0 * t * t * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
                                                                {"ease_out_in_quint", [](double t, const auto &) {
                                                                     return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 5.0)) / 2.0 : std::pow(2.0 * t - 1.0, 5.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Expo ─────────────────────────────────────────────────
                                                                {"ease_in_expo", [](double t, const auto &) { return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0); }},
                                                                {"ease_out_expo", [](double t, const auto &) { return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t); }},
                                                                {"ease_in_out_expo", [](double t, const auto &) {
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0 : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
                                                                 }},
                                                                {"ease_out_in_expo", [](double t, const auto &) {
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     return t < 0.5 ? (1.0 - std::pow(2.0, -20.0 * t)) / 2.0 : std::pow(2.0, 20.0 * t - 20.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Circ ─────────────────────────────────────────────────
                                                                {"ease_in_circ", [](double t, const auto &) { return 1.0 - std::sqrt(1.0 - t * t); }},
                                                                {"ease_out_circ", [](double t, const auto &) { return std::sqrt(1.0 - (t - 1.0) * (t - 1.0)); }},
                                                                {"ease_in_out_circ", [](double t, const auto &) {
                                                                     return t < 0.5 ? (1.0 - std::sqrt(1.0 - 4.0 * t * t)) / 2.0 : (std::sqrt(1.0 - (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) + 1.0) / 2.0;
                                                                 }},
                                                                {"ease_out_in_circ", [](double t, const auto &) {
                                                                     return t < 0.5 ? std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0)) / 2.0 : (1.0 - std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0))) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Back ─────────────────────────────────────────────────
                                                                {"ease_in_back", [](double t, const auto &) {
                                                                     constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
                                                                     return c3 * t * t * t - c1 * t * t;
                                                                 }},
                                                                {"ease_out_back", [](double t, const auto &) {
                                                                     constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
                                                                     return 1.0 + c3 * (t - 1.0) * (t - 1.0) * (t - 1.0) + c1 * (t - 1.0) * (t - 1.0);
                                                                 }},
                                                                {"ease_in_out_back", [](double t, const auto &) {
                                                                     constexpr double c2 = 1.70158 * 1.525;
                                                                     return t < 0.5 ? ((2.0 * t) * (2.0 * t) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0 : ((2.0 * t - 2.0) * (2.0 * t - 2.0) * ((c2 + 1.0) * (2.0 * t - 2.0) + c2) + 2.0) / 2.0;
                                                                 }},
                                                                {"ease_out_in_back", [](double t, const auto &) {
                                                                     constexpr double c1 = 1.70158, c3 = c1 + 1.0;
                                                                     auto eout = [&](double u) { return 1.0 + c3 * (u - 1.0) * (u - 1.0) * (u - 1.0) + c1 * (u - 1.0) * (u - 1.0); };
                                                                     auto ein = [&](double u) { return c3 * u * u * u - c1 * u * u; };
                                                                     return t < 0.5 ? eout(2.0 * t) / 2.0 : ein(2.0 * t - 1.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Elastic ──────────────────────────────────────────────
                                                                {"ease_in_elastic", [](double t, const auto &) {
                                                                     constexpr double c4 = (2.0 * M_PI) / 3.0;
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * c4);
                                                                 }},
                                                                {"ease_out_elastic", [](double t, const auto &) {
                                                                     constexpr double c4 = (2.0 * M_PI) / 3.0;
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * c4) + 1.0;
                                                                 }},
                                                                {"ease_in_out_elastic", [](double t, const auto &) {
                                                                     constexpr double c5 = (2.0 * M_PI) / 4.5;
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     return t < 0.5 ? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 : (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 + 1.0;
                                                                 }},
                                                                {"ease_out_in_elastic", [](double t, const auto &) {
                                                                     constexpr double c4 = (2.0 * M_PI) / 3.0;
                                                                     if (t == 0.0)
                                                                         return 0.0;
                                                                     if (t == 1.0)
                                                                         return 1.0;
                                                                     auto eout = [&](double u) { return std::pow(2.0, -10.0 * u) * std::sin((u * 10.0 - 0.75) * c4) + 1.0; };
                                                                     auto ein = [&](double u) {
                                                                         if (u == 0.0)
                                                                             return 0.0;
                                                                         if (u == 1.0)
                                                                             return 1.0;
                                                                         return -std::pow(2.0, 10.0 * u - 10.0) * std::sin((u * 10.0 - 10.75) * c4);
                                                                     };
                                                                     return t < 0.5 ? eout(2.0 * t) / 2.0 : ein(2.0 * t - 1.0) / 2.0 + 0.5;
                                                                 }},
                                                                // ─── Bounce ───────────────────────────────────────────────
                                                                {"ease_out_bounce", [](double t, const auto &) { return easeOutBounce(t); }},
                                                                {"ease_in_bounce", [](double t, const auto &) { return 1.0 - easeOutBounce(1.0 - t); }},
                                                                {"ease_in_out_bounce", [](double t, const auto &) { return t < 0.5 ? (1.0 - easeOutBounce(1.0 - 2.0 * t)) / 2.0 : (1.0 + easeOutBounce(2.0 * t - 1.0)) / 2.0; }},
                                                                {"ease_out_in_bounce", [](double t, const auto &) { return t < 0.5 ? easeOutBounce(2.0 * t) / 2.0 : (1.0 - easeOutBounce(1.0 - 2.0 * (t - 0.5))) / 2.0 + 0.5; }},
                                                                // ─── カスタムベジェ ───────────────────────────────────────
                                                                {"custom", [](double x, const auto &p) {
                                                                     // p = [cp1x, cp1y, cp2x, cp2y, endX, endY, ...repeat...]
                                                                     // セグメント構造: 始点(prevX, prevY) -> 制御点1, 制御点2 -> 終点(endX, endY)
                                                                     double prevX = 0, prevY = 0;
                                                                     for (size_t i = 0; i < p.size(); i += 6) {
                                                                         double cp1x = p[i], cp1y = p[i + 1];
                                                                         double cp2x = p[i + 2], cp2y = p[i + 3];
                                                                         double endX = p[i + 4], endY = p[i + 5];

                                                                         // 現在のXがこのセグメントに含まれるか、または最後のセグメントなら計算
                                                                         if (x <= endX || i + 6 >= p.size()) {
                                                                             // セグメント内の正規化時間 T を求めるために、X座標を [0,1] 区間にマップする
                                                                             // 簡易化のため、X範囲内での相対位置として解く
                                                                             double range = endX - prevX;
                                                                             if (range < 1e-6)
                                                                                 return endY; // 垂直ジャンプ

                                                                             // ニュートン法のための正規化された制御点
                                                                             double n_cp1x = (cp1x - prevX) / range;
                                                                             double n_cp2x = (cp2x - prevX) / range;
                                                                             double n_x = (x - prevX) / range;

                                                                             double t = solveBezierT(n_x, n_cp1x, n_cp2x);
                                                                             // Y座標の補間
                                                                             return (1 - t) * (1 - t) * (1 - t) * prevY + 3 * (1 - t) * (1 - t) * t * cp1y + 3 * (1 - t) * t * t * cp2y + t * t * t * endY;
                                                                         }
                                                                         prevX = endX;
                                                                         prevY = endY;
                                                                     }
                                                                     return x;
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
        auto getBezierParams = [](const QVariant &v) -> std::vector<double> {
            const auto map = v.toMap();
            if (map.contains("points")) {
                QVariantList lst = map.value("points").toList();
                std::vector<double> pts;
                for (const auto &val : lst)
                    pts.push_back(val.toDouble());
                return pts;
            }
            // 旧 bezier 形式を custom 形式に変換 (後方互換)
            return {map.value("bzx1", 0.33).toDouble(), map.value("bzy1", 0.0).toDouble(), map.value("bzx2", 0.66).toDouble(), map.value("bzy2", 1.0).toDouble(), 1.0, 1.0};
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

            const double tRaw = (frame - f0) / double(f1 - f0);

            QString type = getInterp(track[i]);

            if (!easingFunctions().count(type))
                type = "linear";

            // 色 (文字列) の補間
            if (v0.typeId() == QMetaType::QString && v1.typeId() == QMetaType::QString) {
                QColor c0(v0.toString());
                QColor c1(v1.toString());
                if (c0.isValid() && c1.isValid()) {
                    std::vector<double> params;
                    if (type == "custom") {
                        params = getBezierParams(track[i]);
                    }
                    const double t = easingFunctions().at(type)(tRaw, params);

                    int r = static_cast<int>(c0.red() + (c1.red() - c0.red()) * t);
                    int g = static_cast<int>(c0.green() + (c1.green() - c0.green()) * t);
                    int b = static_cast<int>(c0.blue() + (c1.blue() - c0.blue()) * t);
                    int a = static_cast<int>(c0.alpha() + (c1.alpha() - c0.alpha()) * t);

                    return QColor(r, g, b, a).name(QColor::HexArgb);
                }
            }

            // 数値の補間
            if (!numeric || !v0.canConvert<double>() || !v1.canConvert<double>()) {
                return v0; // step
            }

            const double a = v0.toDouble();
            const double b = v1.toDouble();

            std::vector<double> params;
            if (type == "custom") {
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
    QString m_category;
    bool m_enabled;
    QVariantMap m_params;
    QString m_qmlSource;
    QVariantMap m_uiDefinition;
    QVariantMap m_keyframeTracks; // パラメータ名 -> QVariantList[{frame,value,interp}]
};
} // namespace Rina::UI