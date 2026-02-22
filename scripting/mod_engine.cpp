#include "mod_engine.hpp"
#include "../ui/include/timeline_controller.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QVariant>

namespace Rina::Scripting {

// Lua から参照できるグローバルポインタ
static Rina::UI::TimelineController *g_ctrl = nullptr;

// C API Wrappers for HostApiTable
extern "C" {
static void api_log(const char *msg) {
    if (g_ctrl)
        g_ctrl->log(QString::fromUtf8(msg));
}
static void api_transport_play() {
    if (g_ctrl && !g_ctrl->transport()->isPlaying())
        g_ctrl->transport()->togglePlay();
}
static void api_transport_pause() {
    if (g_ctrl && g_ctrl->transport()->isPlaying())
        g_ctrl->transport()->togglePlay();
}
static void api_transport_toggle() {
    if (g_ctrl)
        g_ctrl->transport()->togglePlay();
}
static void api_transport_seek(int frame) {
    if (g_ctrl)
        g_ctrl->transport()->setCurrentFrame(frame);
}
static int api_transport_get_frame() { return (g_ctrl) ? g_ctrl->transport()->currentFrame() : 0; }
static int api_transport_is_playing() { return (g_ctrl) ? (int)g_ctrl->transport()->isPlaying() : 0; }

static void api_clip_create(const char *type, int start, int layer) {
    if (g_ctrl)
        g_ctrl->createObject(QString::fromUtf8(type), start, layer);
}
static void api_clip_delete(int id) {
    if (g_ctrl)
        g_ctrl->deleteClip(id);
}
static void api_clip_update(int id, int layer, int start, int dur) {
    if (g_ctrl)
        g_ctrl->updateClip(id, layer, start, dur);
}
static void api_clip_select(int id) {
    if (g_ctrl)
        g_ctrl->selectClip(id);
}

static int api_project_get_width() { return (g_ctrl) ? g_ctrl->project()->width() : 0; }
static int api_project_get_height() { return (g_ctrl) ? g_ctrl->project()->height() : 0; }
static double api_project_get_fps() { return (g_ctrl) ? g_ctrl->project()->fps() : 0.0; }

static void api_scene_create(const char *name) {
    if (g_ctrl)
        g_ctrl->createScene(QString::fromUtf8(name));
}
static void api_scene_switch(int id) {
    if (g_ctrl)
        g_ctrl->switchScene(id);
}

static void api_command_begin_group(const char *text) {
    if (g_ctrl && g_ctrl->timeline())
        g_ctrl->timeline()->undoStack()->beginMacro(QString::fromUtf8(text));
}
static void api_command_end_group() {
    if (g_ctrl && g_ctrl->timeline())
        g_ctrl->timeline()->undoStack()->endMacro();
}
}

static HostApiTable g_hostApi = {api_log,         api_transport_play, api_transport_pause,   api_transport_toggle,   api_transport_seek,  api_transport_get_frame, api_transport_is_playing, api_clip_create,         api_clip_delete,
                                 api_clip_update, api_clip_select,    api_project_get_width, api_project_get_height, api_project_get_fps, api_scene_create,        api_scene_switch,         api_command_begin_group, api_command_end_group};

// ヘルパー
static int _checkCtrl(lua_State *L) {
    if (!g_ctrl) {
        lua_pushstring(L, "[RinaAPI] controller not ready");
        lua_error(L);
    }
    return 0;
}

// transport
static int l_transport_play(lua_State *L) {
    _checkCtrl(L);
    if (!g_ctrl->transport()->isPlaying())
        g_ctrl->transport()->togglePlay();
    return 0;
}
static int l_transport_pause(lua_State *L) {
    _checkCtrl(L);
    if (g_ctrl->transport()->isPlaying())
        g_ctrl->transport()->togglePlay();
    return 0;
}
static int l_transport_toggle(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->transport()->togglePlay();
    return 0;
}
static int l_transport_seek(lua_State *L) {
    _checkCtrl(L);
    int frame = (int)luaL_checkinteger(L, 1);
    g_ctrl->transport()->setCurrentFrame(frame);
    return 0;
}
static int l_transport_get_frame(lua_State *L) {
    _checkCtrl(L);
    lua_pushinteger(L, g_ctrl->transport()->currentFrame());
    return 1;
}
static int l_transport_is_playing(lua_State *L) {
    _checkCtrl(L);
    lua_pushboolean(L, g_ctrl->transport()->isPlaying());
    return 1;
}

// clip
static int l_clip_create(lua_State *L) {
    _checkCtrl(L);
    // rina_clip_create(type, startFrame, layer)
    const char *type = luaL_checkstring(L, 1);
    int startFrame = (int)luaL_checkinteger(L, 2);
    int layer = (int)luaL_checkinteger(L, 3);
    g_ctrl->createObject(QString::fromUtf8(type), startFrame, layer);
    return 0;
}
static int l_clip_delete(lua_State *L) {
    _checkCtrl(L);
    int clipId = (int)luaL_checkinteger(L, 1);
    g_ctrl->deleteClip(clipId);
    return 0;
}
static int l_clip_update(lua_State *L) {
    _checkCtrl(L);
    // rina_clip_update(clipId, layer, startFrame, duration)
    int id = (int)luaL_checkinteger(L, 1);
    int layer = (int)luaL_checkinteger(L, 2);
    int start = (int)luaL_checkinteger(L, 3);
    int dur = (int)luaL_checkinteger(L, 4);
    g_ctrl->updateClip(id, layer, start, dur);
    return 0;
}
static int l_clip_select(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->selectClip((int)luaL_checkinteger(L, 1));
    return 0;
}
static int l_clip_split(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->splitClip((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}
static int l_clip_copy(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->copyClip((int)luaL_checkinteger(L, 1));
    return 0;
}
static int l_clip_cut(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->cutClip((int)luaL_checkinteger(L, 1));
    return 0;
}
static int l_clip_paste(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->pasteClip((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}
static int l_clip_list(lua_State *L) {
    _checkCtrl(L);
    QVariantList clips = g_ctrl->clips();
    lua_newtable(L);
    for (int i = 0; i < clips.size(); i++) {
        QVariantMap m = clips[i].toMap();
        lua_newtable(L);
        auto push = [&](const char *k, QVariant v) {
            lua_pushstring(L, k);
            if (v.typeId() == QMetaType::Int || v.typeId() == QMetaType::LongLong)
                lua_pushinteger(L, v.toInt());
            else if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Float)
                lua_pushnumber(L, v.toDouble());
            else
                lua_pushstring(L, v.toString().toUtf8().constData());
            lua_settable(L, -3);
        };
        push("id", m["id"]);
        push("type", m["type"]);
        push("layer", m["layer"]);
        push("startFrame", m["startFrame"]);
        push("duration", m["durationFrames"]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// effect
static int l_effect_add(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->addEffect((int)luaL_checkinteger(L, 1), QString::fromUtf8(luaL_checkstring(L, 2)));
    return 0;
}
static int l_effect_remove(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->removeEffect((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}
static int l_effect_set_param(lua_State *L) {
    _checkCtrl(L);
    // rina_effect_set_param(clipId, effectIndex, paramName, value)
    int clipId = (int)luaL_checkinteger(L, 1);
    int effectIndex = (int)luaL_checkinteger(L, 2);
    const char *key = luaL_checkstring(L, 3);
    QVariant val;
    if (lua_type(L, 4) == LUA_TNUMBER)
        val = lua_tonumber(L, 4);
    else if (lua_type(L, 4) == LUA_TBOOLEAN)
        val = (bool)lua_toboolean(L, 4);
    else
        val = QString::fromUtf8(lua_tostring(L, 4));
    g_ctrl->updateClipEffectParam(clipId, effectIndex, QString::fromUtf8(key), val);
    return 0;
}

// project
static int l_project_get_width(lua_State *L) {
    _checkCtrl(L);
    lua_pushinteger(L, g_ctrl->project()->width());
    return 1;
}
static int l_project_get_height(lua_State *L) {
    _checkCtrl(L);
    lua_pushinteger(L, g_ctrl->project()->height());
    return 1;
}
static int l_project_get_fps(lua_State *L) {
    _checkCtrl(L);
    lua_pushnumber(L, g_ctrl->project()->fps());
    return 1;
}
static int l_project_save(lua_State *L) {
    _checkCtrl(L);
    bool ok = g_ctrl->saveProject(QString::fromUtf8(luaL_checkstring(L, 1)));
    lua_pushboolean(L, ok);
    return 1;
}
static int l_project_load(lua_State *L) {
    _checkCtrl(L);
    bool ok = g_ctrl->loadProject(QString::fromUtf8(luaL_checkstring(L, 1)));
    lua_pushboolean(L, ok);
    return 1;
}

// undo/redo
static int l_undo(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->undo();
    return 0;
}
static int l_redo(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->redo();
    return 0;
}

// scene
static int l_scene_create(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->createScene(QString::fromUtf8(luaL_checkstring(L, 1)));
    return 0;
}
static int l_scene_remove(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->removeScene((int)luaL_checkinteger(L, 1));
    return 0;
}
static int l_scene_switch(lua_State *L) {
    _checkCtrl(L);
    g_ctrl->switchScene((int)luaL_checkinteger(L, 1));
    return 0;
}

// command
static int l_command_begin_group(lua_State *L) {
    _checkCtrl(L);
    const char *text = luaL_checkstring(L, 1);
    if (g_ctrl->timeline())
        g_ctrl->timeline()->undoStack()->beginMacro(QString::fromUtf8(text));
    return 0;
}
static int l_command_end_group(lua_State *L) {
    _checkCtrl(L);
    if (g_ctrl->timeline())
        g_ctrl->timeline()->undoStack()->endMacro();
    return 0;
}

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

void ModEngine::registerController(void *controller) {
    g_ctrl = static_cast<Rina::UI::TimelineController *>(controller);
    if (L) {
        _registerRinaAPI();

        // Export Host API Table
        lua_pushlightuserdata(L, &g_hostApi);
        lua_setglobal(L, "RINA_HOST_API");
    }
}

void ModEngine::_registerRinaAPI() {
    // transport
    lua_register(L, "rina_transport_play", l_transport_play);
    lua_register(L, "rina_transport_pause", l_transport_pause);
    lua_register(L, "rina_transport_toggle", l_transport_toggle);
    lua_register(L, "rina_transport_seek", l_transport_seek);
    lua_register(L, "rina_transport_get_frame", l_transport_get_frame);
    lua_register(L, "rina_transport_is_playing", l_transport_is_playing);
    // clip
    lua_register(L, "rina_clip_create", l_clip_create);
    lua_register(L, "rina_clip_delete", l_clip_delete);
    lua_register(L, "rina_clip_update", l_clip_update);
    lua_register(L, "rina_clip_select", l_clip_select);
    lua_register(L, "rina_clip_split", l_clip_split);
    lua_register(L, "rina_clip_copy", l_clip_copy);
    lua_register(L, "rina_clip_cut", l_clip_cut);
    lua_register(L, "rina_clip_paste", l_clip_paste);
    lua_register(L, "rina_clip_list", l_clip_list);
    // effect
    lua_register(L, "rina_effect_add", l_effect_add);
    lua_register(L, "rina_effect_remove", l_effect_remove);
    lua_register(L, "rina_effect_set_param", l_effect_set_param);
    // project
    lua_register(L, "rina_project_width", l_project_get_width);
    lua_register(L, "rina_project_height", l_project_get_height);
    lua_register(L, "rina_project_fps", l_project_get_fps);
    lua_register(L, "rina_project_save", l_project_save);
    lua_register(L, "rina_project_load", l_project_load);
    // undo/redo
    lua_register(L, "rina_undo", l_undo);
    lua_register(L, "rina_redo", l_redo);
    // scene
    lua_register(L, "rina_scene_create", l_scene_create);
    lua_register(L, "rina_scene_remove", l_scene_remove);
    lua_register(L, "rina_scene_switch", l_scene_switch);
    // command
    lua_register(L, "rina_command_begin_group", l_command_begin_group);
    lua_register(L, "rina_command_end_group", l_command_end_group);

    // rina.xxx() 形式のテーブルAPIをLua側で構築
    const char *rina_table = R"(
rina = {
    transport = {
        play       = rina_transport_play,
        pause      = rina_transport_pause,
        toggle     = rina_transport_toggle,
        seek       = rina_transport_seek,
        get_frame  = rina_transport_get_frame,
        is_playing = rina_transport_is_playing,
    },
    clip = {
        create = rina_clip_create,
        delete = rina_clip_delete,
        update = rina_clip_update,
        select = rina_clip_select,
        split  = rina_clip_split,
        copy   = rina_clip_copy,
        cut    = rina_clip_cut,
        paste  = rina_clip_paste,
        list   = rina_clip_list,
    },
    effect = {
        add       = rina_effect_add,
        remove    = rina_effect_remove,
        set_param = rina_effect_set_param,
    },
    project = {
        width        = rina_project_width,
        height       = rina_project_height,
        fps          = rina_project_fps,
        save         = rina_project_save,
        load         = rina_project_load,
    },
    scene = {
        create = rina_scene_create,
        remove = rina_scene_remove,
        switch = rina_scene_switch,
    },
    command = {
        begin_group = rina_command_begin_group,
        end_group = rina_command_end_group,
    },
    undo = rina_undo,
    redo = rina_redo,
}
)";
    // Lua の delete/switch は予約語なので _G 経由でアクセスする場合のみ注意
    // テーブルフィールドとしては問題なし (LuaJIT は許容する)
    luaL_dostring(L, rina_table);

    qInfo() << "[ModEngine] Rina Lua API registered.";
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