/**
 * @file ContainerDbase.cpp
 * @brief CContainer/CMainObject: dbase & temp-mapping
 * binds (methods inline in CObject.h).  */

#include <windows.h>
#include <algorithm>
#include "Container.h"
#include "Room.h"
#include "User.h"
#include "Item.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "FightRoom.h"
#include "Channel.h"
#include "Sql.h"
#include "LuaBindIncludes.h"

//==== methods ====

int CContainer::query_autoload() { return 0; }

CMapping* CContainer::query_entire_dbase() { return &_M; }

CMapping* CContainer::query_entire_temp_dbase() { return &_T; }

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:set(key, value)  value 可以是 number 或 string
int l_set(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    // 注意：不能用 lua_isnumber —— 它对可转数字的字符串（如 "1"）也返回 true，
    // 会把 set("owner", "1") 错存成数字，导致 querystr("owner") 读不到。
    if (lua_type(L, 3) == LUA_TNUMBER) {
        obj->set(key, (int32_t)lua_tointeger(L, 3));
    }
    else if (lua_isstring(L, 3)) {
        obj->set(key, lua_tostring(L, 3));
    }
    return 0;
}

// self:query(key) -> number
// key 可以是字符串（dbase 键）或数字（C++ 枚举属性如 IS_ITEM=3）
int l_query(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_pushinteger(L, obj->query_attr((int)lua_tointeger(L, 2)));
        return 1;
    }

    const char* key = luaL_checkstring(L, 2);
    lua_pushinteger(L, obj->query(key));
    return 1;
}

// self:querystr(key) -> string
int l_querystr(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    const char* val = obj->querystr(key);
    lua_pushstring(L, val ? val : "");
    return 1;
}

// self:add(key, value)
int l_add(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    int32_t val     = (int32_t)luaL_checkinteger(L, 3);
    obj->add(key, val);
    return 0;
}

// self:del(key)
int l_del(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    obj->del(key);
    return 0;
}

// self:set_name(name, id)
int l_set_name(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* name = luaL_checkstring(L, 2);
    const char* id   = lua_isstring(L, 3) ? lua_tostring(L, 3) : NULL;

    obj->set_name(name, id);
    return 0;
}

// self:set_temp(key, value)
int l_set_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    // 同上：lua_isnumber 会把 "1" 当数字，这里用 lua_type 精确区分
    if (lua_type(L, 3) == LUA_TNUMBER)
        obj->set_temp(key, (int32_t)lua_tointeger(L, 3));
    else if (lua_isstring(L, 3))
        obj->set_temp(key, lua_tostring(L, 3));
    return 0;
}

// self:query_temp(key) -> number
int l_query_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    lua_pushinteger(L, obj->query_temp(key));
    return 1;
}

// self:querymap_temp(key) -> userdata|nil  （仅判断存在性用）
int l_querymap_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CMapping* m     = obj->querymap_temp(key);
    if (m)
        lua_pushlightuserdata(L, (void*)m);
    else
        lua_pushnil(L);
    return 1;
}

// self:querystr_temp(key) -> string
int l_querystr_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    const char* val = obj->querystr_temp(key);
    lua_pushstring(L, val ? val : "");
    return 1;
}

// self:name([flag]) -> string
int l_name(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    int flag      = (int)luaL_optinteger(L, 2, 0);
    const char* n = obj->name(flag);
    lua_pushstring(L, n ? n : "");
    return 1;
}

// self:id([flag]) -> string
int l_id(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    int flag      = (int)luaL_optinteger(L, 2, 0);
    const char* n = obj->id(flag);
    lua_pushstring(L, n ? n : "");
    return 1;
}

// self:get_map(key) -> table|nil - 返回持久 dbase 子映射的 Lua
// 表副本（保留数值/字符串类型）
int l_get_map(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* key = luaL_checkstring(L, 2);
    CMapping* m     = ch->querymap(key);
    if (!m) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    for (std::unordered_map<std::string, CMapping::svalue_t>::iterator it
         = m->m_map.begin();
         it != m->m_map.end(); ++it) {
        if (it->second.type == T_NUMBER)
            lua_pushinteger(L, it->second.u.number);
        else if (it->second.type == T_STRING)
            lua_pushstring(L, it->second.u.string.get());
        else
            continue;
        lua_setfield(L, -2, it->first.c_str());
    }
    return 1;
}

