#pragma once
#include <QColor>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "lua_host.hpp"

// フェーズ3 STEP1: EffectModel の private static 補間ロジックを engine 層に移植。
// EffectModel* を持たずにキーフレーム補間を実行するための純粋関数群。
// clip_effect_system.cpp のみが include する。

namespace Rina::Engine::Timeline::Interp {

using EasingFunction = std::function<double(double, const std::vector<double> &)>;

// ─── ベジェT解（ニュートン法）───────────────────────────────────────

inline double solveBezierT(double x, double x1, double x2) {
    if (x1 == x2 && x1 == x)
        return x;
    double t = x;
    for (int i = 0; i < 8; ++i) {
        const double om = 1.0 - t;
        const double cx = 3.0 * om * om * t * x1 + 3.0 * om * t * t * x2 + t * t * t;
        const double err = cx - x;
        if (std::abs(err) < 1e-5)
            return t;
        const double dx = 3.0 * om * om * x1 + 6.0 * om * t * (x2 - x1) + 3.0 * t * t * (1.0 - x2);
        if (std::abs(dx) < 1e-6)
            break;
        t -= err / dx;
    }
    return std::clamp(t, 0.0, 1.0);
}

// ─── イージング関数マップ（static local、スレッドセーフ）────────────

// bounce out コア計算（キャプチャ不可な static ラムダを回避するため自由関数として定義）
inline double bounceOut(double x) noexcept {
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
}

inline const QHash<QString, EasingFunction> &easingFunctions() {
    static const QHash<QString, EasingFunction> funcs = {
        {QStringLiteral("linear"), [](double t, const auto &) { return t; }},
        {QStringLiteral("ease_in_sine"), [](double t, const auto &) { return 1.0 - std::cos(t * M_PI / 2.0); }},
        {QStringLiteral("ease_out_sine"), [](double t, const auto &) { return std::sin(t * M_PI / 2.0); }},
        {QStringLiteral("ease_in_out_sine"), [](double t, const auto &) { return -(std::cos(M_PI * t) - 1.0) / 2.0; }},
        {QStringLiteral("ease_out_in_sine"), [](double t, const auto &) { return t < 0.5 ? std::sin(t * M_PI) / 2.0 : (1.0 - std::cos((t * 2.0 - 1.0) * M_PI / 2.0)) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_quad"), [](double t, const auto &) { return t * t; }},
        {QStringLiteral("ease_out_quad"), [](double t, const auto &) { return 1.0 - (1.0 - t) * (1.0 - t); }},
        {QStringLiteral("ease_in_out_quad"), [](double t, const auto &) { return t < 0.5 ? 2.0 * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) / 2.0; }},
        {QStringLiteral("ease_out_in_quad"), [](double t, const auto &) { return t < 0.5 ? (1.0 - (1.0 - 2.0 * t) * (1.0 - 2.0 * t)) / 2.0 : (2.0 * t - 1.0) * (2.0 * t - 1.0) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_cubic"), [](double t, const auto &) { return t * t * t; }},
        {QStringLiteral("ease_out_cubic"), [](double t, const auto &) { return 1.0 - std::pow(1.0 - t, 3.0); }},
        {QStringLiteral("ease_in_out_cubic"), [](double t, const auto &) { return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0; }},
        {QStringLiteral("ease_out_in_cubic"), [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 3.0)) / 2.0 : std::pow(2.0 * t - 1.0, 3.0) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_quart"), [](double t, const auto &) { return t * t * t * t; }},
        {QStringLiteral("ease_out_quart"), [](double t, const auto &) { return 1.0 - std::pow(1.0 - t, 4.0); }},
        {QStringLiteral("ease_in_out_quart"), [](double t, const auto &) { return t < 0.5 ? 8.0 * t * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 4.0) / 2.0; }},
        {QStringLiteral("ease_out_in_quart"), [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 4.0)) / 2.0 : std::pow(2.0 * t - 1.0, 4.0) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_quint"), [](double t, const auto &) { return t * t * t * t * t; }},
        {QStringLiteral("ease_out_quint"), [](double t, const auto &) { return 1.0 - std::pow(1.0 - t, 5.0); }},
        {QStringLiteral("ease_in_out_quint"), [](double t, const auto &) { return t < 0.5 ? 16.0 * t * t * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 5.0) / 2.0; }},
        {QStringLiteral("ease_out_in_quint"), [](double t, const auto &) { return t < 0.5 ? (1.0 - std::pow(1.0 - 2.0 * t, 5.0)) / 2.0 : std::pow(2.0 * t - 1.0, 5.0) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_expo"), [](double t, const auto &) { return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0); }},
        {QStringLiteral("ease_out_expo"), [](double t, const auto &) { return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t); }},
        {QStringLiteral("ease_in_out_expo"),
         [](double t, const auto &) -> double {
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0 : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
         }},
        {QStringLiteral("ease_out_in_expo"),
         [](double t, const auto &) -> double {
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             return t < 0.5 ? (1.0 - std::pow(2.0, -20.0 * t)) / 2.0 : std::pow(2.0, 20.0 * t - 20.0) / 2.0 + 0.5;
         }},
        {QStringLiteral("ease_in_circ"), [](double t, const auto &) { return 1.0 - std::sqrt(1.0 - t * t); }},
        {QStringLiteral("ease_out_circ"), [](double t, const auto &) { return std::sqrt(1.0 - (t - 1.0) * (t - 1.0)); }},
        {QStringLiteral("ease_in_out_circ"), [](double t, const auto &) { return t < 0.5 ? (1.0 - std::sqrt(1.0 - 4.0 * t * t)) / 2.0 : (std::sqrt(1.0 - std::pow(-2.0 * t + 2.0, 2.0)) + 1.0) / 2.0; }},
        {QStringLiteral("ease_out_in_circ"), [](double t, const auto &) { return t < 0.5 ? std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0)) / 2.0 : (1.0 - std::sqrt(1.0 - (2.0 * t - 1.0) * (2.0 * t - 1.0))) / 2.0 + 0.5; }},
        {QStringLiteral("ease_in_back"),
         [](double t, const auto &) {
             constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
             return c3 * t * t * t - c1 * t * t;
         }},
        {QStringLiteral("ease_out_back"),
         [](double t, const auto &) {
             constexpr double c1 = 1.70158, c3 = 1.70158 + 1.0;
             return 1.0 + c3 * std::pow(t - 1.0, 3.0) + c1 * std::pow(t - 1.0, 2.0);
         }},
        {QStringLiteral("ease_in_out_back"),
         [](double t, const auto &) {
             constexpr double c2 = 1.70158 * 1.525;
             return t < 0.5 ? (std::pow(2.0 * t, 2.0) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0 : (std::pow(2.0 * t - 2.0, 2.0) * ((c2 + 1.0) * (2.0 * t - 2.0) + c2) + 2.0) / 2.0;
         }},
        {QStringLiteral("ease_out_in_back"),
         [](double t, const auto &) {
             constexpr double c1 = 1.70158, c3 = c1 + 1.0;
             auto eout = [&](double u) { return 1.0 + c3 * std::pow(u - 1.0, 3.0) + c1 * std::pow(u - 1.0, 2.0); };
             auto ein = [&](double u) { return c3 * u * u * u - c1 * u * u; };
             return t < 0.5 ? eout(2.0 * t) / 2.0 : ein(2.0 * t - 1.0) / 2.0 + 0.5;
         }},
        {QStringLiteral("ease_in_elastic"),
         [](double t, const auto &) -> double {
             constexpr double c4 = (2.0 * M_PI) / 3.0;
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * c4);
         }},
        {QStringLiteral("ease_out_elastic"),
         [](double t, const auto &) -> double {
             constexpr double c4 = (2.0 * M_PI) / 3.0;
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * c4) + 1.0;
         }},
        {QStringLiteral("ease_in_out_elastic"),
         [](double t, const auto &) -> double {
             constexpr double c5 = (2.0 * M_PI) / 4.5;
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             return t < 0.5 ? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 : (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 + 1.0;
         }},
        {QStringLiteral("ease_out_in_elastic"),
         [](double t, const auto &) -> double {
             constexpr double c4 = (2.0 * M_PI) / 3.0;
             if (t == 0.0)
                 return 0.0;
             if (t == 1.0)
                 return 1.0;
             auto eout = [&](double u) { return std::pow(2.0, -10.0 * u) * std::sin((u * 10.0 - 0.75) * c4) + 1.0; };
             auto ein = [&](double u) -> double {
                 if (u == 0.0)
                     return 0.0;
                 if (u == 1.0)
                     return 1.0;
                 return -std::pow(2.0, 10.0 * u - 10.0) * std::sin((u * 10.0 - 10.75) * c4);
             };
             return t < 0.5 ? eout(2.0 * t) / 2.0 : ein(2.0 * t - 1.0) / 2.0 + 0.5;
         }},
        {QStringLiteral("ease_out_bounce"), [](double t, const auto &) { return bounceOut(t); }},
        {QStringLiteral("ease_in_bounce"), [](double t, const auto &) { return 1.0 - bounceOut(1.0 - t); }},
        {QStringLiteral("ease_in_out_bounce"), [](double t, const auto &) { return t < 0.5 ? (1.0 - bounceOut(1.0 - 2.0 * t)) / 2.0 : (1.0 + bounceOut(2.0 * t - 1.0)) / 2.0; }},
        {QStringLiteral("ease_out_in_bounce"), [](double t, const auto &) { return t < 0.5 ? bounceOut(2.0 * t) / 2.0 : (1.0 - bounceOut(1.0 - 2.0 * (t - 0.5))) / 2.0 + 0.5; }},
        {QStringLiteral("custom"),
         [](double x, const auto &p) -> double {
             double prevX = 0.0, prevY = 0.0;
             for (size_t i = 0; i + 5 < p.size(); i += 6) {
                 double cp1x = p[i], cp1y = p[i + 1], cp2x = p[i + 2], cp2y = p[i + 3], endX = p[i + 4], endY = p[i + 5];
                 if (x <= endX || i + 6 >= p.size()) {
                     double range = endX - prevX;
                     if (range < 1e-6)
                         return endY;
                     double nx = (x - prevX) / range;
                     double t = solveBezierT(nx, (cp1x - prevX) / range, (cp2x - prevX) / range);
                     return std::pow(1.0 - t, 3) * prevY + 3 * std::pow(1.0 - t, 2) * t * cp1y + 3 * (1.0 - t) * t * t * cp2y + t * t * t * endY;
                 }
                 prevX = endX;
                 prevY = endY;
             }
             return x;
         }},
    };
    return funcs;
}

// ─── トラックヘルパー ─────────────────────────────────────────────

inline bool isStructuredTrack(const QVariant &raw) {
    const QVariantMap m = raw.toMap();
    return m.contains(QStringLiteral("start")) && m.contains(QStringLiteral("points"));
}

inline QVariantList sortPoints(QVariantList pts) {
    std::sort(pts.begin(), pts.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value(QStringLiteral("frame")).toInt() < b.toMap().value(QStringLiteral("frame")).toInt(); });
    return pts;
}

