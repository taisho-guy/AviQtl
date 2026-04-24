#include "clip_effect_system.hpp"
#include "clip_lifecycle_system.hpp"
#include "commands.hpp"
#include "effect_data.hpp"
#include "effect_registry.hpp"
#include "selection_service.hpp"
#include "timeline_service.hpp"
#include <QDebug>
#include <algorithm>

namespace Rina::UI {

void TimelineService::addEffect(int clipId, const QString &effectId) { m_undoStack->push(new AddEffectCommand(this, clipId, effectId, Rina::Core::EffectRegistry::instance().getEffect(effectId).name)); }

void TimelineService::addEffectInternal(int clipId, const QString &effectId) {
    const auto meta = Rina::Core::EffectRegistry::instance().getEffect(effectId);
    Rina::UI::EffectData data;
    data.id = meta.id;
    data.name = meta.name;
    data.kind = meta.kind;
    data.categories = meta.categories;
    data.qmlSource = meta.qmlSource;
    data.uiDefinition = meta.uiDefinition;
    data.enabled = true;
    data.params = meta.defaultParams;
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::addEffect(ecs.editState(), clipId, data);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::restoreEffectInternal(int clipId, const Rina::UI::EffectData &data) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::restoreEffect(ecs.editState(), clipId, data);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::removeEffect(int clipId, int effectIndex) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap)
        return;
    const int n = snap->effects.size();
    const int idx = (effectIndex == -1) ? n - 1 : effectIndex;
    if (idx < 0 || idx >= n)
        return;
    auto *cmd = new RemoveEffectCommand(this, clipId, effectIndex, snap->effects.at(idx).name);
    cmd->setRemovedEffect(snap->effects.at(idx));
    m_undoStack->push(cmd);
}

void TimelineService::removeEffectInternal(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::removeEffect(ecs.editState(), clipId, effectIndex);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::removeMultipleEffects(int clipId, const QList<int> &indices) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap)
        return;
    QList<int> sorted;
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(snap->effects.size()))
            sorted.append(idx);
    }
    if (sorted.isEmpty())
        return;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    auto *cmd = new RemoveMultipleEffectsCommand(this, clipId, sorted, QObject::tr("エフェクト削除 (%1件)").arg(sorted.size()));
    m_undoStack->push(cmd);
}

void TimelineService::removeMultipleEffectsInternal(int clipId, const QList<int> &sortedDescIndices, QList<Rina::UI::EffectData> *outData) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::removeMultipleEffects(ecs.editState(), clipId, sortedDescIndices, outData);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::restoreMultipleEffectsInternal(int clipId, const QList<Rina::UI::EffectData> &ascData) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::restoreMultipleEffects(ecs.editState(), clipId, ascData);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::setEffectEnabled(int clipId, int effectIndex, bool enabled) { m_undoStack->push(new SetEffectEnabledCommand(this, clipId, effectIndex, enabled)); }

void TimelineService::setEffectEnabledInternal(int clipId, int effectIndex, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::setEffectEnabled(ecs.editState(), clipId, effectIndex, enabled);
    ecs.commit();
    emit clipEffectsChanged(clipId);
}

void TimelineService::reorderEffects(int clipId, int oldIndex, int newIndex) { m_undoStack->push(new ReorderEffectCommand(this, clipId, oldIndex, newIndex)); }

