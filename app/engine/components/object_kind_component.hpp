#pragma once
#include <string>

namespace AviQtl {
namespace Engine {

enum class ObjectKind { Video, Audio, Image, Text, Shape };

struct ObjectKindComponent {
  ObjectKind kind = ObjectKind::Video;
  std::string name;
};

} // namespace Engine
} // namespace AviQtl
