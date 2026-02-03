#include "lua_host.hpp"
#include <QDebug>
#include <cmath>
#include <iostream>
#include <lua.hpp> // Standard Lua/LuaJIT header
#include <vector>

namespace Rina::Scripting {

LuaHost &LuaHost::instance() {
    static LuaHost inst;
    return inst;
}

LuaHost::LuaHost() : L(nullptr) { initialize(); }

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

    // mathライブラリへのショートカット (math.sin() の代わりに m.sin() を許可)
    lua_getglobal(L, "math");
    lua_setglobal(L, "m");

    // 利便性のため、標準的な数学関数をグローバルスコープに定義
    const char *math_shortcuts = "sin = math.sin; cos = math.cos; tan = math.tan; "
                                 "abs = math.abs; max = math.max; min = math.min; "
                                 "floor = math.floor; ceil = math.ceil; pi = math.pi; "
                                 "random = math.random;";

    if (luaL_dostring(L, math_shortcuts) != LUA_OK) {
        qWarning() << "[LuaHost] 数学ショートカットの初期化に失敗:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }

    qDebug() << "[LuaHost] LuaJITエンジンを初期化しました";
}

double LuaHost::evaluate(const std::string &expression, double time, int index, double currentValue) {
    if (!L)
        return currentValue;

    // Reset stack
    // スタックをリセット
    lua_settop(L, 0);

    // 1. コンテキスト変数を設定
    lua_pushnumber(L, time);
    lua_setglobal(L, "time");

    lua_pushnumber(L, time); // エイリアス 't'
    lua_setglobal(L, "t");

    lua_pushinteger(L, index);
    lua_setglobal(L, "index");

    lua_pushnumber(L, currentValue);
    lua_setglobal(L, "value");

    lua_pushnumber(L, currentValue); // エイリアス 'v'
    lua_setglobal(L, "v");

    // 2. 式を準備
    // "return " + expression
    std::string code = "return " + expression;

    // 3. 実行
    int ret = luaL_dostring(L, code.c_str());

    if (ret != LUA_OK) {
        const char *errMsg = lua_tostring(L, -1);
        qWarning() << "[LuaHost] 式の評価エラー:" << QString::fromStdString(expression) << "->" << errMsg;
        lua_pop(L, 1);       // エラーメッセージをポップ
        return currentValue; // フォールバック
    }

    // 4. 結果を取得
    if (!lua_isnumber(L, -1)) {
        // 結果が数値ではない (nil, stringなど)
        lua_pop(L, 1);
        return currentValue;
    }

    double result = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return result;
}

} // namespace Rina::Scripting