void TimelineService::reorderEffectsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::reorderEffects(ecs.editState(), clipId, oldIndex, newIndex);
    ecs.commit();
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::reorderMultipleEffects(int clipId, const QVariantList &indicesList, int targetIndex) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap)
        return;
    const int n = snap->effects.size();
    QList<int> indices;
    for (const QVariant &v : indicesList) {
        bool ok = false;
        const int val = v.toInt(&ok);
        if (ok)
            indices.append(val);
    }
    QList<int> valid;
    for (int idx : indices) {
        if (idx > 0 && idx < n)
            valid.append(idx);
    }
    if (valid.isEmpty())
        return;
    std::sort(valid.begin(), valid.end());
    valid.erase(std::unique(valid.begin(), valid.end()), valid.end());
    const QSet<int> selectedSet(valid.begin(), valid.end());
    QList<int> selectedIdx, remainingIdx;
    for (int i = 0; i < n; ++i) {
        if (selectedSet.contains(i))
            selectedIdx.append(i);
        else
            remainingIdx.append(i);
    }
    int countBefore = 0;
    for (int idx : valid) {
        if (idx < targetIndex)
            countBefore++;
    }
    const int insertAt = std::clamp(targetIndex - countBefore, 1, static_cast<int>(remainingIdx.size()));
    QList<int> newOrderIdx;
    newOrderIdx.reserve(n);
    for (int i = 0; i < insertAt; ++i)
        newOrderIdx.append(remainingIdx.at(i));
    for (int idx : std::as_const(selectedIdx))
        newOrderIdx.append(idx);
    for (int i = insertAt; i < static_cast<int>(remainingIdx.size()); ++i)
        newOrderIdx.append(remainingIdx.at(i));
    bool identity = true;
    for (int i = 0; i < n; ++i) {
        if (newOrderIdx.at(i) != i) {
            identity = false;
            break;
        }
    }
    if (identity)
        return;
    QList<int> redoPerm = newOrderIdx;
    QList<int> undoPerm(n);
    for (int i = 0; i < n; ++i)
        undoPerm[newOrderIdx.at(i)] = i;
    m_undoStack->push(new ReorderMultipleEffectsCommand(this, clipId, std::move(redoPerm), std::move(undoPerm), QObject::tr("エフェクト順序変更 (%1件)").arg(valid.size())));
}

void TimelineService::applyPermutationInternal(int clipId, const QList<int> &perm) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::applyPermutation(ecs.editState(), clipId, perm);
    ecs.commit();
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::setAudioPluginEnabled(int clipId, int index, bool enabled) { m_undoStack->push(new SetAudioPluginEnabledCommand(this, clipId, index, enabled)); }

void TimelineService::setAudioPluginEnabledInternal(int clipId, int index, bool enabled) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    auto *stack = ecs.editState().audioStacks.find(clipId);
    if (!stack || index < 0 || index >= stack->audioPlugins.size())
        return;
    stack->audioPlugins[index].enabled = enabled;
    ecs.commit();
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::reorderAudioPlugins(int clipId, int oldIndex, int newIndex) { m_undoStack->push(new ReorderAudioPluginCommand(this, clipId, oldIndex, newIndex)); }

void TimelineService::reorderAudioPluginsInternal(int clipId, int oldIndex, int newIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    auto *stack = ecs.editState().audioStacks.find(clipId);
    if (!stack || oldIndex < 0 || oldIndex >= stack->audioPlugins.size() || newIndex < 0 || newIndex >= stack->audioPlugins.size())
        return;
    stack->audioPlugins.move(oldIndex, newIndex);
    ecs.commit();
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::copyEffect(int clipId, int effectIndex) { // NOLINT(bugprone-easily-swappable-parameters)
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (snap && effectIndex >= 0 && effectIndex < snap->effects.size())
        m_effectClipboard = snap->effects.at(effectIndex);
}

void TimelineService::pasteEffect(int clipId, int targetIndex) {
    if (!m_effectClipboard.has_value())
        return;
    m_undoStack->push(new PasteEffectCommand(this, clipId, targetIndex, m_effectClipboard.value()));
}

void TimelineService::pasteEffectInternal(int clipId, int targetIndex, // NOLINT(bugprone-easily-swappable-parameters)
                                          const Rina::UI::EffectData &data) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::pasteEffect(ecs.editState(), clipId, targetIndex, data);
    ecs.commit();
    emit clipEffectsChanged(clipId);
    emit clipsChanged();
}

void TimelineService::updateEffectParam(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap || effectIndex >= static_cast<int>(snap->effects.size()))
        return;
    const QVariant oldValue = snap->effects.at(effectIndex).params.value(paramName);
    m_undoStack->push(new UpdateEffectParamCommand(this, clipId, effectIndex, paramName, value, oldValue, snap->effects.at(effectIndex).name));
}

