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

// ── クリップ分割時のベジェ曲線 de Casteljau 分割ヘルパー ──────────────────────

// 単一ベジェセグメントを正規化パラメータ t で de Casteljau 分割する
// 入力: P0=(p0x,p0y) P1=(cp1x,cp1y) P2=(cp2x,cp2y) P3=(p3x,p3y), t∈[0,1]
// 出力: 前半 [p0..mid] の CP 配列 {q1x,q1y,r1x,r1y,midX,midY}
//       後半 [mid..p3] の CP 配列 {r2x,r2y,q3x,q3y,p3x,p3y}
inline std::pair<std::array<double, 6>, std::array<double, 6>> splitBezierSegment(double p0x, double p0y, double p1x, double p1y, double p2x, double p2y, double p3x, double p3y, double t) noexcept {
    auto lp = [](double a, double b, double t) noexcept { return a + (b - a) * t; };
    const double q1x = lp(p0x, p1x, t), q1y = lp(p0y, p1y, t);
    const double q2x = lp(p1x, p2x, t), q2y = lp(p1y, p2y, t);
    const double q3x = lp(p2x, p3x, t), q3y = lp(p2y, p3y, t);
    const double r1x = lp(q1x, q2x, t), r1y = lp(q1y, q2y, t);
    const double r2x = lp(q2x, q3x, t), r2y = lp(q2y, q3y, t);
    const double mx = lp(r1x, r2x, t), my = lp(r1y, r2y, t);
    return {std::array<double, 6>{q1x, q1y, r1x, r1y, mx, my}, std::array<double, 6>{r2x, r2y, q3x, q3y, p3x, p3y}};
}

// custom bezier の points[] を分割点 t_split(0〜1, 正規化) で分割する
// 入力:  pts = [cp1x,cp1y,cp2x,cp2y,endX,endY, ...]  (6要素/セグメント, x/y 全て正規化)
// 出力:  {前半 points[], 後半 points[]}
//        前半は x を [0, t_split] に再正規化
//        後半は x を [0, 1-t_split] に再正規化
inline std::pair<QList<double>, QList<double>> splitCustomBezierPoints(const QList<double> &pts, double t_split) noexcept {
    QList<double> first, second;
    if (pts.size() < 6 || t_split <= 0.0 || t_split >= 1.0)
        return {first, second};

    double prevX = 0.0, prevY = 0.0;
    bool splitDone = false;

    for (int i = 0; i + 5 < pts.size(); i += 6) {
        const double cp1x = pts[i], cp1y = pts[i + 1];
        const double cp2x = pts[i + 2], cp2y = pts[i + 3];
        const double endX = pts[i + 4], endY = pts[i + 5];

        if (!splitDone && t_split <= endX + 1e-9) {
            // このセグメントが分割点を含む
            const double segRange = endX - prevX;
            if (segRange < 1e-9) {
                // 縮退セグメント: 評価値のみ
                splitDone = true;
                prevX = endX;
                prevY = endY;
                continue;
            }
            // セグメント内での t を計算
            const double t_seg = std::clamp((t_split - prevX) / segRange, 0.0, 1.0);
            auto [fArr, sArr] = splitBezierSegment(prevX, prevY, cp1x, cp1y, cp2x, cp2y, endX, endY, t_seg);
            // 前半セグメントを [0, t_split] に再正規化して追加
            if (t_split > 1e-9) {
                const double invFirst = 1.0 / t_split;
                first.append(fArr[0] * invFirst);
                first.append(fArr[1]);
                first.append(fArr[2] * invFirst);
                first.append(fArr[3]);
                first.append(fArr[4] * invFirst);
                first.append(fArr[5]);
            }
            // 後半セグメントを [0, 1-t_split] に再正規化して追加
            const double secondRange = 1.0 - t_split;
            if (secondRange > 1e-9) {
                const double invSecond = 1.0 / secondRange;
                second.append((sArr[0] - t_split) * invSecond);
                second.append(sArr[1]);
                second.append((sArr[2] - t_split) * invSecond);
                second.append(sArr[3]);
                second.append((sArr[4] - t_split) * invSecond);
                second.append(sArr[5]);
            }
            splitDone = true;
        } else if (!splitDone) {
            // 分割点より前のセグメント → 前半へ（x を [0,t_split] に再正規化）
            if (t_split > 1e-9) {
                const double inv = 1.0 / t_split;
                first.append(cp1x * inv);
                first.append(cp1y);
                first.append(cp2x * inv);
                first.append(cp2y);
                first.append(endX * inv);
                first.append(endY);
            }
        } else {
            // 分割点より後のセグメント → 後半へ（x を [0,1-t_split] に再正規化）
            const double secondRange = 1.0 - t_split;
            if (secondRange > 1e-9) {
                const double inv = 1.0 / secondRange;
                second.append((cp1x - t_split) * inv);
                second.append(cp1y);
                second.append((cp2x - t_split) * inv);
                second.append(cp2y);
                second.append((endX - t_split) * inv);
                second.append(endY);
            }
        }
        prevX = endX;
        prevY = endY;
    }
    return {first, second};
}

