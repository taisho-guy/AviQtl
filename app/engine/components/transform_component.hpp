#pragma once

namespace AviQtl {
namespace Engine {

struct TransformComponent {
  float x = 0.0f;
  float y = 0.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float rotation = 0.0f; // in degrees
  float opacity = 1.0f;  // 0.0f to 1.0f
};

} // namespace Engine
} // namespace AviQtl
