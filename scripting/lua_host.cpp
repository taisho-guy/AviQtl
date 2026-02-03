#include "lua_host.hpp"
#include <iostream>
#include <lua.hpp> // Standard Lua/LuaJIT header
#include <vector>
#include <cmath>
#include <QDebug>

namespace Rina::Scripting {

LuaHost &LuaHost::instance() {
    static LuaHost inst;
    return inst;
}

LuaHost::LuaHost() : L(nullptr) {
    initialize();
}

LuaHost::~LuaHost() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void LuaHost::initialize() {
    if (L)
        lua_close(L);
    L = luaL_newstate();
    luaL_openlibs(L);

    // Math shortcut (allow usage of sin() instead of math.sin())
    lua_getglobal(L, "math");
    lua_setglobal(L, "m");

    // Define standard math functions in global scope for convenience
    const char *math_shortcuts = "sin = math.sin; cos = math.cos; tan = math.tan; "
                                 "abs = math.abs; max = math.max; min = math.min; "
                                 "floor = math.floor; ceil = math.ceil; pi = math.pi; "
                                 "random = math.random;";

    if (luaL_dostring(L, math_shortcuts) != LUA_OK) {
        qWarning() << "[LuaHost] Failed to init math shortcuts:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }

    qDebug() << "[LuaHost] Initialized LuaJIT engine";
}

double LuaHost::evaluate(const std::string &expression, double time, int index, double currentValue) {
    if (!L)
        return currentValue;

    // Reset stack
    lua_settop(L, 0);

    // 1. Set Context Variables
    lua_pushnumber(L, time);
    lua_setglobal(L, "time");

    lua_pushnumber(L, time); // alias 't'
    lua_setglobal(L, "t");

    lua_pushinteger(L, index);
    lua_setglobal(L, "index");

    lua_pushnumber(L, currentValue);
    lua_setglobal(L, "value");

    lua_pushnumber(L, currentValue); // alias 'v'
    lua_setglobal(L, "v");

    // 2. Prepare Expression
    // "return " + expression
    std::string code = "return " + expression;

    // 3. Execute
    int ret = luaL_dostring(L, code.c_str());

    if (ret != LUA_OK) {
        const char *errMsg = lua_tostring(L, -1);
        qWarning() << "[LuaHost] Error evaluating:" << QString::fromStdString(expression) << "->" << errMsg;
        lua_pop(L, 1); // pop error
        return currentValue; // Fallback
    }

    // 4. Get Result
    if (!lua_isnumber(L, -1)) {
        // Result is not a number (nil, string, etc.)
        lua_pop(L, 1);
        return currentValue;
    }

    double result = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return result;
}

} // namespace Rina::Scripting
