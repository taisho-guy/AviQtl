#pragma once
#include <cstdint>

namespace AviQtl {
namespace Engine {

struct SceneComponent {
  int width = 1920;
  int height = 1080;
  double fps = 30.0;
  int64_t totalFrames = 1800; // 60 seconds at 30 fps
};

} // namespace Engine
} // namespace AviQtl