inline QVariantMap makePoint(int frame, const QVariant &value, const QString &interp = QStringLiteral("none")) {
    QVariantMap p;
    p[QStringLiteral("frame")] = frame;
    p[QStringLiteral("value")] = value;
    p[QStringLiteral("interp")] = interp;
    return p;
}

inline int inferredDurationForTrack(const QVariant &raw) {
    auto maxFrame = [](const QVariantList &list) {
        int mx = 0;
        for (const auto &v : list)
            mx = std::max(mx, v.toMap().value(QStringLiteral("frame")).toInt());
        return std::max(1, mx + 1);
    };
    if (isStructuredTrack(raw))
        return maxFrame(raw.toMap().value(QStringLiteral("points")).toList());
    const QVariantList l = raw.toList();
    return l.isEmpty() ? 1 : maxFrame(l);
}

inline QVariantList flattenStructuredTrack(const QVariantMap &track) {
    QVariantList out;
    out.append(track.value(QStringLiteral("start")));
    for (const auto &v : sortPoints(track.value(QStringLiteral("points")).toList()))
        out.append(v);
    return out;
}

inline QVariantMap normalizeTrackForDuration(const QVariant &rawTrack, const QVariant &fallback, int durationFrames) {
    if (isStructuredTrack(rawTrack)) {
        QVariantMap raw = rawTrack.toMap();
        QVariantMap start = raw.value(QStringLiteral("start")).toMap();
        QVariantList pts = raw.value(QStringLiteral("points")).toList(), next;
        start[QStringLiteral("frame")] = 0;
        if (!start.contains(QStringLiteral("value")))
            start[QStringLiteral("value")] = fallback;
        for (const auto &v : std::as_const(pts)) {
            const int f = v.toMap().value(QStringLiteral("frame")).toInt();
            if (f > 0 && f <= durationFrames)
                next.append(v);
        }
        QVariantMap out;
        out[QStringLiteral("start")] = start;
        out[QStringLiteral("points")] = sortPoints(next);
        return out;
    }
    QVariantList legacy = sortPoints(rawTrack.toList()), pts;
    QVariantMap start = makePoint(0, legacy.isEmpty() ? fallback : legacy.front().toMap().value(QStringLiteral("value")), QStringLiteral("linear"));
    for (const auto &v : std::as_const(legacy)) {
        const int f = v.toMap().value(QStringLiteral("frame")).toInt();
        if (f > 0 && f < durationFrames)
            pts.append(v);
    }
    QVariantMap out;
    out[QStringLiteral("start")] = start;
    out[QStringLiteral("points")] = sortPoints(pts);
    return out;
}

