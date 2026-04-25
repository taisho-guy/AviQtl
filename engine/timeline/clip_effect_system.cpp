#include "clip_effect_system.hpp"
#include "effect_interpolation.hpp"
#include "lua_host.hpp"
#include <QColor>
#include <algorithm>
#include <cmath>

namespace {

// splitKeyframeTracks 境界値計算用
using namespace Rina::Engine::Timeline::Interp;

QVariantMap ensureStructuredTrackLocal(const QVariant &raw, const QVariant &fallback) { return normalizeTrackForDuration(raw, fallback, inferredDurationForTrack(raw)); }

} // anonymous namespace

namespace Rina::Engine::Timeline {

// ── 実装 ──────────────────────────────────────────────────────────

QVariantMap ClipEffectSystem::evaluateParams(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int relFrame, int durationFrames, double fps) {
    QVariantMap out;
    const auto *stack = effectStacks.find(clipId);
    if (!stack)
        return out;
    for (const auto &eff : stack->effects) {
        if (!eff.enabled)
            continue;
        QVariantMap evaluated;
        for (auto it = eff.params.cbegin(); it != eff.params.cend(); ++it) {
            evaluated.insert(it.key(), Interp::evaluateParam(eff.keyframeTracks, eff.params, it.key(), relFrame, durationFrames, fps));
        }
        out.insert(eff.id, evaluated);
    }
    return out;
}

QVariant ClipEffectSystem::evaluateParam(const DenseComponentMap<EffectStackComponent> &effectStacks, int clipId, int effectIndex, const QString &paramName, int relFrame, int durationFrames, double fps) {
    const auto *stack = effectStacks.find(clipId);
    if (!stack || effectIndex < 0 || effectIndex >= stack->effects.size())
        return {};
    const auto &eff = stack->effects[effectIndex];
    return Interp::evaluateParam(eff.keyframeTracks, eff.params, paramName, relFrame, durationFrames, fps);
}

void ClipEffectSystem::updateParam(ECSState &state, int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    if (effectIndex < 0 || effectIndex >= stack->effects.size())
        return;
    stack->effects[effectIndex].params.insert(paramName, value);

    // fallback 値が変わるため、該当パラメータのキャッシュを無効化
    ECS::instance().interpCache().invalidateParam(clipId, effectIndex, paramName);
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
    ECS::instance().interpCache().resize(clipId, stack->effects.size());
}

void ClipEffectSystem::restoreEffect(ECSState &state, int clipId, const Rina::UI::EffectData &data) {
    // Undo redo 側：keyframeTracks 込みのデータをそのまま末尾に追加
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    stack->effects.append(data);
    ECS::instance().interpCache().resize(clipId, stack->effects.size());
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
    ECS::instance().interpCache().removeAt(clipId, idx);
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
    if (anyRemoved)
        ECS::instance().interpCache().resize(clipId, state.effectStacks[clipId].effects.size());
    return anyRemoved;
}

void ClipEffectSystem::restoreMultipleEffects(ECSState &state, int clipId, const QList<Rina::UI::EffectData> &ascData) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    for (const auto &d : ascData)
        stack->effects.append(d);
    ECS::instance().interpCache().resize(clipId, stack->effects.size());
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
    ECS::instance().interpCache().move(clipId, oldIndex, newIndex);
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
    ECS::instance().interpCache().resetAll(clipId, stack->effects.size());
    return true;
}

void ClipEffectSystem::pasteEffect(ECSState &state, int clipId, int targetIndex, const Rina::UI::EffectData &data) {
    auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return;
    const int idx = std::clamp(targetIndex, 0, static_cast<int>(stack->effects.size()));
    stack->effects.insert(idx, data);
    ECS::instance().interpCache().resize(clipId, stack->effects.size());
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
    QVariantMap track = Interp::normalizeTrackForDuration(rawTrack, fallback, Interp::inferredDurationForTrack(rawTrack));

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

    track[QStringLiteral("points")] = Interp::sortPoints(points);
    eff.keyframeTracks[paramName] = track;

    // キャッシュ無効化：このパラメータのフラットポイント列が変わった
    ECS::instance().interpCache().invalidateParam(clipId, effectIndex, paramName);
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
    QVariantMap track = Interp::normalizeTrackForDuration(rawTrack, fallback, Interp::inferredDurationForTrack(rawTrack));

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

    // キャッシュ無効化
    ECS::instance().interpCache().invalidateParam(clipId, effectIndex, paramName);
}