void TimelineService::updateEffectParamInternal(int clipId, int effectIndex, const QString &paramName, const QVariant &value) {
    qDebug() << "[TMM] updateEffectParamInternal: clipId=" << clipId << "effectIndex=" << effectIndex << "paramName=" << paramName << "value=" << value;
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::updateParam(ecs.editState(), clipId, effectIndex, paramName, value);
    ecs.commit();
    if (paramName == QLatin1String("path") || paramName == QLatin1String("source") || paramName == QStringLiteral("targetSceneId"))
        emit clipsChanged();
    emit effectParamChanged(clipId, effectIndex, paramName, value);
    // [BUG FIX #1] refreshSelectionData 削除。
    // selectedClipDataChanged が emit される → QML SettingDialog がスライダー value を再セット
    // → onValueChanged 再発火 → updateEffectParamInternal 2重呼び出し（重さの根本原因）。
}

void TimelineService::setKeyframe(int clipId, int effectIndex, const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap || effectIndex >= snap->effects.size())
        return;
    const auto &tracks = snap->effects.at(effectIndex).keyframeTracks;
    bool wasExisting = false;
    QVariant oldValue;
    QVariantMap oldOptions;
    const QVariant rawTrack = tracks.value(paramName);
    auto searchPts = [&](const QVariantList &pts) {
        for (const auto &v : pts) {
            const auto m = v.toMap();
            if (m.value(QStringLiteral("frame")).toInt() == frame) {
                wasExisting = true;
                oldValue = m.value(QStringLiteral("value"));
                oldOptions = m;
                return;
            }
        }
    };
    if (rawTrack.toMap().contains(QStringLiteral("points")))
        searchPts(rawTrack.toMap().value(QStringLiteral("points")).toList());
    else
        searchPts(rawTrack.toList());
    m_undoStack->push(new SetKeyframeCommand(this, clipId, effectIndex, paramName, frame, value, options, oldValue, oldOptions, wasExisting));
}

void TimelineService::removeKeyframe(int clipId, int effectIndex, const QString &paramName, int frame) {
    const auto *snap = Rina::Engine::Timeline::ECS::instance().getSnapshot()->effectStacks.find(clipId);
    if (!snap || effectIndex >= snap->effects.size())
        return;
    const QVariant rawTrack = snap->effects.at(effectIndex).keyframeTracks.value(paramName);
    QVariant savedValue;
    QVariantMap savedOptions;
    auto searchPts = [&](const QVariantList &pts) {
        for (const auto &v : pts) {
            const auto m = v.toMap();
            if (m.value(QStringLiteral("frame")).toInt() == frame) {
                savedValue = m.value(QStringLiteral("value"));
                savedOptions = m;
                return;
            }
        }
    };
    if (rawTrack.toMap().contains(QStringLiteral("points")))
        searchPts(rawTrack.toMap().value(QStringLiteral("points")).toList());
    else
        searchPts(rawTrack.toList());
    m_undoStack->push(new RemoveKeyframeCommand(this, clipId, effectIndex, paramName, frame, savedValue, savedOptions));
}

void TimelineService::setKeyframeInternal(int clipId, int effectIndex, // NOLINT(bugprone-easily-swappable-parameters)
                                          const QString &paramName, int frame, const QVariant &value, const QVariantMap &options) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::setKeyframe(ecs.editState(), clipId, effectIndex, paramName, frame, value, options);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

void TimelineService::removeKeyframeInternal(int clipId, int effectIndex, // NOLINT(bugprone-easily-swappable-parameters)
                                             const QString &paramName, int frame) {
    auto &ecs = Rina::Engine::Timeline::ECS::instance();
    Rina::Engine::Timeline::ClipEffectSystem::removeKeyframe(ecs.editState(), clipId, effectIndex, paramName, frame);
    ecs.commit();
    emit clipsChanged();
    emit clipEffectsChanged(clipId);
}

} // namespace Rina::UI
