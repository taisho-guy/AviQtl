#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Rina::Core {
// 全規格共通の抽象インターフェース
class IPluginInstance {
  public:
    virtual ~IPluginInstance() = default;
    virtual void process(float **inputs, float **outputs, size_t samples) = 0;
    virtual void setParameter(uint32_t index, float value) = 0;
    virtual int getParameterCount() const = 0;
    virtual std::string getParameterName(int index) const = 0;
    virtual float getParameterValue(int index) const = 0;
};

class PluginManager {
  public:
    static PluginManager &instance();
    std::unique_ptr<IPluginInstance> createInstance(const std::string &type, const std::string &path);

  private:
    PluginManager() = default;
};
} // namespace Rina::Core