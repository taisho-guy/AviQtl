#pragma once
// フェーズ6: ECS → GPU 直結のための SSBO SoA レイアウト定義
// std430 準拠。シェーダー側の layout(std430, binding=N) に正確に対応する
// すべてのフィールドが trivially_copyable であるため glBufferData への memcpy が安全
#include <array>
#include <cstdint>
#include <type_traits>

namespace AviQtl::Engine::Timeline {

// 最大同時アクティブクリップ数 (IntervalTree 最大ヒット数より十分大きい値)
inline constexpr int MAX_ACTIVE_CLIPS = 256;

// std430 SoA ブロック: TransformComponent → GPU
// GLSL 対応:
//   layout(std430, binding=0) readonly buffer TransformSSBO {
//       int count; int _pad[3];
//       int clipIds[MAX_ACTIVE_CLIPS]; int layers[MAX_ACTIVE_CLIPS];
//       float timePositions[MAX_ACTIVE_CLIPS];
//       int startFrames[MAX_ACTIVE_CLIPS]; int durationFrames[MAX_ACTIVE_CLIPS];
//   };
struct alignas(16) GpuTransformSoA {
    int32_t count = 0;
    int32_t _pad[3] = {};
    std::array<int32_t, MAX_ACTIVE_CLIPS> clipIds{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> layers{};
    std::array<float, MAX_ACTIVE_CLIPS> timePositions{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> startFrames{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> durationFrames{};
};
static_assert(std::is_trivially_copyable_v<GpuTransformSoA>, "GpuTransformSoA must be trivially copyable for glBufferData");
static_assert(sizeof(GpuTransformSoA) == 16 + MAX_ACTIVE_CLIPS * 5 * sizeof(int32_t), "GpuTransformSoA layout check failed");

// std430 SoA ブロック: AudioComponent → GPU
// GLSL 対応:
//   layout(std430, binding=1) readonly buffer AudioSSBO {
//       int count; int _pad[3];
//       float volumes[N]; float pans[N]; int mutes[N];
//       int startFrames[N]; int durationFrames[N];
//   };
struct alignas(16) GpuAudioSoA {
    int32_t count = 0;
    int32_t _pad[3] = {};
    std::array<float, MAX_ACTIVE_CLIPS> volumes{};
    std::array<float, MAX_ACTIVE_CLIPS> pans{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> mutes{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> startFrames{};
    std::array<int32_t, MAX_ACTIVE_CLIPS> durationFrames{};
};
static_assert(std::is_trivially_copyable_v<GpuAudioSoA>, "GpuAudioSoA must be trivially copyable for glBufferData");
static_assert(sizeof(GpuAudioSoA) == 16 + MAX_ACTIVE_CLIPS * 5 * sizeof(int32_t), "GpuAudioSoA layout check failed");

} // namespace AviQtl::Engine::Timeline
