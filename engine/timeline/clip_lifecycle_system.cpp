#include "clip_lifecycle_system.hpp"
#include "clip_snapshot.hpp"
#include "effect_model.hpp"

namespace AviQtl::Engine::Timeline {

void ClipLifecycleSystem::createClip(ECSState &state, int clipId, const QString &type, int layer, int startFrame, int durationFrames) {
    auto &transform = state.transforms[clipId];
    transform.clipId = clipId;
    transform.layer = layer;
    transform.startFrame = startFrame;
    transform.durationFrames = durationFrames;

    auto &metadata = state.metadataStates[clipId];
    metadata.clipId = clipId;
    metadata.type = type;

    auto &selection = state.selections[clipId];
    selection.clipId = clipId;
    selection.isSelected = false;

    auto &fxStack = state.effectStacks[clipId];
    fxStack.clipId = clipId;

    auto &audioStack = state.audioStacks[clipId];
    audioStack.clipId = clipId;
}

void ClipLifecycleSystem::destroyClip(ECSState &state, int clipId) {
    state.transforms.erase(clipId);
    state.metadataStates.erase(clipId);
    state.selections.erase(clipId);
    state.effectStacks.erase(clipId);
    state.audioStacks.erase(clipId);
    state.audioStates.erase(clipId);
    state.renderStates.erase(clipId);
}

void ClipLifecycleSystem::restoreClipFromDTO(ECSState &state, const AviQtl::UI::ClipData &dto) {
    // TransformComponent
    auto &transform = state.transforms[dto.id];
    transform.clipId = dto.id;
    transform.layer = dto.layer;
    transform.startFrame = dto.startFrame;
    transform.durationFrames = dto.durationFrames;

    // MetadataComponent
    auto &metadata = state.metadataStates[dto.id];
    metadata.clipId = dto.id;
    metadata.type = dto.type;

    // SelectionComponent（復元時は非選択）
    auto &sel = state.selections[dto.id];
    sel.clipId = dto.id;
    sel.isSelected = false;

    // EffectStackComponent（EffectModel* → EffectData 変換）
    auto &fxStack = state.effectStacks[dto.id];
    fxStack.clipId = dto.id;
    fxStack.effects.clear();
    for (const auto *eff : std::as_const(dto.effects)) {
        fxStack.effects.append(AviQtl::UI::effectDataFromModel(*eff));
    }

    // AudioStackComponent（AudioPluginState を直接格納）
    auto &audioStack = state.audioStacks[dto.id];
    audioStack.clipId = dto.id;
    audioStack.audioPlugins = dto.audioPlugins;

    // AudioComponent（初期値。volume/pan/mute は再生エンジンが上書きする）
    auto &audio = state.audioStates[dto.id];
    audio.clipId = dto.id;
    audio.startFrame = dto.startFrame;
    audio.durationFrames = dto.durationFrames;
}

void ClipLifecycleSystem::restoreClipFromSnapshot(ECSState &state, const AviQtl::UI::ClipSnapshot &snap) {
    const int id = snap.transform.clipId;
    state.transforms[id] = snap.transform;
    state.metadataStates[id] = snap.metadata;
    state.effectStacks[id] = snap.effectStack;
    state.audioStacks[id] = snap.audioStack;

    auto &sel = state.selections[id];
    sel.clipId = id;
    sel.isSelected = false;

    auto &audio = state.audioStates[id];
    audio.clipId = id;
    audio.startFrame = snap.transform.startFrame;
    audio.durationFrames = snap.transform.durationFrames;
}

} // namespace AviQtl::Engine::Timeline