std::pair<QVariantMap, QVariantMap> ClipEffectSystem::splitKeyframeTracks(const QVariantMap &tracks, const QVariantMap &params, int firstHalfDuration, int originalDuration) {
    QVariantMap firstHalfTracks, secondHalfTracks;
    if (originalDuration < 1 || firstHalfDuration <= 0 || firstHalfDuration >= originalDuration)
        return {firstHalfTracks, secondHalfTracks};

    const int splitF = firstHalfDuration;

    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString key = it.key();
        const QVariant fallback = it.value();
        const QVariantMap track = ensureStructuredTrackLocal(tracks.value(key), fallback);
        // splitTrackAt: custom bezier は de Casteljau 分割、標準 easing は評価値を使用
        auto [ft, st] = Interp::splitTrackAt(track, fallback, splitF);
        firstHalfTracks[key] = ft;
        secondHalfTracks[key] = st;
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
        if (Rina::Engine::Timeline::Interp::isStructuredTrack(eff.keyframeTracks.value(key)))
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
    // キャッシュ全無効化（tracks が丸ごと差し替わった）
    ECS::instance().interpCache().invalidateAll(clipId);
}

// ── キャッシュあり評価 ────────────────────────────────────────────────────

QVariant ClipEffectSystem::evaluateParamCached(const ECSState &state, InterpolationCache &cache, int clipId, int effectIndex, const QString &paramName, int relFrame, int durationFrames, double fps) {
    const auto *stack = state.effectStacks.find(clipId);
    if (!stack || effectIndex < 0 || effectIndex >= stack->effects.size())
        return {};
    const auto &eff = stack->effects[effectIndex];
    if (!eff.enabled)
        return eff.params.value(paramName);

    const QVariant fallback = eff.params.value(paramName);

    // エクスプレッション（Lua）は毎回評価
    const QString strVal = fallback.toString();
    if (strVal.startsWith(QStringLiteral("="))) {
        std::string expr = strVal.mid(1).toStdString();
        const double time = (fps > 0.0) ? relFrame / fps : 0.0;
        return Rina::Scripting::LuaHost::instance().evaluate(expr, time, 0, fallback.toDouble());
    }

    if (!eff.keyframeTracks.contains(paramName))
        return fallback;

    // キャッシュコンポーネントを取得（なければ新規作成）
    auto &entry = cache.getOrCreate(clipId, effectIndex, stack->effects.size());
    // durationFrames が変化したらキャッシュ全体を無効化
    if (entry.lastDuration != durationFrames) {
        entry.resolvedTracks.clear();
        entry.lastDuration = durationFrames;
    }

    if (!entry.resolvedTracks.contains(paramName)) {
        const QVariant raw = eff.keyframeTracks.value(paramName);
        if (Interp::isStructuredTrack(raw)) {
            const auto normalized = Interp::normalizeTrackForDuration(raw, fallback, durationFrames);
            entry.resolvedTracks.insert(paramName, Interp::flattenStructuredTrack(normalized));
        } else {
            entry.resolvedTracks.insert(paramName, Interp::sortPoints(raw.toList()));
        }
    }

    return Interp::evaluateTrack(entry.resolvedTracks.value(paramName), relFrame, fallback);
}

QVariantMap ClipEffectSystem::evaluateParamsCached(const ECSState &state, InterpolationCache &cache, int clipId, int relFrame, int durationFrames, double fps) {
    QVariantMap out;
    const auto *stack = state.effectStacks.find(clipId);
    if (!stack)
        return out;
    for (int ei = 0; ei < stack->effects.size(); ++ei) {
        const auto &eff = stack->effects[ei];
        if (!eff.enabled)
            continue;
        QVariantMap evaluated;
        for (auto it = eff.params.cbegin(); it != eff.params.cend(); ++it) {
            evaluated.insert(it.key(), evaluateParamCached(state, cache, clipId, ei, it.key(), relFrame, durationFrames, fps));
        }
        out.insert(eff.id, evaluated);
    }
    return out;
}

QVariantList ClipEffectSystem::keyframeListForUi(const ECSState &state, int clipId, int effectIndex, const QString &paramName) {
    const auto *stack = state.effectStacks.find(clipId);
    if (!stack || effectIndex < 0 || effectIndex >= stack->effects.size())
        return {};
    const auto &eff = stack->effects[effectIndex];
    const QVariant rawTrack = eff.keyframeTracks.value(paramName);
    if (!Interp::isStructuredTrack(rawTrack))
        return {};
    const QVariant fallback = eff.params.value(paramName);
    const QVariantMap track = Interp::normalizeTrackForDuration(rawTrack, fallback, Interp::inferredDurationForTrack(rawTrack));
    return Interp::flattenStructuredTrack(track);
}

} // namespace Rina::Engine::Timeline
