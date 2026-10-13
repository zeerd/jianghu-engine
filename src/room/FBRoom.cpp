/**
 * @file FBRoom.cpp
 * @brief implementation of the CFBRoom / CFBDoor classes.
 */

#include <windows.h>

#include "Global.h"
#include "FBRoom.h"
#include "FB.h"
#include "Npc.h"
#include "User.h"

//---------------------------------------------------------
// CFBDoor
//---------------------------------------------------------
int CFBDoor::enter_door(CChar* me)
{
    if (!m_parent_fb || m_parent_fb->is_destructed()) return 0;
    CFBRoom* r = m_parent_fb->load_fb_room(querystr("next_room"));
    if (!r) return 0;

    // 在me身上做一个下一房间门的标识
    me->set_temp("init_door", querystr("next_door"));
    if (!me->move(r)) {
        me->delete_temp("init_door");
        return 0;
    }
    me->delete_temp("init_door");

    return 1;
}

//---------------------------------------------------------
// CFBRoom
//---------------------------------------------------------
CFBRoom::CFBRoom()
{
    set_name("标准副本房间");
    m_box_num   = 0;
    m_parent_fb = NULL;
    // 副本房间只清物、不复位 NPC（防 boss 无限复活）：
    // CRoom::reset 在 clean_up_items 后据此标志提前返回。
    set("no_reset_npc", 1);
}

CFBRoom::CFBRoom(CFB* fb)
{
    set_name("标准副本房间");
    m_box_num   = 0;
    m_parent_fb = fb;
    set("no_reset_npc", 1);
}

CFBRoom::~CFBRoom()
{
    m_object_list.clear();
    m_parent_fb = NULL;
}

// 门工厂：产出携带父副本指针的副本出口（enter_door 经父副本按需加载房间）
CDoor* CFBRoom::new_door() { return new CFBDoor(m_parent_fb); }

// 增加NPC：副本房间内的 NPC 不允许拾起
CNpc* CFBRoom::add_npc(const char* base_name, int32_t bx, int32_t by,
                       int32_t ex, int32_t ey, int dir, int active, int ival)
{
    CNpc* obj = CRoom::add_npc(base_name, bx, by, ex, ey, dir, active, ival);
    obj->set("no_get", "副本NPC不允许拾起！");

    return obj;
}

int32_t CFBRoom::query_attr(int attr)
{
    switch (attr) {
        case IS_FBROOM:
            return 1;
    }

    return CRoom::query_attr(attr);
}

// 副本房间 setup：区域名取 base_name 前 8 字符，且不设 city（与普通房不同）
void CFBRoom::setup()
{
    if (_M.undefinedp("map")) _M.set("map", _M.querystr("base_name"));

    if (_M.undefinedp("area")) {
        char area_name[9];
        strncpy(area_name, _M.querystr("base_name"), 8)[8] = 0;
        set("area", area_name);  // 区域名
    }
}

// 副本房间拒绝一切指令动作（含 CRoom 的 "back" 返回总部——防越狱）
int CFBRoom::handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    return 0;
}

void CFBRoom::destructed()
{
    m_parent_fb = NULL;
    // 释放出口、箱子（门走 CRoom::m_door_list）
    remove_all_box();
    remove_all_doors();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:query_fb_parent() -> object | nil -
// 若本对象是副本房间(CFBRoom)且父FB存在，返回父FB对象
int l_query_fb_parent(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CFBRoom* fr = dynamic_cast<CFBRoom*>(obj);
    if (!fr || !fr->m_parent_fb) {
        lua_pushnil(L);
        return 1;
    }

    lua_bind_push_object(L, fr->m_parent_fb);
    return 1;
}
