#pragma once
#include <cstdint>

namespace AviQtl {
namespace Engine {

struct TimeRangeComponent {
  int64_t startFrame = 0;
  int64_t durationFrames = 150; // 5 seconds at 30 fps
  int layer = 1;
};

} // namespace Engine
} // namespace AviQtl
