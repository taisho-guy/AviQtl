#pragma once
// フェーズ7: Transform + Audio を単一 SSBO に統合した GpuClipSoA
// std430 準拠。GpuTransformSoA / GpuAudioSoA を廃止し重複フィールドを除去する。
// すべてのフィールドが trivially_copyable であるため memcpy が安全。
//
// GLSL 対応:
//   layout(std430, binding=0) readonly buffer ClipSSBO {
//       int  count;
//       int  _pad[3];
//       int  clipIds[MAX_ACTIVE_CLIPS];
//       int  layers[MAX_ACTIVE_CLIPS];
//       float timePositions[MAX_ACTIVE_CLIPS];
//       int  startFrames[MAX_ACTIVE_CLIPS];
//       int  durationFrames[MAX_ACTIVE_CLIPS];
//       float volumes[MAX_ACTIVE_CLIPS];
//       float pans[MAX_ACTIVE_CLIPS];
//       int  mutes[MAX_ACTIVE_CLIPS];
//   };
#include <array>
#include <cstdint>
#include <type_traits>

namespace AviQtl::Engine::Timeline {

inline constexpr int MAX_ACTIVE_CLIPS = 256;

// Phase 8 で QRhiShaderResourceBinding に渡す binding 番号の確定値
// 変更禁止: この定数を参照するすべての .comp/.frag/.glsl および
// ComputeEffect::setStorageBufferRaw() 呼び出し元はこの値に依存する
inline constexpr int SSBO_BINDING_CLIP = 0;

struct alignas(16) GpuClipSoA {
    int32_t count = 0;
    int32_t _pad[3] = {};
    // Transform フィールド (TransformComponent から転写)
    std::array<int32_t, MAX_ACTIVE_CLIPS> clipIds{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> layers{};
    std::array<float,   MAX_ACTIVE_CLIPS> timePositions{};
    // タイミング情報 (TransformComponent が正規のソース)
    // AudioComponent.startFrame / durationFrames はオーディオミキサー専用
    std::array<int32_t, MAX_ACTIVE_CLIPS> startFrames{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> durationFrames{};
    // Audio フィールド (AudioComponent から転写。コンポーネント不在時はデフォルト値)
    std::array<float,   MAX_ACTIVE_CLIPS> volumes{};
    std::array<float,   MAX_ACTIVE_CLIPS> pans{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> mutes{};
};

static_assert(std::is_trivially_copyable_v<GpuClipSoA>,
    "GpuClipSoA must be trivially copyable for glBufferData / vkCmdCopyBuffer");
static_assert(sizeof(GpuClipSoA) == 16 + MAX_ACTIVE_CLIPS * 8 * sizeof(int32_t),
    "GpuClipSoA layout check failed: std430 alignment violation");

} // namespace AviQtl::Engine::Timeline
