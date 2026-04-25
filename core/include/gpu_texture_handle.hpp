#pragma once
#include <cstdint>

namespace AviQtl::Core {

// サポートするグラフィックスAPIの種類
enum class GpuApiType { Unknown, OpenGL, Vulkan, D3D11, D3D12, Metal };

// 抽象テクスチャハンドル
// 異なるグラフィックスAPI間のテクスチャ共有を目的とする。
struct GpuTextureHandle {
    GpuApiType api;     // テクスチャが属するAPI
    uint64_t textureId; // GLuint, VkImage, etc. (void* の代わりに64bit整数で)
    int width;
    int height;
    int format; // API固有のフォーマット値
};

} // namespace AviQtl::Core