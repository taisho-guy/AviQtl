#include "mod_engine.hpp"
#include <QCoreApplication>
#include <QDebug>

namespace Rina::Scripting {

ModEngine &ModEngine::instance() {
    static ModEngine inst;
    return inst;
}

ModEngine::~ModEngine() {
    if (L)
        lua_close(L);
}

void ModEngine::initialize(void *ecsPtr) {
    if (L)
        return;
    L = luaL_newstate();
    luaL_openlibs(L); // 全標準ライブラリ（io, os, debug, ffi等）を解放

    // 名前を "RINA_CORE_PTR" に統一して登録
    lua_pushlightuserdata(L, ecsPtr);
    lua_setglobal(L, "RINA_CORE_PTR");
    qInfo() << "[ModEngine] LuaJIT initialized. Core pointer registered as RINA_CORE_PTR";
}

void ModEngine::loadPlugins() {
    QString pluginsPath = QCoreApplication::applicationDirPath() + "/plugins";
    QDir dir(pluginsPath);

    if (!dir.exists()) {
        dir.mkpath(".");
        return;
    }

    QStringList filters;
    filters << "*.lua";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo &fileInfo : files) {
        qInfo() << "[ModEngine] Loading MOD:" << fileInfo.fileName();
        if (luaL_dofile(L, fileInfo.absoluteFilePath().toUtf8().constData())) {
            qCritical() << "[ModEngine] Load Error:" << lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    }
}

void ModEngine::onUpdate() {
    if (!L)
        return;
    lua_getglobal(L, "RinaUpdateHook");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != 0) {
            qCritical() << "[ModEngine] Hook Error:" << lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

} // namespace Rina::Scripting