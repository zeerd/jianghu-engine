/**
 * @file ContainerItem.cpp
 * @brief CContainer: item list, present/lookup,
 * descriptions, say.  */

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

// self:Get_ItemList() -> item_list | nil（列表为空时返回 nil）
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

// self:present(id) -> object
int l_present(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    const char* id = luaL_checkstring(L, 2);
    lua_bind_push_object(L, c->present(id));
    return 1;
}

// self:Present(object_id) -> object
int l_present_by_id(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    int32_t obj_id = (int32_t)luaL_checkinteger(L, 2);
    int flag       = (int)luaL_optinteger(L, 3, 0);
    lua_bind_push_object(L, c->present(obj_id, flag));
    return 1;
}

// self:present_name(base_name, nType[, flag]) -> object|nil
int l_present_name(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    const char* base_name = luaL_checkstring(L, 2);
    int type              = (int)luaL_optinteger(L, 3, 0);
    int flag              = (int)luaL_optinteger(L, 4, 0);
    CContainer* r         = c->present_name(base_name, type, flag);
    if (r)
        lua_bind_push_object(L, r);
    else
        lua_pushnil(L);
    return 1;
}

// self:Long([raw]) -> string - 长描述
int l_long(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    int raw       = (int)luaL_optinteger(L, 2, 0);
    const char* s = c->long_desc(raw);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// self:Short([raw]) -> string - 短描述
int l_short(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    int raw       = (int)luaL_optinteger(L, 2, 0);
    const char* s = c->short_desc(raw);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// self:title([raw]) -> string - 称号
int l_title(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    int raw       = (int)luaL_optinteger(L, 2, 0);
    const char* s = c->title(raw);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// self:say(msg, me)
int l_say(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    IDialogSource* c = dynamic_cast<IDialogSource*>(obj);
    if (!c) return 0;

    const char* msg = luaL_checkstring(L, 2);
    CChar* me       = NULL;
    if (lua_isuserdata(L, 3)) me = dynamic_cast<CChar*>(get_object(L, 3));

    c->say(msg, me);
    return 0;
}
