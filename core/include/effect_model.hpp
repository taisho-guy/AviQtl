#pragma once
#include "../../scripting/lua_host.hpp"
#include <QColor>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariant>
#include <QVariantList>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>

namespace Rina::UI {

// イージング関数シグネチャ: double function(t, params)
using EasingFunction = std::function<double(double, const std::vector<double> &)>;

class EffectModel : public QObject {
    Q_OBJECT

  private:
    // ─── Static Helpers (メソッドからの参照のため先頭に配置) ───

    static bool isStructuredTrack(const QVariant &raw) {
        const QVariantMap m = raw.toMap();
        return m.contains("start") && m.contains("points");
    }
    // end キーが存在し非空マップであれば明示的終了点あり
    static bool hasExplicitEnd(const QVariant &rawTrack) {
        if (!rawTrack.canConvert<QVariantMap>())
            return false;
        const QVariantMap m = rawTrack.toMap();
        const QVariant ev = m.value("end");
        return ev.canConvert<QVariantMap>() && !ev.toMap().isEmpty();
    }

    static QVariantMap makePoint(int frame, const QVariant &value, const QString &interp = "none") {
        QVariantMap p;
        p["frame"] = frame;
        p["value"] = value;
        p["interp"] = interp;
        return p;
    }

    static QVariantList sortPoints(QVariantList points) {
        std::sort(points.begin(), points.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("frame").toInt() < b.toMap().value("frame").toInt(); });
        return points;
    }

    static int inferredDurationForTrack(const QVariant &raw) {
        if (isStructuredTrack(raw)) {
            // 明示的終了点があればその位置を、なければ中間点の最大フレームを使用
            if (hasExplicitEnd(raw)) {
                const int endFrame = raw.toMap().value("end").toMap().value("frame").toInt();
                return std::max(1, endFrame + 1);
            }
            const QVariantList points = raw.toMap().value("points").toList();
            int maxFrame = 0;
            for (const auto &v : points)
                maxFrame = std::max(maxFrame, v.toMap().value("frame").toInt());
            return std::max(1, maxFrame + 1);
        }
        const QVariantList list = raw.toList();
        if (list.isEmpty())
            return 1;
        int maxFrame = 0;
        for (const auto &v : list)
            maxFrame = std::max(maxFrame, v.toMap().value("frame").toInt());
        return std::max(1, maxFrame + 1);
    }

    static QVariantList flattenStructuredTrack(const QVariantMap &track) {
        QVariantList out;
        out.append(track.value("start"));
        QVariantList points = track.value("points").toList();
        points = sortPoints(points);
        for (const auto &v : points)
            out.append(v);
        // end が明示的に存在する場合のみ追加（任意終了点の哲学）
        const QVariant endVal = track.value("end");
        if (endVal.canConvert<QVariantMap>() && !endVal.toMap().isEmpty())
            out.append(endVal);
        return out;
    }

    // 3次ベジェ曲線のX座標(時間)からT(パラメータ)を求める - ニュートン法
    static double solveBezierT(double x, double x1, double x2) {
        if (x1 == x2 && x1 == x)
            return x;
        double t = x;
        for (int i = 0; i < 8; ++i) {
            const double one_minus_t = 1.0 - t;
            const double current_x = 3 * one_minus_t * one_minus_t * t * x1 + 3 * one_minus_t * t * t * x2 + t * t * t;
            const double error = current_x - x;
            if (std::abs(error) < 1e-5)
                return t;
            const double dx_dt = 3 * one_minus_t * one_minus_t * x1 + 6 * one_minus_t * t * (x2 - x1) + 3 * t * t * (1.0 - x2);
            if (std::abs(dx_dt) < 1e-6)
                break;
            t -= error / dx_dt;
        }
        return std::clamp(t, 0.0, 1.0);
    }

