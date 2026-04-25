#pragma once
#include <cstdint>
#include <memory>
#include <optional>

namespace AviQtl::Core {

enum class PixelFormat : uint8_t {
    Unknown = 0,
    RGBA8,
    BGRA8,
    NV12,
    YUV420P,
};

// CPU/GPU 両対応の映像フレームバッファ
// GPU Zero-copy パスは gpuTextureId を介して VA-API ハンドルを渡す
struct ImageBuffer {
    int width = 0;
    int height = 0;
    int stride = 0; // バイト/行
    PixelFormat format = PixelFormat::Unknown;
    std::shared_ptr<uint8_t[]> data;      // CPU バッファ (GPU専用の場合 nullptr)
    std::optional<uint32_t> gpuTextureId; // VA-API/EGL テクスチャID

    bool isValid() const { return width > 0 && height > 0; }

    // CPU バッファを確保して新規作成
    static ImageBuffer allocate(int w, int h, PixelFormat fmt) {
        ImageBuffer buf;
        buf.width = w;
        buf.height = h;
        buf.format = fmt;
        buf.stride = w * 4; // RGBA8/BGRA8 前提; NV12は呼び出し元で設定
        buf.data = std::shared_ptr<uint8_t[]>(new uint8_t[static_cast<size_t>(buf.stride * h)]);
        return buf;
    }
};

} // namespace AviQtl::Core
