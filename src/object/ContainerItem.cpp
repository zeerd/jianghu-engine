/**
 * @file ContainerItem.cpp
 * @brief CContainer: item list, present/lookup,
 * descriptions, say.  */

#include <windows.h>
#include <algorithm>
#include <cstring>
#include "Container.h"
#include "Room.h"
#include "User.h"
#include "Item.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "FightRoom.h"
#include "Channel.h"
#include "IClientObject.h"
#include "Sql.h"

//==== methods ====

DTItemList* CContainer::get_item_list() { return &m_item_list; }

// 是否存在于物品列表
CContainer* CContainer::present(int32_t obj_id, int flag)
{
    for (auto ob : m_item_list) {
        if (!flag && !ob->visible()) continue;
        if (ob->object_id() == obj_id) return ob;
    }
    return 0;
}

// 找到物品中满足基本名的物件。nType可以是IS_CHAR
CContainer* CContainer::present_name(const char* base_name, int type, int flag)
{
    for (auto obj : m_item_list) {
        if (!flag && !obj->visible()) continue;
        if (EQUALSTR(obj->querystr("base_name"), base_name)
            && obj->query_attr(type))
            return obj;
    }

    return 0;
}

// 找到id一致的物件 "xiao er"
CContainer* CContainer::present(const char* id, int flag)
{
    for (auto obj : m_item_list) {
        if (!flag && !obj->visible()) continue;
        if (EQUALSTR(obj->querystr("id"), id)) {
            return obj;
        }
    }

    return 0;
}

const char* CContainer::query_short()
{
    const char* m_short_desc = querystr("short");
    if (m_short_desc[0]) return m_short_desc;
    return name();
}

// 返回外观描述
const char* CContainer::long_desc(int raw)
{
    const char* des = querystr_temp("apply/long");

    if (raw || !des[0]) {
        des = querystr("long");
    }

    return des;
}

const char* CContainer::short_desc(int raw)
{
    static char msg[255];
    char mask[255];

    strncpy(msg, querystr("short"), 255)[254] = 0;
    if (!msg[0]) {
        snprintf(msg, 255, "%s(%s)", name(raw), id(raw));
    }

    if (!is_character()) return msg;

    strncpy(mask, querystr_temp("apply/short"), 255)[254] = 0;
    if (!raw && mask[0])
        strcpy(msg, mask);
    else {
        char nickname[40];

        strcpy(mask, msg);

        snprintf(nickname, 40, "「%s」", querystr("nickname"))[39] = 0;
        if (strlen(nickname) < 5) nickname[0] = 0;

        if (querymap("party")) {
            snprintf(msg, 255, "%s%s %s %s %s %s", querystr("party/party_name"),
                     querystr("party/rank"), querystr("title"),
                     querystr_temp("c_title"), nickname, mask);
        }
        else
            snprintf(msg, 255, "%s %s %s %s", querystr("title"),
                     querystr_temp("c_title"), nickname, mask);
    }

    return msg;
}

const char* CContainer::title(int raw)
{
    const char* ti = querystr_temp("apply/title");
    if (raw || !ti[0]) {
        // 增加对帮派的支持
        const char* c_ti = querystr_temp("c_title");
        if (c_ti[0]) {
            ti = c_ti;
        }
        else
            ti = querystr("title");
    }

    return ti;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:get_item_list() -> item_list | nil（列表为空时返回 nil）
int l_get_item_list(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    DTItemList* list = c->get_item_list();
    if (!list || list->empty()) {
        lua_pushnil(L);
        return 1;
    }

    DTItemList** ud = (DTItemList**)lua_newuserdata(L, sizeof(DTItemList*));
    *ud             = list;
    luaL_getmetatable(L, "jianghu_item_list");
    lua_setmetatable(L, -2);
    return 1;
}

// self:present(kind, id, ...) -> object|nil - 按 KIND 查容器内已装载对象：
//   id(原型 id 字符串)
//   object_id(对象 id 数值[, flag])，flag=1 连不可见一起找
//   name(base_name[, type[, flag]])，按类型过滤
// 未登记 KIND 报错
int l_present(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    const char* kind = luaL_checkstring(L, 2);
    if (strcmp(kind, "id") == 0) {
        const char* id = luaL_checkstring(L, 3);
        lua_bind_push_object(L, c->present(id));
    }
    else if (strcmp(kind, "object_id") == 0) {
        int32_t obj_id = (int32_t)luaL_checkinteger(L, 3);
        int flag       = (int)luaL_optinteger(L, 4, 0);
        lua_bind_push_object(L, c->present(obj_id, flag));
    }
    else if (strcmp(kind, "name") == 0) {
        const char* base_name = luaL_checkstring(L, 3);
        int type              = (int)luaL_optinteger(L, 4, 0);
        int flag              = (int)luaL_optinteger(L, 5, 0);
        CContainer* r         = c->present_name(base_name, type, flag);
        if (r)
            lua_bind_push_object(L, r);
        else
            lua_pushnil(L);
    }
    else {
        return luaL_error(L, "unknown present kind: %s", kind);
    }
    return 1;
}

// self:desc(kind[, raw]) -> string - 描述查询，KIND 取 long/short/title，
// 未登记 KIND 报错
int l_desc(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    const char* kind = luaL_checkstring(L, 2);
    int raw          = (int)luaL_optinteger(L, 3, 0);
    const char* s    = nullptr;
    if (strcmp(kind, "long") == 0)
        s = c->long_desc(raw);
    else if (strcmp(kind, "short") == 0)
        s = c->short_desc(raw);
    else if (strcmp(kind, "title") == 0)
        s = c->title(raw);
    else
        return luaL_error(L, "unknown desc kind: %s", kind);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// self:say(msg, me[, tips])
int l_say(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) {
        lua_pushboolean(L, 0);
        return 1;
    }

    IDialogSource* c = dynamic_cast<IDialogSource*>(obj);
    if (!c) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char* msg = luaL_checkstring(L, 2);
    CChar* me       = NULL;
    if (lua_isuserdata(L, 3)) me = dynamic_cast<CChar*>(get_object(L, 3));

    const char* tips = lua_isstring(L, 4) ? lua_tostring(L, 4) : NULL;

    lua_pushboolean(L, c->say(msg, me, tips) != 0);
    return 1;
}
