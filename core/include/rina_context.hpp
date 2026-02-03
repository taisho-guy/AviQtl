#pragma once
#include <memory>

namespace Rina::Core {
class Context {
  public:
    static Context &instance() {
        static Context inst;
        return inst;
    }
    // DI (Dependency Injection) コンテナはここに配置する
  private:
    Context() = default;
};
} // namespace Rina::Core
