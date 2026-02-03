#pragma once

#include <string>
#include <lua.hpp>

namespace Rina::Scripting {

class LuaHost {
public:
    static LuaHost& instance();
    ~LuaHost();

    // Evaluate a numeric expression
    // time: current time in seconds
    // index: layer index or property index
    // currentValue: current keyframed value
    double evaluate(const std::string& expression, double time, int index, double currentValue);

private:
    LuaHost();
    void initialize();
    lua_State* L;
    
    // Disable copy
    LuaHost(const LuaHost&) = delete;
    LuaHost& operator=(const LuaHost&) = delete;
};

} // namespace Rina::Scripting