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

// 末参作用域选袋："T" → 临时属性袋，nil/缺省 → 持久 dbase，其他值报错
// （写错落点比写错值更难排查，宁可拒绝）
static bool scope_is_temp(lua_State* L, int idx)
{
    if (lua_isnoneornil(L, idx)) return false;
    const char* scope = luaL_checkstring(L, idx);
    if (strcmp(scope, "T") == 0) return true;
    luaL_error(L, "unknown dbase scope: %s", scope);  // 不返回
    return false;
}

// self:set(key, value[, "T"])  value 可以是 number 或 string
int l_set(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    bool temp       = scope_is_temp(L, 4);
    // 注意：不能用 lua_isnumber —— 它对可转数字的字符串（如 "1"）也返回 true，
    // 会把 set("owner", "1") 错存成数字，导致 querystr("owner") 读不到。
    if (lua_type(L, 3) == LUA_TNUMBER) {
        mapping_value_t v = (mapping_value_t)lua_tointeger(L, 3);
        if (temp)
            obj->set_temp(key, v);
        else
            obj->set(key, v);
    }
    else if (lua_isstring(L, 3)) {
        const char* v = lua_tostring(L, 3);
        if (temp)
            obj->set_temp(key, v);
        else
            obj->set(key, v);
    }
    return 0;
}

// self:query(key[, "T"]) -> number
// key 可以是字符串（dbase 键）或数字（C++ 枚举属性如 IS_ITEM=3）
int l_query(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    if (scope_is_temp(L, 3)) {
        const char* key = luaL_checkstring(L, 2);
        lua_pushinteger(L, (lua_Integer)obj->query_temp(key));
        return 1;
    }

    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_pushinteger(L, obj->query_attr((int)lua_tointeger(L, 2)));
        return 1;
    }

    const char* key = luaL_checkstring(L, 2);
    lua_pushinteger(L, (lua_Integer)obj->query(key));
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

// self:add(key, value[, "T"])
int l_add(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key     = luaL_checkstring(L, 2);
    mapping_value_t val = (mapping_value_t)luaL_checkinteger(L, 3);
    if (scope_is_temp(L, 4))
        obj->add_temp(key, val);
    else
        obj->add(key, val);
    return 0;
}

// self:del(key[, "T"])
int l_del(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    const char* key = luaL_checkstring(L, 2);
    if (scope_is_temp(L, 3))
        obj->delete_temp(key);
    else
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

// ---- 持久 dbase 分派 self:dbase(action, key[, value]) ----
// 分支实现的栈位约定：1=self、2=action、3=key（set 的表在 4）

static int dbase_defined(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
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

static int dbase_map(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
    CMapping* m     = obj->querymap(key);
    if (m)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    return 1;
}

static int dbase_map_keys(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
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

static int dbase_vec(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
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

static int dbase_get(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
    CChar* ch       = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CMapping* m = ch->querymap(key);
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

static int dbase_set(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
    CChar* ch       = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    if (lua_type(L, 4) != LUA_TTABLE) return 0;

    CMapping m;
    lua_pushnil(L);
    while (lua_next(L, 4) != 0) {
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

// self:dbase(action, key[, value]) - 持久 dbase 查询/读写，ACTION 取
// defined（key 是否已定义）/ map（子映射存在性）/ map_keys（子映射
// {key,value} 条目）/ vec（CVector 转字符串数组）/ get（子映射整表
// 副本，保留数值/字符串类型）/ set（用 Lua 表整体设置子映射）。
// 未登记 ACTION 报错；get/set 仅 CChar，其余为容器级查询
int l_dbase(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* action = luaL_checkstring(L, 2);
    if (strcmp(action, "defined") == 0) return dbase_defined(L, obj);
    if (strcmp(action, "map") == 0) return dbase_map(L, obj);
    if (strcmp(action, "map_keys") == 0) return dbase_map_keys(L, obj);
    if (strcmp(action, "vec") == 0) return dbase_vec(L, obj);
    if (strcmp(action, "get") == 0) return dbase_get(L, obj);
    if (strcmp(action, "set") == 0) return dbase_set(L, obj);
    return luaL_error(L, "unknown dbase action: %s", action);
}

// ---- 临时 dbase 分派 self:dbase_temp(action, key) ----

static int dbase_temp_map(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
    CMapping* m     = obj->querymap_temp(key);
    if (m)
        lua_pushlightuserdata(L, (void*)m);
    else
        lua_pushnil(L);
    return 1;
}

static int dbase_temp_map_keys(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
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

static int dbase_temp_str(lua_State* L, CMainObject* obj)
{
    const char* key = luaL_checkstring(L, 3);
    const char* val = obj->querystr_temp(key);
    lua_pushstring(L, val ? val : "");
    return 1;
}

// self:dbase_temp(action, key) - 临时 dbase 查询，ACTION 取
// map（子映射存在性，返回 lightuserdata 仅作存在性判断）/
// map_keys（子映射 {key,value} 条目，value 取数值）/ str（字符串值，
// 无值返回 ""）；未登记 ACTION 报错
int l_dbase_temp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    const char* action = luaL_checkstring(L, 2);
    if (strcmp(action, "map") == 0) return dbase_temp_map(L, obj);
    if (strcmp(action, "map_keys") == 0) return dbase_temp_map_keys(L, obj);
    if (strcmp(action, "str") == 0) return dbase_temp_str(L, obj);
    return luaL_error(L, "unknown dbase_temp action: %s", action);
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