// ─── キーフレームトラック評価（全イージング対応）───────────────────

inline QVariant evaluateTrack(const QVariantList &track, int frame, const QVariant &fallback) {
    if (track.isEmpty())
        return fallback;

    auto getFrame = [](const QVariant &v) { return v.toMap().value(QStringLiteral("frame")).toInt(); };
    auto getValue = [](const QVariant &v) { return v.toMap().value(QStringLiteral("value")); };
    auto getInterp = [](const QVariant &v) { return v.toMap().value(QStringLiteral("interp")).toString(); };
    auto getModeP = [](const QVariant &v) { return v.toMap().value(QStringLiteral("modeParams")).toMap(); };
    auto getBezierP = [](const QVariant &v) -> std::vector<double> {
        const auto map = v.toMap();
        if (map.contains(QStringLiteral("points"))) {
            std::vector<double> pts;
            for (const auto &val : map.value(QStringLiteral("points")).toList())
                pts.push_back(val.toDouble());
            return pts;
        }
        return {map.value(QStringLiteral("bzx1"), 0.33).toDouble(), map.value(QStringLiteral("bzy1"), 0.0).toDouble(), map.value(QStringLiteral("bzx2"), 0.66).toDouble(), map.value(QStringLiteral("bzy2"), 1.0).toDouble(), 1.0, 1.0};
    };

    if (frame <= getFrame(track.front()))
        return getValue(track.front());
    if (frame >= getFrame(track.back()))
        return getValue(track.back());

    for (int i = 0; i < track.size() - 1; ++i) {
        const int f0 = getFrame(track[i]), f1 = getFrame(track[i + 1]);
        if (frame < f0 || frame > f1)
            continue;

        const QVariant v0 = getValue(track[i]), v1 = getValue(track[i + 1]);
        const double tRaw = (frame - f0) / double(f1 - f0);
        QString type = getInterp(track[i]);
        const QVariantMap modeParams = getModeP(track[i]);

        if (type == QStringLiteral("none"))
            return (frame < f1) ? v0 : v1;

        if (v0.typeId() == QMetaType::QString && v1.typeId() == QMetaType::QString) {
            QColor c0(v0.toString()), c1(v1.toString());
            if (c0.isValid() && c1.isValid()) {
                std::vector<double> params;
                if (type == QStringLiteral("custom"))
                    params = getBezierP(track[i]);
                if (!easingFunctions().contains(type))
                    type = QStringLiteral("linear");
                const double t = easingFunctions().value(type)(tRaw, params);
                return QColor(static_cast<int>(c0.red() + (c1.red() - c0.red()) * t), static_cast<int>(c0.green() + (c1.green() - c0.green()) * t), static_cast<int>(c0.blue() + (c1.blue() - c0.blue()) * t),
                              static_cast<int>(c0.alpha() + (c1.alpha() - c0.alpha()) * t))
                    .name(QColor::HexArgb);
            }
        }

        if (!v0.canConvert<double>() || !v1.canConvert<double>())
            return v0;

        const double a = v0.toDouble(), b = v1.toDouble();

        if (type == QStringLiteral("random")) {
            const int step = std::max(1, modeParams.value(QStringLiteral("stepFrames"), 1).toInt());
            const int idx = (frame - f0) / step;
            const quint32 seed = qHash(f0) ^ qHash(f1) ^ qHash(idx) ^ qHash(static_cast<qint64>(a * 1000)) ^ qHash(static_cast<qint64>(b * 1000));
            return std::min(a, b) + (std::max(a, b) - std::min(a, b)) * (double(seed % 1000000u) / 999999.0);
        }
        if (type == QStringLiteral("alternate")) {
            const int step = std::max(1, modeParams.value(QStringLiteral("stepFrames"), 1).toInt());
            return ((frame - f0) / step % 2 == 0) ? a : b;
        }

        std::vector<double> params;
        if (type == QStringLiteral("custom"))
            params = getBezierP(track[i]);
        if (!easingFunctions().contains(type))
            type = QStringLiteral("linear");
        return a + (b - a) * easingFunctions().value(type)(tRaw, params);
    }
    return getValue(track.back());
}

// ─── エントリポイント: 単一パラメータ評価 ────────────────────────

inline QVariant evaluateParam(const QVariantMap &keyframeTracks, const QVariantMap &params, const QString &paramName, int relFrame, int durationFrames, double fps = 60.0) {
    const QVariant fallback = params.value(paramName);

    const QString strVal = fallback.toString();
    if (strVal.startsWith(QLatin1Char('='))) {
        const std::string expr = strVal.mid(1).toStdString();
        const double time = (fps > 0.0) ? relFrame / fps : 0.0;
        return Rina::Scripting::LuaHost::instance().evaluate(expr, time, 0, fallback.toDouble());
    }

    if (!keyframeTracks.contains(paramName))
        return fallback;

    const QVariant rawTrack = keyframeTracks.value(paramName);
    QVariantList resolved;
    if (isStructuredTrack(rawTrack)) {
        const int d = (durationFrames > 0) ? durationFrames : inferredDurationForTrack(rawTrack);
        resolved = flattenStructuredTrack(normalizeTrackForDuration(rawTrack, fallback, d));
    } else {
        resolved = sortPoints(rawTrack.toList());
    }

    return evaluateTrack(resolved, relFrame, fallback);
}

} // namespace Rina::Engine::Timeline::Interp
