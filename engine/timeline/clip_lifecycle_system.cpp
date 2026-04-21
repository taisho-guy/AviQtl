#include "clip_lifecycle_system.hpp"

namespace Rina::Engine::Timeline {

void ClipLifecycleSystem::createClip(ECSState &state, int clipId, const QString &type, int layer, int startFrame, int durationFrames) {
    // 必要なコンポーネントを実体化し、IDを紐付ける
    auto &transform = state.transforms[clipId];
    transform.clipId = clipId; // フェーズ3で追加したプロパティ
    transform.layer = layer;
    transform.startFrame = startFrame;
    transform.durationFrames = durationFrames;

    auto &metadata = state.metadataStates[clipId];
    metadata.clipId = clipId;
    metadata.type = type;

    auto &selection = state.selections[clipId];
    selection.clipId = clipId;
    selection.isSelected = false;

    // TODO: Audio, EffectStacks は初期段階では空だがエントリだけ作る
    auto &fxStack = state.effectStacks[clipId];
    fxStack.clipId = clipId;

    auto &audioStack = state.audioStacks[clipId];
    audioStack.clipId = clipId;
}

void ClipLifecycleSystem::destroyClip(ECSState &state, int clipId) {
    // 全 DenseComponentMap から clipId を消去する
    state.transforms.erase(clipId);
    state.metadataStates.erase(clipId);
    state.selections.erase(clipId);
    state.effectStacks.erase(clipId);
    state.audioStacks.erase(clipId);
    state.audioStates.erase(clipId);
    state.renderStates.erase(clipId);
}

void ClipLifecycleSystem::restoreClipFromDTO(ECSState &state, const Rina::UI::ClipData &dto) {
    // DTO(スナップショット)からコンポーネントを再構築する
    auto &transform = state.transforms[dto.id];
    transform.clipId = dto.id;
    transform.layer = dto.layer;
    transform.startFrame = dto.startFrame;
    transform.durationFrames = dto.durationFrames;

    auto &metadata = state.metadataStates[dto.id];
    metadata.clipId = dto.id;
    metadata.type = dto.type;

    // --- フェーズ1〜4の過渡期におけるハイブリッド復元 ---
    // effectStacks, audioStacks などへのポインタの復帰が必要
    // 本来はDOD化されたパラメータ配列へ復元するが、現状はリストをそのまま載せる
    // (unpackClipData から呼ばれる想定)
}

} // namespace Rina::Engine::Timeline