// キーフレームトラック 1 本を firstHalfDuration フレームで正確に分割する
// 戻り値: {前半トラック, 後半トラック}
// custom bezier 区間は de Casteljau 分割、標準 easing 区間は分割点での評価値を使用する
inline std::pair<QVariantMap, QVariantMap> splitTrackAt(const QVariantMap &track, const QVariant &fallback, int splitF) {
    const QVariantList flat = flattenStructuredTrack(track);
    const QVariantMap startKf = track.value(QStringLiteral("start")).toMap();

    // 前半トラック
    QVariantMap firstStart = startKf;
    QVariantList firstPoints;

    // 後半トラック
    QVariantMap secondStart;
    secondStart[QStringLiteral("frame")] = 0;
    secondStart[QStringLiteral("value")] = evaluateTrack(flat, splitF, fallback);

    // 後半先頭の interp は分割点を含む区間の interp を引き継ぐ
    QString splitInterp = startKf.value(QStringLiteral("interp"), QStringLiteral("none")).toString();

    QVariantList secondPoints;
    const int n = flat.size();

    for (int i = 0; i < n - 1; ++i) {
        const QVariantMap m0 = flat[i].toMap();
        const QVariantMap m1 = flat[i + 1].toMap();
        const int f0 = m0.value(QStringLiteral("frame")).toInt();
        const int f1 = m1.value(QStringLiteral("frame")).toInt();
        const QString interp = m0.value(QStringLiteral("interp"), QStringLiteral("none")).toString();

        if (f1 <= splitF) {
            // 区間全体が前半側: 次のキーフレーム（i+1 が末尾でない場合）を前半 points に追加
            if (i + 1 < n - 1)
                firstPoints.append(flat[i + 1]);
        } else if (f0 >= splitF) {
            // 区間全体が後半側: i>0 かつ f0>splitF の点を後半 points に追加
            if (f0 > splitF) {
                QVariantMap pt = m0;
                pt[QStringLiteral("frame")] = f0 - splitF;
                secondPoints.append(pt);
            }
        } else {
            // 分割点が区間内 (f0 < splitF < f1)
            splitInterp = interp;
            if (interp == QStringLiteral("custom")) {
                // de Casteljau 分割
                const double t_split = (f1 > f0) ? std::clamp(double(splitF - f0) / (f1 - f0), 0.0, 1.0) : 0.5;
                const QVariant rawPts = m0.value(QStringLiteral("points"), m0.value(QStringLiteral("bzx1")));
                QList<double> pts;
                if (m0.contains(QStringLiteral("points"))) {
                    for (const auto &v : m0.value(QStringLiteral("points")).toList())
                        pts.append(v.toDouble());
                } else {
                    // legacy 4点形式をセグメント形式に変換
                    pts = {m0.value(QStringLiteral("bzx1"), 0.33).toDouble(), m0.value(QStringLiteral("bzy1"), 0.0).toDouble(), m0.value(QStringLiteral("bzx2"), 0.66).toDouble(), m0.value(QStringLiteral("bzy2"), 1.0).toDouble(), 1.0, 1.0};
                }
                auto [firstPts, secondPts] = splitCustomBezierPoints(pts, t_split);
                // 前半末尾に分割点を追加
                if (!firstPts.isEmpty()) {
                    QVariantMap fp = m0;
                    fp[QStringLiteral("frame")] = splitF; // 前半末尾（後で firstEndFrame でクリップ）
                    QVariantList fPtsList;
                    for (double v : firstPts)
                        fPtsList.append(v);
                    fp[QStringLiteral("points")] = fPtsList;
                    fp.remove(QStringLiteral("bzx1"));
                    fp.remove(QStringLiteral("bzy1"));
                    fp.remove(QStringLiteral("bzx2"));
                    fp.remove(QStringLiteral("bzy2"));
                    // 分割点の value を設定
                    fp[QStringLiteral("value")] = evaluateTrack(flat, splitF, fallback);
                    firstPoints.append(fp);
                }
                // 後半先頭の interp は後半の制御点を持つ
                if (!secondPts.isEmpty()) {
                    QVariantList sPtsList;
                    for (double v : secondPts)
                        sPtsList.append(v);
                    secondStart[QStringLiteral("points")] = sPtsList;
                }
            } else {
                // 標準 easing: 分割点で値を確定。前半末尾は追加不要（startKf〜splitF まで）
                // 後半先頭の interp を引き継ぐ
            }
        }
    }

    secondStart[QStringLiteral("interp")] = splitInterp;

    QVariantMap ft, st;
    ft[QStringLiteral("start")] = firstStart;
    ft[QStringLiteral("points")] = firstPoints;
    st[QStringLiteral("start")] = secondStart;
    st[QStringLiteral("points")] = secondPoints;
    return {ft, st};
}

} // namespace Rina::Engine::Timeline::Interp
