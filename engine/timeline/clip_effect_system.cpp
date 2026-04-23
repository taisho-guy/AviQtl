#include "clip_effect_system.hpp"
#include <QColor>
#include <algorithm>
#include <cmath>

namespace {

// ── キーフレームトラック操作ヘルパー（EffectModel の private static を純粋関数として移植）──

bool isStructuredTrack(const QVariant &raw) {
    const QVariantMap m = raw.toMap();
    return m.contains(QStringLiteral("start")) && m.contains(QStringLiteral("points"));
}

QVariantList sortPoints(QVariantList points) {
    std::sort(points.begin(), points.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value(QStringLiteral("frame")).toInt() < b.toMap().value(QStringLiteral("frame")).toInt(); });
    return points;
}

QVariantMap makePoint(int frame, const QVariant &value, const QString &interp = QStringLiteral("none")) {
    QVariantMap p;
    p[QStringLiteral("frame")] = frame;
    p[QStringLiteral("value")] = value;
    p[QStringLiteral("interp")] = interp;
    return p;
}

int inferredDurationForTrack(const QVariant &raw) {
    if (isStructuredTrack(raw)) {
        const QVariantList points = raw.toMap().value(QStringLiteral("points")).toList();
        int maxFrame = 0;
        for (const auto &v : std::as_const(points))
            maxFrame = std::max(maxFrame, v.toMap().value(QStringLiteral("frame")).toInt());
        return std::max(1, maxFrame + 1);
    }
    const QVariantList list = raw.toList();
    if (list.isEmpty())
        return 1;
    int maxFrame = 0;
    for (const auto &v : std::as_const(list))
        maxFrame = std::max(maxFrame, v.toMap().value(QStringLiteral("frame")).toInt());
    return std::max(1, maxFrame + 1);
}

// レガシーリスト形式でのフレーム線形補間（normalizeTrackForDuration 内部で使用）
QVariant evaluateTrackLinear(const QVariantList &track, int frame, const QVariant &fallback) {
    if (track.isEmpty())
        return fallback;
    auto getFrame = [](const QVariant &v) { return v.toMap().value(QStringLiteral("frame")).toInt(); };
    auto getValue = [](const QVariant &v) { return v.toMap().value(QStringLiteral("value")); };
    if (frame <= getFrame(track.front()))
        return getValue(track.front());
    if (frame >= getFrame(track.back()))
        return getValue(track.back());
    for (int i = 0; i < track.size() - 1; ++i) {
        const int f0 = getFrame(track[i]), f1 = getFrame(track[i + 1]);
        if (frame < f0 || frame > f1)
            continue;
        const QVariant v0 = getValue(track[i]), v1 = getValue(track[i + 1]);
        if (!v0.canConvert<double>() || !v1.canConvert<double>())
            return v0;
        const double t = (frame - f0) / double(f1 - f0);
        return v0.toDouble() + (v1.toDouble() - v0.toDouble()) * t;
    }
    return getValue(track.back());
}

QVariantMap normalizeTrackForDuration(const QVariant &rawTrack, const QVariant &fallback, int durationFrames) {
    if (isStructuredTrack(rawTrack)) {
        QVariantMap raw = rawTrack.toMap();
        QVariantMap start = raw.value(QStringLiteral("start")).toMap();
        QVariantList points = raw.value(QStringLiteral("points")).toList(), nextPoints;
        start[QStringLiteral("frame")] = 0;
        if (!start.contains(QStringLiteral("value")))
            start[QStringLiteral("value")] = fallback;

        for (const auto &v : std::as_const(points)) {
            const int f = v.toMap().value(QStringLiteral("frame")).toInt();
            if (f > 0 && f <= durationFrames)
                nextPoints.append(v);
        }
        QVariantMap out;
        out[QStringLiteral("start")] = start;
        out[QStringLiteral("points")] = sortPoints(nextPoints);
        return out;
    }
    // レガシーリスト形式
    QVariantList legacy = sortPoints(rawTrack.toList()), points;
    QVariantMap start = makePoint(0, legacy.isEmpty() ? fallback : evaluateTrackLinear(legacy, 0, fallback), QStringLiteral("linear"));
    for (const auto &v : std::as_const(legacy)) {
        const int f = v.toMap().value(QStringLiteral("frame")).toInt();
        if (f > 0 && f < durationFrames)
            points.append(v);
    }
    QVariantMap out;
    out[QStringLiteral("start")] = start;
    out[QStringLiteral("points")] = sortPoints(points);
    return out;
}

} // anonymous namespace

