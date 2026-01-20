#include <lua.hpp>
#include <luajit.h> // LuaJIT固有ヘッダ
#include <iostream>
#include <vector>

namespace Rina::Scripting {

    // ユーザー操作対象の構造体 (16バイトアライメント推奨)
    struct ObjState {
        float x, y, z;
        float zoom;
        float alpha;
        // パディング等が必要な場合あり
    };

    class LuaHost {
        lua_State* L;
        ObjState* currentObj; // 現在処理中のオブジェクトへのポインタ

    public:
        LuaHost() {
            L = luaL_newstate();
            luaL_openlibs(L);
            setupFFI();
        }

        ~LuaHost() {
            lua_close(L);
        }

        void setupFFI() {
            // FFI定義
            if (luaL_dostring(L, R"(
                local ffi = require("ffi")
                ffi.cdef[[
                    typedef struct { float x, y, z; float zoom; float alpha; } ObjState;
                    void print_debug(const char* msg);
                ]]
                
                -- グローバルな obj テーブルを作成
                -- __index, __newindex を使って C++ のポインタにアクセスする
                _G.obj_ptr = nil -- C++側からセットされるポインタ
                
                local mt = {
                    __index = function(t, k)
                        if _G.obj_ptr == nil then return nil end
                        return _G.obj_ptr[k]
                    end,
                    __newindex = function(t, k, v)
                        if _G.obj_ptr ~= nil then
                            _G.obj_ptr[k] = v
                        end
                    end
                }
                _G.obj = setmetatable({}, mt)
            )")) {
                std::cerr << "Lua Init Error: " << lua_tostring(L, -1) << std::endl;
            }
        }

        // フレーム毎に呼ばれる
        void executeScript(const std::string& script, ObjState* target) {
            // ポインタをLua側に渡す (FFIキャスト)
            // 注: 本来はlua_pushlightuserdata等を使うが、FFIなので文字列や専用APIで渡す
            
            // 安全のためグローバル変数にキャストしたポインタをセットするヘルパー関数をLua側に用意しておき、
            // それをC++から呼ぶのが定石
            
            // 簡易実装: ポインタアドレスをLuaに渡してキャストさせる
            lua_getglobal(L, "set_context"); 
            // ... (詳細なバインディング実装が必要)
        }
    };
}
