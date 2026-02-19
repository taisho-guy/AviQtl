#pragma once
#include <QDir>
#include <QString>
#include <lua.hpp>

namespace Rina::Scripting {

class ModEngine {
  public:
    static ModEngine &instance();

    void initialize(void *ecsPtr);
    // TimelineController を登録 (main.cpp の QML登録後に呼ぶ)
    void registerController(void *controller);
    void loadPlugins();
    void onUpdate();

    lua_State *state() { return L; }

  private:
    ModEngine() = default;
    ~ModEngine();
    lua_State *L = nullptr;
    void _registerRinaAPI();
};

} // namespace Rina::Scripting