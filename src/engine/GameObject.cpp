/**
 * @file GameObject.cpp
 * @brief 跨类型通用对象管理  */

#include "GameObject.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "User.h"
#include "Npc.h"
#include "Item.h"
#include "Room.h"
#include "Channel.h"
#include "FightRoom.h"
#include "../util/Logger.h"

//---------------------------------------------------------
// 随机取物品栏某物
//---------------------------------------------------------

//---------------------------------------------------------
// 角色查询
//---------------------------------------------------------
int is_living(CContainer* me) { return me->query_attr(CChar::IS_LIVING); }

int wiz_level(CChar* me) { return me->query("grant"); }

//---------------------------------------------------------
// 地图实时位置
//---------------------------------------------------------
void set_live_point(CContainer* obj, int32_t x, int32_t y)
{
    obj->set_temp("map/bx", x);
    obj->set_temp("map/by", y);
    obj->set_temp("map/ex", x);
    obj->set_temp("map/ey", y);
    obj->set_temp("map/ac", 0);
}

void set_live_dest(CContainer* obj, int32_t x, int32_t y)
{
    obj->set_temp("map/ex", x);
    obj->set_temp("map/ey", y);
}

//---------------------------------------------------------
// 比试
//---------------------------------------------------------
CFightRoom* do_bishi(CChar* me, CChar* victim, char* name, int check)
{
    CFightRoom* room
        = static_cast<CFightRoom*>(g_item_mgr->load_item("fight_room"));
    room->m_single = true;
    room->set_name(name);
    set_live_dest(room, me->query_temp("map/ex"), me->query_temp("map/ey"));

    room->move(me->environment());

    me->move(room);
    room->join_team(me, CFightRoom::FIGHT_TEAM_A);

    victim->move(room);
    room->join_team(victim, CFightRoom::FIGHT_TEAM_B);

    room->begin_fight(1, check);
    return room;
}

//---------------------------------------------------------
// 通用查找
//---------------------------------------------------------
CContainer* find_object(int32_t id, int flag)
{
    CContainer* it = 0;

    it = find_npc(id);
    if (!it) it = find_player(id);
    if (!flag)
        if (!it) it = g_item_mgr->find_item(id);

    return it;
}

//---------------------------------------------------------
// 对象销毁
//---------------------------------------------------------
void remove_user(int32_t id);
void remove_npc(int32_t id);
void remove_item(int32_t id);
void remove_room(string room_name);

void destruct_with_logs(PCContainer me, const char* function)
{
    if (!me) return;

#ifdef MORE_LOGS
    char env[40] = "(NULL)";
    if (me->environment())
        strncpy(env, (me->environment())->name(), 30)[30] = 0;
    LOG_TRACE("DESTRUCT: #OBJ=%ld, NAME=%s, ENV=%s FUN=%s\n", me->object_id(),
              me->name(), env, function);
#endif

    destruct(me);
}

void destruct(PCContainer me)
{
    if (!me) return;

    me->set_destructed();
    me->set_heart_beat(0);
    me->clean_up();
    me->move(NULL);

    g_last_destruct_object = me->object_id();

    if (me->query_attr(CUser::IS_USER)) {
        LOG_DEBUG("%d", g_last_destruct_object);
        remove_user(g_last_destruct_object);
        g_channel.remove_snoop(static_cast<CChar*>(me));
        g_channel.loginlist.add(get_ip(static_cast<CChar*>(me)), -1);
        static_cast<CUser*>(me)->set_online_flag(0);
    }
    else if (me->query_attr(CNpc::IS_NPC)) {
        remove_npc(g_last_destruct_object);
    }
    else if (me->query_attr(CChar::IS_ROOM)
             && !me->query_attr(CChar::IS_FBROOM)) {
        remove_room(me->querystr("base_name"));
    }
    else {
        g_item_mgr->remove_item(g_last_destruct_object);
    }

    me->destructed();
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.push_back(me);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// destruct(ob) - 销毁对象
int l_destruct(lua_State* L)
{
    CContainer* ob = NULL;
    if (lua_isuserdata(L, 1)) ob = dynamic_cast<CContainer*>(get_object(L, 1));
    if (ob) destruct(ob);
    return 0;
}
