#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Rina::Core {

using ParamValue = std::variant<bool, int, double, std::string>;
using ParamMap = std::unordered_map<std::string, ParamValue>;

struct PackageInfo {
    std::string id, name, version, description;
    bool installed = false;
};

} // namespace Rina::Core