namespace Rina::Engine::Timeline {

// ── 既存関数（STEP 2 から継続）────────────────────────────────────────

QVariantMap ClipEffectSystem::evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame) {
    QVariantMap out;
    const auto *stack = effectStacks.find(clipId);
    if (!stack)
        return out;
    for (const auto &eff : stack->effects) {
        if (!eff.enabled)
            continue;
        // relFrame を使ったキーフレーム補間は将来実装
        out.insert(eff.id, eff.params);
    }
    return out;
}

void ClipEffectSystem::updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    if (effectIndex < 0 || effectIndex >= stack->effects.size())
        return;
    stack->effects[effectIndex].params.insert(paramName, value);
}

// ── 新規追加関数 ──────────────────────────────────────────────────────

void ClipEffectSystem::addEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    // "transform" エフェクトは先頭に強制配置
    if (data.id == QStringLiteral("transform"))
        stack->effects.prepend(data);
    else
        stack->effects.append(data);
}

void ClipEffectSystem::restoreEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data) {
    // Undo redo 側：keyframeTracks 込みのデータをそのまま末尾に追加
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    stack->effects.append(data);
}

bool ClipEffectSystem::removeEffect(ECSState &state, int clipId, int effectIndex, Rina::UI::EffectData *outRemoved) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return false;

    const int n = stack->effects.size();
    int idx = (effectIndex == -1) ? n - 1 : effectIndex;

    if (idx < 0 || idx >= n)
        return false;

    // transform エフェクト（index 0）は削除禁止
    if (idx == 0 && stack->effects.at(0).id == QStringLiteral("transform"))
        return false;

    if (outRemoved)
        *outRemoved = stack->effects.at(idx);

    stack->effects.removeAt(idx);
    return true;
}

bool ClipEffectSystem::removeMultipleEffects(ECSState &state, int clipId, const QList<int> &sortedDescIndices, QList<Rina::UI::EffectData> *outRemoved) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return false;

    if (outRemoved)
        outRemoved->clear();

    bool anyRemoved = false;
    for (int idx : sortedDescIndices) {
        Rina::UI::EffectData removed;
        if (removeEffect(state, clipId, idx, outRemoved ? &removed : nullptr)) {
            if (outRemoved)
                outRemoved->prepend(removed); // ascData として返す
            anyRemoved = true;
        }
    }
    return anyRemoved;
}

void ClipEffectSystem::restoreMultipleEffects(ECSState &state, int clipId, const QList<Rina::UI::EffectData> &ascData) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    for (const auto &d : ascData)
        stack->effects.append(d);
}

void ClipEffectSystem::setEffectEnabled(ECSState &state, int clipId, int effectIndex, bool enabled) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    if (effectIndex < 0 || effectIndex >= stack->effects.size())
        return;
    stack->effects[effectIndex].enabled = enabled;
}

bool ClipEffectSystem::reorderEffects(ECSState &state, int clipId, int oldIndex, int newIndex) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return false;
    const int n = stack->effects.size();
    if (oldIndex < 0 || oldIndex >= n || newIndex < 0 || newIndex >= n)
        return false;
    stack->effects.move(oldIndex, newIndex);
    return true;
}

bool ClipEffectSystem::applyPermutation(ECSState &state, int clipId, const QList<int> &perm) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return false;
    if (perm.size() != stack->effects.size())
        return false;
    QList<Rina::UI::EffectData> reordered;
    reordered.reserve(perm.size());
    for (int idx : perm)
        reordered.append(stack->effects.at(idx));
    stack->effects = std::move(reordered);
    return true;
}

void ClipEffectSystem::pasteEffect(ECSState &state, int clipId, int targetIndex, const Rina::UI::EffectData &data) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    const int idx = std::clamp(targetIndex, 0, static_cast<int>(stack->effects.size()));
    stack->effects.insert(idx, data);
}

