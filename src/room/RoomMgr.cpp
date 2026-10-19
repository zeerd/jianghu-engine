/**
 * @file RoomMgr.cpp
 * @brief 房间管理  */

#include "RoomMgr.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "Room.h"
#include "../util/Logger.h"

#include <cstdio>

//---------------------------------------------------------
// 房间加载
//---------------------------------------------------------
std::unique_ptr<CRoom> create_room(string base_name);
CRoom* load_room(const char* base_name)
{
    iterator_room p = g_room_list.find(base_name);
    if (p != g_room_list.end()) return p->second;

    std::unique_ptr<CRoom> r = create_room(base_name);
    CRoom* obj               = r.release();
    g_room_list[base_name]   = obj;
    obj->set("base_name", base_name);
    obj->create();
    obj->setup();
    return obj;
}

//---------------------------------------------------------
// 房间释放
//---------------------------------------------------------
void room_release()
{
    iterator_room p;
    for (p = g_room_list.begin(); p != g_room_list.end(); p++) {
        (p->second)->set_heart_beat(0);
        delete p->second;
    }

    g_room_list.clear();
}

//---------------------------------------------------------
// 房间删除
//---------------------------------------------------------
void remove_room(string room_name) { g_room_list.erase(room_name); }

//---------------------------------------------------------
// 导出房间地图（调试）
//---------------------------------------------------------
void draw_map()
{
    iterator_room p;
    FILE* fp;
    fp = fopen("map.txt", "wt");
    if (!fp) return;

    for (p = g_room_list.begin(); p != g_room_list.end(); p++)
        fprintf(fp, "\"%s\",\n", p->first.c_str());

    fclose(fp);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// load_room(name) -> object - 加载房间
int l_load_room(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    lua_bind_push_object(L, load_room(name));
    return 1;
}

// global_room_name(i) -> string - 全局房间名（0 基索引，动态列表）
int l_global_room_name(lua_State* L)
{
    int i            = (int)luaL_checkinteger(L, 1);
    const char* name = g_LuaEngine->get_room_name_by_index(i);
    lua_pushstring(L, name);
    return 1;
}

// room_count() -> number - 全局房间总数（动态）
int l_room_count(lua_State* L)
{
    lua_pushinteger(L, g_LuaEngine->get_room_count());
    return 1;
}

// get_my_house(who) -> object|nil - 获取玩家的住宅房间
int l_get_my_house(lua_State* L)
{
    CChar* ch = dynamic_cast<CChar*>(get_object(L, 1));
    if (!ch || !ch->querystr("resident")[0]) {
        lua_pushnil(L);
        return 1;
    }

    char room_name[255];
    snprintf(room_name, sizeof(room_name), "house_%s_%s",
             ch->querystr("resident"), ch->id(1));
    CRoom* room = load_room(room_name);
    if (!room || room->query("fail_ob")) {
        lua_pushnil(L);
        return 1;
    }

    lua_bind_push_object(L, room);
    return 1;
}