// self:set_map(key, table) - 用 Lua 表整体设置持久 dbase 子映射
int l_set_map(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* key = luaL_checkstring(L, 2);
    if (lua_type(L, 3) != LUA_TTABLE) return 0;

    CMapping m;
    lua_pushnil(L);
    while (lua_next(L, 3) != 0) {
        // key 在 -2，value 在 -1
        const char* k = lua_tostring(L, -2);
        if (k) {
            if (lua_type(L, -1) == LUA_TNUMBER)
                m.set(k, (int32_t)lua_tointeger(L, -1));
            else if (lua_isstring(L, -1))
                m.set(k, lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    }
    ch->set(key, m);
    return 0;
}

// self:queryvec(key) -> table|nil - CVector 转 Lua 字符串数组
int l_queryvec(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CVector* v      = obj->queryvec(key);
    if (!v || v->count() <= 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, (int)v->count(), 0);
    for (long i = 0; i < v->count(); i++) {
        const char* s = (*v)[i];
        lua_pushstring(L, s ? s : "");
        lua_rawseti(L, -2, i + 1);  // Lua 数组从 1 开始
    }
    return 1;
}

// self:set_verbs(...) - 设置 CVector 字符串数组到 dbase（对应 C++ 的
// set("verbs", v)） 参数：set_verbs("verbs", {"bite", "claw"}) 或
// set_verbs("verbs", "bite", "claw") 也支持 set_verbs({"bite", "claw"}) 默认键
// "verbs"
int l_set_verbs(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = "verbs";
    int startIdx    = 2;

    // 第一参是字符串键名，第二参开始是元素或 table
    if (lua_isstring(L, 2) && !lua_istable(L, 3)) {
        key      = lua_tostring(L, 2);
        startIdx = 3;
    }
    else if (lua_isstring(L, 2) && lua_istable(L, 3)) {
        key      = lua_tostring(L, 2);
        startIdx = 3;
    }

    CVector v;
    // 收集元素：可能是单个 table，也可能是多个字符串参数
    if (lua_istable(L, startIdx)) {
        // table 形式：set_verbs("verbs", {"a", "b"}) 或 set_verbs({"a", "b"})
        int n = (int)lua_rawlen(L, startIdx);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, startIdx, i);
            if (lua_isstring(L, -1)) v.append(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    else {
        // 多个字符串参数：set_verbs("verbs", "a", "b")
        for (int i = startIdx; i <= lua_gettop(L); i++) {
            if (lua_isstring(L, i)) v.append(lua_tostring(L, i));
        }
    }

    obj->set(key, v);
    return 0;
}

// self:add_temp(key, value)
int l_add_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    int32_t val     = (int32_t)luaL_checkinteger(L, 3);
    obj->add_temp(key, val);
    return 0;
}

// self:delete_temp(key)
int l_delete_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    obj->delete_temp(key);
    return 0;
}

// self:querymap(key) -> boolean|nil - 持久 dbase 中的子映射是否存在
int l_querymap(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CMapping* m     = obj->querymap(key);
    if (m)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    return 1;
}

// self:querymap_keys(key) -> table|nil - 持久 dbase 子映射的 {key,value}
// 条目数组
int l_querymap_keys(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CContainer* con = dynamic_cast<CContainer*>(obj);
    if (!con) return 0;
    CMapping* m = con->querymap(key);
    if (!m) {
        lua_pushnil(L);
        return 1;
    }

    CVector v;
    m->keys(v);
    if (v.count() <= 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, (int)v.count(), 0);
    int idx = 1;
    for (long i = 0; i < v.count(); i++) {
        const char* k = v[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, k ? k : "");
        lua_setfield(L, -2, "key");
        const char* val = m->querystr(k);
        lua_pushstring(L, val ? val : "");
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// self:querymap_temp_keys(key) -> table|nil - 临时 dbase 子映射的 {key,value}
// 条目数组
int l_querymap_temp_keys(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CMapping* m     = obj->querymap_temp(key);
    if (!m) {
        lua_pushnil(L);
        return 1;
    }

    CVector v;
    m->keys(v);
    if (v.count() <= 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, (int)v.count(), 0);
    int idx = 1;
    for (long i = 0; i < v.count(); i++) {
        const char* k = v[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, k ? k : "");
        lua_setfield(L, -2, "key");
        lua_pushinteger(L, m->query(k));
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// self:dbase_defined(key) -> boolean - 持久 dbase 中 key 是否已定义
// （等价 C++ 的 !obj->query_entire_dbase()->undefinedp(key)）
int l_dbase_defined(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* key = luaL_checkstring(L, 2);
    CContainer* con = dynamic_cast<CContainer*>(obj);
    if (!con) return 0;
    CMapping* m = con->query_entire_dbase();
    if (!m) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, !m->undefinedp(key));
    return 1;
}