void ClipEffectSystem::setKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    if (effectIndex < 0 || effectIndex >= stack->effects.size())
        return;

    auto &eff = stack->effects[effectIndex];
    const QVariant fallback = eff.params.value(paramName);
    const QVariant rawTrack = eff.keyframeTracks.value(paramName);
    QVariantMap track = normalizeTrackForDuration(rawTrack, fallback, inferredDurationForTrack(rawTrack));

    QVariantMap startPt = track.value(QStringLiteral("start")).toMap();
    QVariantList points = track.value(QStringLiteral("points")).toList();
    const QString interp = options.value(QStringLiteral("interp"), QStringLiteral("none")).toString();
    const int startFrame = startPt.value(QStringLiteral("frame")).toInt();

    if (frame <= startFrame) {
        startPt[QStringLiteral("value")] = value;
        startPt[QStringLiteral("interp")] = options.value(QStringLiteral("interp"), startPt.value(QStringLiteral("interp"), QStringLiteral("none")));
        track[QStringLiteral("start")] = startPt;
        eff.keyframeTracks[paramName] = track;
        return;
    }

    QVariantMap kf;
    kf[QStringLiteral("frame")] = frame;
    kf[QStringLiteral("value")] = value;
    kf[QStringLiteral("interp")] = interp;
    if (options.contains(QStringLiteral("points")))
        kf[QStringLiteral("points")] = options.value(QStringLiteral("points"));
    if (options.contains(QStringLiteral("modeParams")))
        kf[QStringLiteral("modeParams")] = options.value(QStringLiteral("modeParams"));

    bool updated = false;
    for (int i = 0; i < points.size(); ++i) {
        if (points[i].toMap().value(QStringLiteral("frame")).toInt() == frame) {
            points[i] = kf;
            updated = true;
            break;
        }
    }
    if (!updated)
        points.append(kf);

    track[QStringLiteral("points")] = sortPoints(points);
    eff.keyframeTracks[paramName] = track;
}

void ClipEffectSystem::removeKeyframe(ECSState &state, int clipId, int effectIndex, const QString &paramName, int frame) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    if (effectIndex < 0 || effectIndex >= stack->effects.size())
        return;

    auto &eff = stack->effects[effectIndex];
    const QVariant fallback = eff.params.value(paramName);
    const QVariant rawTrack = eff.keyframeTracks.value(paramName);
    QVariantMap track = normalizeTrackForDuration(rawTrack, fallback, inferredDurationForTrack(rawTrack));

    const int startFrame = track.value(QStringLiteral("start")).toMap().value(QStringLiteral("frame")).toInt();
    // 開始点は削除不可
    if (frame <= startFrame)
        return;

    QVariantList points = track.value(QStringLiteral("points")).toList(), next;
    for (const auto &v : std::as_const(points))
        if (v.toMap().value(QStringLiteral("frame")).toInt() != frame)
            next.append(v);

    track[QStringLiteral("points")] = next;
    eff.keyframeTracks[paramName] = track;
}

namespace {

bool isStructuredTrackLocal(const QVariant &v) { return v.toMap().contains(QStringLiteral("start")); }

QVariantMap ensureStructuredTrackLocal(const QVariant &raw, const QVariant &fallback) {
    if (isStructuredTrackLocal(raw))
        return raw.toMap();
    QVariantMap s;
    s[QStringLiteral("frame")] = 0;
    s[QStringLiteral("value")] = fallback;
    s[QStringLiteral("interp")] = QStringLiteral("none");
    QVariantMap t;
    t[QStringLiteral("start")] = s;
    t[QStringLiteral("points")] = QVariantList();
    return t;
}

QVariantList flatPointsLocal(const QVariantMap &track) {
    QVariantList r;
    r.append(track.value(QStringLiteral("start")));
    r.append(track.value(QStringLiteral("points")).toList());
    return r;
}

QVariant evalTrackLocal(const QVariantList &flat, int frame, const QVariant &fallback) {
    if (flat.isEmpty())
        return fallback;
    QVariantMap prev = flat.first().toMap();
    for (int i = 1; i < flat.size(); ++i) {
        const QVariantMap cur = flat.at(i).toMap();
        const int cf = cur.value(QStringLiteral("frame")).toInt();
        if (frame <= cf) {
            const int pf = prev.value(QStringLiteral("frame")).toInt();
            if (pf == cf)
                return prev.value(QStringLiteral("value"));
            const double t = static_cast<double>(frame - pf) / (cf - pf);
            const QVariant pv = prev.value(QStringLiteral("value"));
            const QVariant cv2 = cur.value(QStringLiteral("value"));
            if (pv.typeId() == QMetaType::Double || pv.typeId() == QMetaType::Int || pv.typeId() == QMetaType::LongLong)
                return pv.toDouble() + (cv2.toDouble() - pv.toDouble()) * t;
            return pv;
        }
        prev = cur;
    }
    return prev.value(QStringLiteral("value"));
}

} // anonymous namespace

