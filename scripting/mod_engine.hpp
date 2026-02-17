#pragma once
#include <QDir>
#include <QString>
#include <lua.hpp>

namespace Rina::Scripting {

class ModEngine {
  public:
    static ModEngine &instance();

    void initialize(void *ecsPtr);
    void loadPlugins();
    void onUpdate();

    lua_State *state() { return L; }

  private:
    ModEngine() = default;
    ~ModEngine();
    lua_State *L = nullptr;
};

} // namespace Rina::Scripting