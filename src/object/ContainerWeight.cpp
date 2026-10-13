/**
 * @file ContainerWeight.cpp
 * @brief CContainer: weight/encumbrance, amount,
 * equip checks, UpdateMe.  */

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

//==== methods ====

void CContainer::set_weight(int32_t value)
{
    if (!environment()) {
        m_weight = value;
        return;
    }

    if (value != m_weight) environment()->add_encumbrance(value - m_weight);

    m_weight = value;
}

int32_t CContainer::query_weight() { return m_weight; }

int32_t CContainer::weight() { return m_weight + encumb; }

int32_t CContainer::query_max_encumbrance() { return max_encumb; }

void CContainer::set_max_encumbrance(int32_t e) { max_encumb = e; }

void CContainer::add_encumbrance(int32_t w)
{
    encumb += w;
    if (encumb < 0) {
        LOG_ERROR("Object %s(%s) encumbrance underflow. env: %s w: %ld",
                  name(1), querystr("id"),
                  (environment() ? environment()->name(1) : "NULL"), w);
    }

    if (encumb > max_encumb) over_encumbrance();
    if (environment()) environment()->add_encumbrance(w);
}

void CContainer::over_encumbrance()
{
    if (query("infinity") || !query_attr(IS_USER)) return;
    LOG_DEBUG("Object %s(%s) over encumbrance!", name(1), querystr("id"));
}

int32_t CContainer::query_encumb() { return encumb; }

int32_t CContainer::query_amount() { return 1; }

int32_t CContainer::query_max_amount() { return 1; }

void CContainer::set_amount(int32_t amount) {}

int32_t CContainer::add_amount(int32_t amount) { return amount; }

int CContainer::over_encumbranced() { return encumb > max_encumb; }

int CContainer::is_character() { return query_attr(IS_CHAR); }

// 加入/移除心跳链表
void CContainer::set_heart_beat(int flag) { set_heart_beat_obj(this, flag); }

// 更新自身
void CContainer::update_me()
{
    char msg[255];

    m_update++;

    if (!environment()) return;

    // 通知谁呢？先通知自己吧。
    if (query_attr(IS_USER)) {
        static_cast<CChar*>(this)->flush_my_info();
    }

    snprintf(msg, 255, "downloadobject %d", object_id());

    // 再通知环境
    if (environment()->query_attr(IS_CHAR)) {
        static_cast<CChar*>(environment())->command(msg);
    }

    // 加入新人需要通知环境内其他角色下载该物件
    message_vision_room(this, "execute", msg);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:set_weight(weight)
int l_set_weight(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    int32_t w = (int32_t)luaL_checkinteger(L, 2);
    c->set_weight(w);
    return 0;
}

// self:Query_Amount() -> number
int l_query_amount(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    lua_pushinteger(L, c->query_amount());
    return 1;
}

// self:Query_Weight() -> number
int l_query_weight(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    lua_pushinteger(L, c->query_weight());
    return 1;
}

// self:is_character() -> boolean
int l_is_character(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    lua_pushboolean(L, c->is_character());
    return 1;
}

// self:can_equipped() -> boolean
int l_can_equipped(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    lua_pushboolean(L, c->can_equipped());
    return 1;
}

// self:UpdateMe() - 刷新玩家属性（力量/攻击等重新计算后调用）
int l_update_me(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    c->update_me();
    return 0;
}