std::pair<QVariantMap, QVariantMap> ClipEffectSystem::splitKeyframeTracks(const QVariantMap &tracks, const QVariantMap &params, int firstHalfDuration, int originalDuration) {
    QVariantMap firstHalfTracks, secondHalfTracks;
    if (originalDuration < 1)
        return {firstHalfTracks, secondHalfTracks};

    const int firstEndFrame = std::max(0, firstHalfDuration - 1);
    const int secondEndFrame = std::max(0, originalDuration - firstHalfDuration - 1);

    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString key = it.key();
        const QVariant fallback = it.value();
        const QVariantMap track = ensureStructuredTrackLocal(tracks.value(key), fallback);
        const QVariantList flat = flatPointsLocal(track);
        const QVariantMap startKf = track.value(QStringLiteral("start")).toMap();
        const QVariantList points = track.value(QStringLiteral("points")).toList();

        QVariantList firstPoints;
        for (const auto &v : points) {
            const int f = v.toMap().value(QStringLiteral("frame")).toInt();
            if (f > 0 && f < firstEndFrame)
                firstPoints.append(v);
        }
        QVariantMap firstTrack;
        firstTrack[QStringLiteral("start")] = startKf;
        firstTrack[QStringLiteral("points")] = firstPoints;
        firstHalfTracks[key] = firstTrack;

        QVariantMap secondStart;
        secondStart[QStringLiteral("frame")] = 0;
        secondStart[QStringLiteral("value")] = evalTrackLocal(flat, firstHalfDuration, fallback);
        secondStart[QStringLiteral("interp")] = startKf.value(QStringLiteral("interp"), QStringLiteral("none"));
        QVariantList secondPoints;
        for (const auto &v : points) {
            auto m = v.toMap();
            const int f = m.value(QStringLiteral("frame")).toInt();
            if (f > firstHalfDuration && f < std::max(0, originalDuration - 1)) {
                m[QStringLiteral("frame")] = f - firstHalfDuration;
                const int nf = m.value(QStringLiteral("frame")).toInt();
                if (nf > 0 && nf < secondEndFrame)
                    secondPoints.append(m);
            }
        }
        QVariantMap secondTrack;
        secondTrack[QStringLiteral("start")] = secondStart;
        secondTrack[QStringLiteral("points")] = secondPoints;
        secondHalfTracks[key] = secondTrack;
    }
    return {firstHalfTracks, secondHalfTracks};
}

void ClipEffectSystem::setKeyframeTracksAt(ECSState &state, int clipId, int effectIndex, const QVariantMap &tracks, int durationFrames) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack || effectIndex < 0 || effectIndex >= stack->effects.size())
        return;
    auto &eff = stack->effects[effectIndex];
    eff.keyframeTracks = tracks;
    for (auto it = eff.params.begin(); it != eff.params.end(); ++it) {
        const QString &key = it.key();
        if (isStructuredTrackLocal(eff.keyframeTracks.value(key)))
            continue;
        QVariantMap s;
        s[QStringLiteral("frame")] = 0;
        s[QStringLiteral("value")] = it.value();
        s[QStringLiteral("interp")] = QStringLiteral("none");
        QVariantMap t;
        t[QStringLiteral("start")] = s;
        t[QStringLiteral("points")] = QVariantList();
        eff.keyframeTracks[key] = t;
    }
    (void)durationFrames;
}

} // namespace Rina::Engine::Timeline