    static const QHash<QString, EasingFunction> &easingFunctions() {
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

        static const QHash<QString, EasingFunction> funcs = {
            {"linear", [](double t, const auto &) { return t; }},
            {"ease_in_sine", [](double t, const auto &) { return 1.0 - std::cos(t * M_PI / 2.0); }},
            {"ease_out_sine", [](double t, const auto &) { return std::sin(t * M_PI / 2.0); }},
            {"ease_in_out_sine", [](double t, const auto &) { return -(std::cos(M_PI * t) - 1.0) / 2.0; }},
            {"ease_out_in_sine", [](double t, const auto &) { return t < 0.5 ? std::sin(t * M_PI) / 2.0 : (1.0 - std::cos((t * 2.0 - 1.0) * M_PI / 2.0)) / 2.0 + 0.5; }},
            {"ease_in_quad", [](double t, const auto &) { return t * t; }},
            {"ease_out_quad", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t); }},
            {"ease_in_out_quad", [](double t, const auto &) { return t < 0.5 ? 2.0 * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
            {"ease_out_in_quad", [](double t, const auto &) { return t < 0.5 ? (1.0 - (1.0 - 2.0 * t) * (1.0 - 2.0 * t)) / 2.0 : (2.0 * t - 1.0) * (2.0 * t - 1.0) / 2.0 + 0.5; }},
            {"ease_in_cubic", [](double t, const auto &) { return t * t * t; }},
            {"ease_out_cubic", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t); }},
            {"ease_in_out_cubic", [](double t, const auto &) { return t < 0.5 ? 4.0 * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
            {"ease_out_in_cubic", [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 3.0)) / 2.0 : std::pow(2.0 * t - 1.0, 3.0) / 2.0 + 0.5; }},
            {"ease_in_quart", [](double t, const auto &) { return t * t * t * t; }},
            {"ease_out_quart", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t); }},
            {"ease_in_out_quart", [](double t, const auto &) { return t < 0.5 ? 8.0 * t * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
            {"ease_out_in_quart", [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 4.0)) / 2.0 : std::pow(2.0 * t - 1.0, 4.0) / 2.0 + 0.5; }},
            {"ease_in_quint", [](double t, const auto &) { return t * t * t * t * t; }},
            {"ease_out_quint", [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t) * (1.0 - t); }},
            {"ease_in_out_quint", [](double t, const auto &) { return t < 0.5 ? 16.0 * t * t * t * t * t : 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) / 2.0; }},
            {"ease_out_in_quint", [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 5.0)) / 2.0 : std::pow(2.0 * t - 1.0, 5.0) / 2.0 + 0.5; }},
            {"ease_in_expo", [](double t, const auto &) { return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0); }},
            {"ease_out_expo", [](double t, const auto &) { return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t); }},
            {"ease_in_out_expo",
             [](double t, const auto &) {
                 if (t == 0.0)
                     return 0.0;
                 if (t == 1.0)
                     return 1.0;
                 return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0 : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
             }},
            {"ease_out_in_expo",
             [](double t, const auto &) {
                 if (t == 0.0)
                     return 0.0;
                 if (t == 1.0)
                     return 1.0;
                 return t < 0.5 ? (1.0 - std::pow(2.0, -20.0 * t)) / 2.0 : std::pow(2.0, 20.0 * t - 20.0) / 2.0 + 0.5;
             }},
            {"ease_in_circ", [](double t, const auto &) { return 1.0 - std::sqrt(1.0 - t * t); }},
            {"ease_out_circ", [](double t, const auto &) { return std::sqrt(1.0 - (t - 1.0) * (t - 1.0)); }},
            {"ease_in_out_circ", [](double t, const auto &) { return t < 0.5 ? (1.0 - std::sqrt(1.0 - 4.0 * t * t)) / 2.0 : (std::sqrt(1.0 - (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) + 1.0) / 2.0; }},
            {"ease_out_in_circ", [](double t, const auto &) { return t < 0.5 ? std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0)) / 2.0 : (1.0 - std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0))) / 2.0 + 0.5; }},
            {"ease_in_back",
             [](double t, const auto &) {
                 constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
                 return c3 * t * t * t - c1 * t * t;
             }},
            {"ease_out_back",
             [](double t, const auto &) {
                 constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
                 return 1.0 + c3 * (t - 1.0) * (t - 1.0) * (t - 1.0) + c1 * (t - 1.0) * (t - 1.0);
             }},
            {"ease_in_out_back",
             [](double t, const auto &) {
                 constexpr double c2 = 1.70158 * 1.525;
                 return t < 0.5 ? ((2.0 * t) * (2.0 * t) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0 : ((2.0 * t - 2.0) * (2.0 * t - 2.0) * ((c2 + 1.0) * (2.0 * t - 2.0) + c2) + 2.0) / 2.0;
             }},
            {"ease_out_in_back",
             [](double t, const auto &) {
                 constexpr double c1 = 1.70158, c3 = c1 + 1.0;
                 auto eout = [&](double u) { return 1.0 + c3 * (u - 1.0) * (u - 1.0) * (u - 1.0) + c1 * (u - 1.0) * (u - 1.0); };
                 auto ein = [&](double u) { return c3 * u * u * u - c1 * u * u; };
                 return t < 0.5 ? eout(2.0 * t) / 2.0 : ein(2.0 * t - 1.0) / 2.0 + 0.5;
             }},
            {"ease_in_elastic",
             [](double t, const auto &) {
                 constexpr double c4 = (2.0 * M_PI) / 3.0;
                 if (t == 0.0)
                     return 0.0;
                 if (t == 1.0)
                     return 1.0;
                 return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * c4);
             }},
            {"ease_out_elastic",
             [](double t, const auto &) {
                 constexpr double c4 = (2.0 * M_PI) / 3.0;
                 if (t == 0.0)
                     return 0.0;
                 if (t == 1.0)
                     return 1.0;
                 return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * c4) + 1.0;
             }},
            {"ease_in_out_elastic",
             [](double t, const auto &) {
                 constexpr double c5 = (2.0 * M_PI) / 4.5;
                 if (t == 0.0)
                     return 0.0;
                 if (t == 1.0)
                     return 1.0;
                 return t < 0.5 ? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 : (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 + 1.0;
             }},
            {"ease_out_in_elastic",
             [](double t, const auto &) {
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
            {"ease_out_bounce", [](double t, const auto &) { return easeOutBounce(t); }},
            {"ease_in_bounce", [](double t, const auto &) { return 1.0 - easeOutBounce(1.0 - t); }},
            {"ease_in_out_bounce", [](double t, const auto &) { return t < 0.5 ? (1.0 - easeOutBounce(1.0 - 2.0 * t)) / 2.0 : (1.0 + easeOutBounce(2.0 * t - 1.0)) / 2.0; }},
            {"ease_out_in_bounce", [](double t, const auto &) { return t < 0.5 ? easeOutBounce(2.0 * t) / 2.0 : (1.0 - easeOutBounce(1.0 - 2.0 * (t - 0.5))) / 2.0 + 0.5; }},
            {"custom", [](double x, const auto &p) {
                 double prevX = 0, prevY = 0;
                 for (size_t i = 0; i < p.size(); i += 6) {
                     double cp1x = p[i], cp1y = p[i + 1], cp2x = p[i + 2], cp2y = p[i + 3], endX = p[i + 4], endY = p[i + 5];
                     if (x <= endX || i + 6 >= p.size()) {
                         double range = endX - prevX;
                         if (range < 1e-6)
                             return endY;
                         double n_cp1x = (cp1x - prevX) / range, n_cp2x = (cp2x - prevX) / range, n_x = (x - prevX) / range;
                         double t = solveBezierT(n_x, n_cp1x, n_cp2x);
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
        auto getModeParams = [](const QVariant &v) { return v.toMap().value("modeParams").toMap(); };
        auto getBezierParams = [](const QVariant &v) -> std::vector<double> {
            const auto map = v.toMap();
            if (map.contains("points")) {
                QVariantList lst = map.value("points").toList();
                std::vector<double> pts;
                for (const auto &val : lst)
                    pts.push_back(val.toDouble());
                return pts;
            }
            return {map.value("bzx1", 0.33).toDouble(), map.value("bzy1", 0.0).toDouble(), map.value("bzx2", 0.66).toDouble(), map.value("bzy2", 1.0).toDouble(), 1.0, 1.0};
        };

        if (frame <= getFrame(track.front()))
            return getValue(track.front());
        if (frame >= getFrame(track.back()))
            return getValue(track.back());

        const bool numeric = fallback.canConvert<double>();
        for (int i = 0; i < track.size() - 1; ++i) {
            const int f0 = getFrame(track[i]), f1 = getFrame(track[i + 1]);
            if (frame < f0 || frame > f1)
                continue;
            const QVariant v0 = getValue(track[i]), v1 = getValue(track[i + 1]);
            const double tRaw = (frame - f0) / double(f1 - f0);
            QString type = getInterp(track[i]);
            const QVariantMap modeParams = getModeParams(track[i]);

            if (type == "none")
                return v0;
            if (v0.typeId() == QMetaType::QString && v1.typeId() == QMetaType::QString) {
                QColor c0(v0.toString()), c1(v1.typeId() == QMetaType::QString ? v1.toString() : v0.toString());
                if (c0.isValid() && c1.isValid()) {
                    std::vector<double> params;
                    if (type == "custom")
                        params = getBezierParams(track[i]);
                    if (!easingFunctions().contains(type))
                        type = "linear";
                    const double t = easingFunctions().value(type)(tRaw, params);
                    return QColor(static_cast<int>(c0.red() + (c1.red() - c0.red()) * t), static_cast<int>(c0.green() + (c1.green() - c0.green()) * t), static_cast<int>(c0.blue() + (c1.blue() - c0.blue()) * t),
                                  static_cast<int>(c0.alpha() + (c1.alpha() - c0.alpha()) * t))
                        .name(QColor::HexArgb);
                }
            }
            if (!numeric || !v0.canConvert<double>() || !v1.canConvert<double>())
                return v0;
            const double a = v0.toDouble(), b = v1.toDouble();
            if (type == "random") {
                const int stepFrames = std::max(1, modeParams.value("stepFrames", 1).toInt()), stepIndex = (frame - f0) / stepFrames;
                const quint32 seed = qHash(f0) ^ qHash(f1) ^ qHash(stepIndex) ^ qHash(static_cast<qint64>(a * 1000)) ^ qHash(static_cast<qint64>(b * 1000));
                return std::min(a, b) + (std::max(a, b) - std::min(a, b)) * (double(seed % 1000000u) / 999999.0);
            }
            if (type == "alternate") {
                const int stepFrames = std::max(1, modeParams.value("stepFrames", 1).toInt());
                return ((frame - f0) / stepFrames % 2 == 0) ? a : b;
            }
            std::vector<double> params;
            if (type == "custom")
                params = getBezierParams(track[i]);
            if (!easingFunctions().contains(type))
                type = "linear";
            return a + (b - a) * easingFunctions().value(type)(tRaw, params);
        }
        return getValue(track.back());
    }

    static QVariantMap normalizeTrackForDuration(const QVariant &rawTrack, const QVariant &fallback, int durationFrames) {
        const int maxFrame = std::max(0, durationFrames - 1);
        if (isStructuredTrack(rawTrack)) {
            QVariantMap raw = rawTrack.toMap();
            QVariantMap start = raw.value("start").toMap();
            QVariantList points = raw.value("points").toList(), nextPoints;
            start["frame"] = 0;
            if (!start.contains("value"))
                start["value"] = fallback;

            // 明示的終了点が範囲内にある場合のみ評価に含める（フレーム位置は変更しない）
            const bool inRangeEnd = hasExplicitEnd(rawTrack) && raw.value("end").toMap().value("frame").toInt() > 0 && raw.value("end").toMap().value("frame").toInt() <= maxFrame;
            const int ceiling = inRangeEnd ? raw.value("end").toMap().value("frame").toInt() : durationFrames;

            for (const auto &v : points) {
                const int f = v.toMap().value("frame").toInt();
                if (f > 0 && f < ceiling)
                    nextPoints.append(v);
            }
            QVariantMap out;
            out["start"] = start;
            out["points"] = sortPoints(nextPoints);
            if (inRangeEnd)
                out["end"] = raw.value("end"); // フレーム位置はユーザー設定値のまま
            return out;
        }
        // レガシーリスト形式（end なし）
        QVariantList legacy = sortPoints(rawTrack.toList()), points;
        QVariantMap start = makePoint(0, legacy.isEmpty() ? fallback : evaluateTrack(legacy, 0, fallback), "linear");
        for (const auto &v : legacy) {
            const int f = v.toMap().value("frame").toInt();
            if (f > 0 && f < durationFrames)
                points.append(v);
        }
        QVariantMap out;
        out["start"] = start;
        out["points"] = sortPoints(points);
        return out;
    }

  public:
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString category READ category CONSTANT)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantMap params READ params NOTIFY paramsChanged)
    Q_PROPERTY(QString qmlSource READ qmlSource CONSTANT)
    Q_PROPERTY(QVariantMap keyframeTracks READ keyframeTracks NOTIFY keyframeTracksChanged)
    Q_PROPERTY(QVariantMap uiDefinition READ uiDefinition CONSTANT)

    explicit EffectModel(const QString &id, const QString &name, const QString &category = "filter", const QVariantMap &params = {}, const QString &qmlSource = "", const QVariantMap &uiDef = {}, QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_name(name), m_category(category), m_enabled(true), m_params(params), m_qmlSource(qmlSource), m_uiDefinition(uiDef) {
        for (auto it = m_params.begin(); it != m_params.end(); ++it) {
            QVariantMap track;
            QVariantMap start;
            start["frame"] = 0;
            start["value"] = it.value();
            start["interp"] = "none";
            track["start"] = start;
            // end は設定しない（任意終了点の哲学）
            track["points"] = QVariantList();
            m_keyframeTracks[it.key()] = track;
        }
    }

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString category() const { return m_category; }
    bool isEnabled() const { return m_enabled; }
    QVariantMap params() const { return m_params; }
    QString qmlSource() const { return m_qmlSource; }
    QVariantMap keyframeTracks() const { return m_keyframeTracks; }
    QVariantMap uiDefinition() const { return m_uiDefinition; }

    EffectModel *clone() const {
        auto *copy = new EffectModel(m_id, m_name, m_category, m_params, m_qmlSource, m_uiDefinition);
        copy->m_enabled = m_enabled;
        copy->m_keyframeTracks = m_keyframeTracks;
        copy->m_lastDuration = m_lastDuration;
        return copy;
    }

    Q_INVOKABLE QVariantList keyframeListForUi(const QString &paramName) const {
        invalidateCache(paramName);
        const QVariant raw = m_keyframeTracks.value(paramName);
        if (isStructuredTrack(raw))
            return flattenStructuredTrack(raw.toMap());
        QVariantList list = raw.toList();
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("frame").toInt() < b.toMap().value("frame").toInt(); });
        return list;
    }

    Q_INVOKABLE bool isEndpointFrame(const QString &paramName, int frame) const {
        const QVariant raw = m_keyframeTracks.value(paramName);
        const int startFrame = isStructuredTrack(raw) ? raw.toMap().value("start").toMap().value("frame").toInt() : 0;
        if (frame == startFrame)
            return true;
        if (hasExplicitEnd(raw)) {
            const int endFrame = raw.toMap().value("end").toMap().value("frame").toInt();
            if (frame == endFrame)
                return true;
        }
        return false;
    }
    Q_INVOKABLE bool hasExplicitEndPoint(const QString &paramName) const { return hasExplicitEnd(m_keyframeTracks.value(paramName)); }

    Q_INVOKABLE void syncTrackEndpoints(int durationFrames) {
        m_resolvedCache.clear();
        m_lastDuration = durationFrames;
        // 未初期化パラメータのみ {start, points} で初期化する
        // 既存トラックは変更しない（中間点の位置を保持）
        for (auto it = m_params.begin(); it != m_params.end(); ++it) {
            if (!m_keyframeTracks.contains(it.key()) || !isStructuredTrack(m_keyframeTracks.value(it.key()))) {
                QVariantMap start;
                start["frame"] = 0;
                start["value"] = it.value();
                start["interp"] = "none";
                QVariantMap track;
                track["start"] = start;
                track["points"] = QVariantList();
                m_keyframeTracks[it.key()] = track;
            }
        }
        emit keyframeTracksChanged();
    }

    Q_INVOKABLE QVariantMap splitTracks(int firstHalfDuration, int originalDuration) {
        m_resolvedCache.clear();
        QVariantMap secondHalfTracks;
        if (originalDuration < 1)
            return secondHalfTracks;

        const int firstEndFrame = std::max(0, firstHalfDuration - 1);
        const int secondHalfDur = std::max(1, originalDuration - firstHalfDuration);
        const int secondEndFrame = std::max(0, secondHalfDur - 1);
        QVariantMap currentTracks = m_keyframeTracks;

        for (auto it = m_params.begin(); it != m_params.end(); ++it) {
            const QString key = it.key();
            const QVariant fallback = it.value();
            QVariantMap track = normalizeTrackForDuration(currentTracks.value(key), fallback, originalDuration);
            QVariantList flat = flattenStructuredTrack(track);
            QVariantMap start = track.value("start").toMap();
            QVariantList points = track.value("points").toList();
            const bool originalHasEnd = hasExplicitEnd(m_keyframeTracks.value(key));

            // 前半トラック
            QVariantMap firstTrack;
            QVariantList firstPoints;
            for (const auto &v : points) {
                const int f = v.toMap().value("frame").toInt();
                if (f > 0 && f < firstEndFrame)
                    firstPoints.append(v.toMap());
            }
            firstTrack["start"] = start;
            firstTrack["points"] = firstPoints;
            if (originalHasEnd) {
                QVariantMap firstEnd;
                firstEnd["frame"] = firstEndFrame;
                firstEnd["value"] = evaluateTrack(flat, firstEndFrame, fallback);
                firstEnd["interp"] = m_keyframeTracks.value(key).toMap().value("end").toMap().value("interp", "none");
                firstTrack["end"] = firstEnd;
            }
            currentTracks[key] = firstTrack;

            // 後半トラック
            QVariantMap secondTrack;
            QVariantMap secondStart;
            secondStart["frame"] = 0;
            secondStart["value"] = evaluateTrack(flat, firstHalfDuration, fallback);
            secondStart["interp"] = start.value("interp", "none");
            QVariantList secondPoints;
            for (const auto &v : points) {
                auto m = v.toMap();
                const int f = m.value("frame").toInt();
                if (f > firstHalfDuration && f < std::max(0, originalDuration - 1)) {
                    m["frame"] = f - firstHalfDuration;
                    const int nf = m.value("frame").toInt();
                    if (nf > 0 && nf < secondEndFrame)
                        secondPoints.append(m);
                }
            }
            secondTrack["start"] = secondStart;
            secondTrack["points"] = secondPoints;
            if (originalHasEnd) {
                QVariantMap secondEnd;
                secondEnd["frame"] = secondEndFrame;
                secondEnd["value"] = evaluateTrack(flat, std::max(0, originalDuration - 1), fallback);
                secondEnd["interp"] = m_keyframeTracks.value(key).toMap().value("end").toMap().value("interp", "none");
                secondTrack["end"] = secondEnd;
            }
            secondHalfTracks[key] = secondTrack;
        }
        m_keyframeTracks = currentTracks;
        emit keyframeTracksChanged();
        return secondHalfTracks;
    }

    // Must be public to be invokable from QML
    Q_INVOKABLE QStringList availableEasings() const {
        QStringList keys;
        keys << "none";
        auto funcs = easingFunctions();
        for (auto it = funcs.begin(); it != funcs.end(); ++it)
            keys << it.key();
        keys << "random" << "alternate";
        return keys;
    }

    void setEnabled(bool e) {
        m_resolvedCache.clear();
        if (m_enabled != e) {
            m_enabled = e;
            emit enabledChanged();
        }
    }

    Q_INVOKABLE void setParam(const QString &key, const QVariant &val) {
        invalidateCache(key);
        if (m_params[key] != val) {
            m_params[key] = val;
            emit paramsChanged();
            emit paramChanged(key, val);
        }
    }

    Q_INVOKABLE void setKeyframe(const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
        invalidateCache(paramName);
        const QVariant fallback = m_params.value(paramName);
        QVariantMap track = normalizeTrackForDuration(m_keyframeTracks.value(paramName), fallback, inferredDurationForTrack(m_keyframeTracks.value(paramName)));

        // isEnd オプションが true の場合、指定フレームを終了点として明示的に生成
        if (options.value("isEnd").toBool()) {
            QVariantMap end;
            end["frame"] = frame;
            end["value"] = value;
            end["interp"] = options.value("interp", "none").toString();
            track["end"] = end;
            m_keyframeTracks[paramName] = track;
            emit keyframeTracksChanged();
            return;
        }

        QVariantMap start = track.value("start").toMap();
        QVariantList points = track.value("points").toList();
        const QString interp = options.value("interp", "none").toString();

        const int startFrame = start.value("frame").toInt();
        const bool trackHasEnd = hasExplicitEnd(m_keyframeTracks.value(paramName));
        const int endFrame = trackHasEnd ? m_keyframeTracks.value(paramName).toMap().value("end").toMap().value("frame").toInt() : std::numeric_limits<int>::max();

        if (frame <= startFrame) {
            start["value"] = value;
            start["interp"] = options.value("interp", start.value("interp", "none"));
            track["start"] = start;
            m_keyframeTracks[paramName] = track;
            emit keyframeTracksChanged();
            return;
        }

        // 終了点が明示的に存在する場合のみ終了点への更新を行う
        if (trackHasEnd && frame >= endFrame) {
            QVariantMap end = m_keyframeTracks.value(paramName).toMap().value("end").toMap();
            end["value"] = value;
            end["interp"] = options.value("interp", end.value("interp", "none"));
            track["end"] = end;
            m_keyframeTracks[paramName] = track;
            emit keyframeTracksChanged();
            return;
        }

        QVariantMap kf;
        kf["frame"] = frame;
        kf["value"] = value;
        kf["interp"] = interp;
        if (options.contains("points"))
            kf["points"] = options.value("points");
        if (options.contains("modeParams"))
            kf["modeParams"] = options.value("modeParams");

        bool updated = false;
        for (int i = 0; i < points.size(); ++i) {
            if (points[i].toMap().value("frame").toInt() == frame) {
                points[i] = kf;
                updated = true;
                break;
            }
        }
        if (!updated)
            points.append(kf);

        track["points"] = sortPoints(points);
        m_keyframeTracks[paramName] = track;
        emit keyframeTracksChanged();
    }

    Q_INVOKABLE void removeKeyframe(const QString &paramName, int frame) {
        invalidateCache(paramName);
        const QVariant fallback = m_params.value(paramName);
        QVariantMap track = normalizeTrackForDuration(m_keyframeTracks.value(paramName), fallback, inferredDurationForTrack(m_keyframeTracks.value(paramName)));

        const int startFrame = track.value("start").toMap().value("frame").toInt();
        // 開始点は削除不可（終了点は明示的な場合のみ保護）
        if (frame <= startFrame)
            return;
        if (hasExplicitEnd(m_keyframeTracks.value(paramName))) {
            const int endFrame = m_keyframeTracks.value(paramName).toMap().value("end").toMap().value("frame").toInt();
            if (frame >= endFrame)
                return;
        }

        QVariantList points = track.value("points").toList(), next;
        for (const auto &v : points)
            if (v.toMap().value("frame").toInt() != frame)
                next.append(v);
        track["points"] = next;
        m_keyframeTracks[paramName] = track;
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

    Q_INVOKABLE QVariant evaluatedParam(const QString &paramName, int frame, double fps = 60.0) const {
        const QVariant fallback = m_params.value(paramName);
        if (!m_keyframeTracks.contains(paramName))
            return fallback;

        // 1. キャッシュされた解決済みトラックを使用して評価（ヒープ割り当てを回避）
        if (!m_resolvedCache.contains(paramName)) {
            const QVariant raw = m_keyframeTracks.value(paramName);
            if (isStructuredTrack(raw)) {
                int d = (m_lastDuration > 0) ? m_lastDuration : inferredDurationForTrack(raw);
                QVariantMap normalized = normalizeTrackForDuration(raw, fallback, d);
                m_resolvedCache.insert(paramName, flattenStructuredTrack(normalized));
            } else {
                m_resolvedCache.insert(paramName, sortPoints(raw.toList()));
            }
        }
        QVariant baseValue = evaluateTrack(m_resolvedCache.value(paramName), frame, fallback);

        // 2. パラメータ自体がエクスプレッション定義かチェック（簡易実装）
        QString strVal = m_params.value(paramName).toString();
        if (strVal.startsWith("=")) {
            // "=time*100" -> "time*100"
            std::string expr = strVal.mid(1).toStdString();
            double time = (fps > 0.0) ? frame / fps : 0.0;
            return Rina::Scripting::LuaHost::instance().evaluate(expr, time, 0, baseValue.toDouble());
        }

        return baseValue;
    }

    void setKeyframeTracks(const QVariantMap &tracks) {
        m_keyframeTracks = tracks;
        emit keyframeTracksChanged();
    }

    void invalidateCache(const QString &paramName) const {
        if (!paramName.isEmpty()) {
            m_resolvedCache.remove(paramName);
        }
    }

  signals:
    void enabledChanged();
    void paramsChanged();
    void paramChanged(const QString &key, const QVariant &val);
    void keyframeTracksChanged();

  private:
    QString m_id;
    QString m_name;
    QString m_category;
    bool m_enabled;
    QVariantMap m_params;
    QString m_qmlSource;
    QVariantMap m_uiDefinition;
    QVariantMap m_keyframeTracks; // パラメータ名 -> QVariantList[{frame,value,interp}]

    mutable int m_lastDuration = -1;
    mutable QHash<QString, QVariantList> m_resolvedCache;
};
} // namespace Rina::UI