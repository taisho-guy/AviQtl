#pragma once

#include <lua.hpp>
#include <string>

namespace Rina::Scripting {

class LuaHost {
  public:
    static LuaHost &instance();
    ~LuaHost();

    // 数値表現の式を評価する
    // time: 現在の時間 (秒)
    // index: レイヤーインデックスまたはプロパティインデックス
    // currentValue: 現在のキーフレーム値
    static double evaluate(const std::string &expression, double time, int index, double currentValue);

  private:
    LuaHost();
    void initialize();
    lua_State *L;

    // コピーを無効化
    LuaHost(const LuaHost &) = delete;
    LuaHost &operator=(const LuaHost &) = delete;
};

} // namespace Rina::Scripting