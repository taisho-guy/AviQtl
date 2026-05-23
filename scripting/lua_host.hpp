#pragma once

#include <lua.hpp>
#include <string>

namespace AviQtl::Scripting {

class LuaHost {
  public:
    static LuaHost &instance();
    ~LuaHost();

    static double evaluate(const std::string &expression, double time, int index, double currentValue);

  private:
    LuaHost();
    void initialize();
    lua_State *L;

    // コピーを無効化
    LuaHost(const LuaHost &) = delete;
    LuaHost &operator=(const LuaHost &) = delete;
};

} // namespace AviQtl::Scripting