/**
 * @file FB.cpp
 * @brief implementation of the CFB class (副本总控).
 */

#include <windows.h>

#include "Global.h"
#include "FB.h"
#include "FBRoom.h"
#include "User.h"
#include "fightroom.h"

#include "Sql.h"

void set_heart_beat_obj(CContainer* obj, int flag);
std::unique_ptr<CFBRoom> create_fb_room(string room_name, CFB* fb);

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CFB::CFB()
{
    CItem::CItem();
    m_destruct = false;
    m_room_num = 0;
    m_nodie    = 0;
    set("infinity", 1);  // 不受容量限制
    set("max_items", 50);
    set("no_fight", 1);
    set("no_cast", 1);
    set("area", "fuben");
    set("no_clean_up", 1);
    m_tick = 0;
}

void CFB::destructed()
{
    set_destructed();
    remove_all_room();
}

CFB::~CFB()
{
    for (auto obj : m_user_list) {
        delete obj;
    }
    m_user_list.clear();
}

void CFB::create(int flag) { return; }

void CFB::remove_all_room()
{
    for (auto room : m_room_list) {
        destruct(room);
    }
    m_room_list.clear();
    m_room_num = 0;
}

int CFB::do_look(CChar* me)
{
    char tmp[20];
    if (querystr("long")[0]) {
        char msg[1024];

        snprintf(msg, 1024, "这是：%s，内有房间：%s\n具体描述：\n%s", name(),
                 chinese_number(m_room_num, tmp), querystr("long"));
        say(msg, me);
    }
    else {
        char msg[255];
        snprintf(msg, 255, "这是：%s，内有房间：%s\n没有特殊描述", name(),
                 chinese_number(m_room_num, tmp));
        say(msg, me);
    }
    return 0;
}

CFBRoom* CFB::load_fb_room(const char* room_name)
{
    for (auto room : m_room_list) {
        if (EQUALSTR(room->querystr("base_name"), room_name)) {
            return room;
        }
    }

    std::unique_ptr<CFBRoom> room = create_fb_room(room_name, this);

    CFBRoom* obj = room.get();
    m_room_list.push_front(room.release());
    obj->set("base_name", room_name);
    obj->create();
    obj->setup();
    if (!m_room_num) set("start_map", room_name);
    m_room_num++;
    return obj;
}

int CFB::move_in(CChar* me, int party)
{
    // 直接扔进第一个场景
    if (me && m_room_num && querystr("start_map") && querystr("start_map")[0]) {
        DTItemList search;
        copy_list(&search, me->get_item_list());
        for (auto obj : search) {
            if (is_user(obj))
                obj->move(load_room(g_start_room));
            else if (obj->query_attr(IS_FIGHTROOM))
                obj->move(load_room(g_start_room));
        }
        search.clear();
        if (m_nodie) me->set_temp("fb_nodie", 1);
        int ret = me->move(load_fb_room(querystr("start_map")));
        if (ret) {
            desc(me);

            int found = 0;

            for (auto obj : m_user_list) {
                if (obj->id == me->id(1)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                USER_NODE* user = new USER_NODE;

                user->id    = me->id(1);
                user->party = party;
                m_user_list.push_front(user);
            }
        }
        return ret;
    }
    return 0;
}

void CFB::init(CChar* me) {}

int32_t CFB::query_attr(int attr)
{
    switch (attr) {
        case CAN_GET:
            return 0;

        case IS_FB:
            return 1;
    }

    return CItem::query_attr(attr);
}

void CFB::reset() {}

void CFB::begin_fb(const char* source_map, int limit_time, int flag)
{
    try {
        m_nodie = flag;
        if (limit_time) set("limit_time", limit_time);
        set("source_map", source_map);
        set("begin_time", g_current_time);
        m_tick = 0;
        set_heart_beat_obj(this, 1);
    } catch (...) {
        LOG_ERROR("CFB::begin_fb");
    }

    return;
}

void CFB::end_fb()
{
    try {
        char msg[255];

        CMapping* m = querymap("player");
        if (m != NULL) {
            CVector v;
            m->keys(v);

            for (int i = 0; i < v.count(); i++) {
                snprintf(msg, 80, "player/%s", v[i]);
                sql_update_record(v[i], 1, query(msg), "", true);
            }
        }

        DTItemList search;
        CContainer* env;

        if (querystr("source_map") && querystr("source_map")[0])
            env = load_room(querystr("source_map"));
        else
            env = load_room(g_start_room);

        copy_list(&search, get_item_list());
        try {
            for (auto item2 : search) {
                if (!is_user(item2)
                    && !item2->query_attr(
                        IS_FIGHTROOM)) {  // 只有玩家和战斗移到别的地图
                    destruct(item2);
                    continue;
                }
                item2->move(env);
            }
        } catch (...) {
            ;
        }
        search.clear();

        // 把副本中所有房间的人和战斗也移出去

        for (auto room : m_room_list) {
            copy_list(&search, room->get_item_list());

            for (auto item2 : search) {
                if (!is_user(item2)
                    && !item2->query_attr(
                        IS_FIGHTROOM)) {  // 只有玩家和战斗移到别的地图
                    destruct(item2);
                    continue;
                }
                item2->move(env);
            }
            search.clear();
        }

        set_heart_beat_obj(this, 0);
        DESTRUCT_OB(this, "CFB::end_fb");
    } catch (...) {
        LOG_ERROR("CFB::end_fb");
    }
    return;
}

void CFB::heart_beat()
{
    // 首先判断该副本是不是有时间限制，且到时间了
    int limit_time = query("limit_time");
    CContainer* env;
    DTItemList search;
    char msg[255];

    if ((limit_time && (g_current_time - limit_time) > query("begin_time"))
        || query("need_end")) {
        m_tick++;  // 30秒后销毁
    }
    else if (m_tick < 15) {  // 15秒后无论是否有人，都无法继续

        m_tick++;
        for (auto room : m_room_list) {
            copy_list(&search, room->get_item_list());
            int found = 0;

            for (auto item2 : search) {
                if (is_user(item2)
                    || item2->query_attr(
                        IS_FIGHTROOM)) {  // 只有玩家和战斗移到别的地图
                    m_tick = 0;
                    found  = 1;
                    break;
                }
            }
            search.clear();
            if (found) break;
        }
    }
    else {
        m_tick++;
    }

    if ((limit_time && ((g_current_time - limit_time) % 60) == 0)) {
        int32_t t = (query("begin_time") + limit_time - g_current_time) / 60;
        for (auto room : m_room_list) {
            tell_room(room, snprintf(msg, 255,
                                     "$HIR任务副本距离关闭还有%d分钟。", t));
        }
    }

    // 开始销毁计时
    if (m_tick) {
        for (auto room : m_room_list) {
            tell_room(room,
                      snprintf(msg, 255, "$HIR任务副本即将关闭，还有%d秒。",
                               30 - m_tick));

            if (m_tick >= 15) {
                room->set("no_fight", 1);
                room->set("no_cast", 1);
                copy_list(&search, room->get_item_list());

                for (auto item2 : search) {
                    if (item2->query_attr(
                            IS_FIGHTROOM)) {  // 只有玩家和战斗移到别的地图
                        CFightRoom* fr = (CFightRoom*)item2;
                        fr->end_fight();
                    }
                }
                search.clear();
            }
        }
        if (m_tick >= 30) {
            end_fb();
            return;
        }
    }

    if (querystr("source_map") && querystr("source_map")[0])
        env = load_room(querystr("source_map"));
    else
        env = load_room(g_start_room);

    copy_list(&search, get_item_list());
    try {
        for (auto item2 : search) {
            if (!is_user(item2)
                && !item2->query_attr(
                    IS_FIGHTROOM)) {  // 只有玩家和战斗移到别的地图
                destruct(item2);
                continue;
            }
            item2->move(env);  // 没有成功进入副本，直接扔回去
        }
    } catch (...) {
        ;
    }
    search.clear();
}

void CFB::change_room(CContainer* me)
{
    CItem::change_room(me);
    me->delete_temp("client/loading");
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:Begin_FB(source_map, limit_time, flag) - 门派副本开始
int l_begin_fb(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFB* fb = dynamic_cast<CFB*>(obj);
    if (!fb) return 0;
    const char* src = luaL_checkstring(L, 2);
    int limit       = (int)luaL_optinteger(L, 3, 0);
    int flag        = (int)luaL_optinteger(L, 4, 0);
    fb->begin_fb(src, limit, flag);
    return 0;
}

// self:MoveIn(me[, party]) - 门派副本移入玩家
int l_move_in(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFB* fb = dynamic_cast<CFB*>(obj);
    if (!fb) return 0;
    CMainObject* mo = get_object(L, 2);
    CChar* me       = mo ? dynamic_cast<CChar*>(mo) : NULL;
    if (!me) return 0;
    int party = (int)luaL_optinteger(L, 3, 0);
    fb->move_in(me, party);
    return 0;
}

// self:load_fb_room(room_name) -> object - 副本总控(CFB)加载一个副本房间
int l_load_fb_room(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFB* fb = dynamic_cast<CFB*>(obj);
    if (!fb) return 0;
    const char* room_name = luaL_checkstring(L, 2);
    char roomBuf[256];
    strncpy(roomBuf, room_name, 255);
    roomBuf[255] = 0;
    lua_bind_push_object(L, fb->load_fb_room(roomBuf));
    return 1;
}

// load_fb(name[, flag]) -> object - 加载江湖门派副本
int l_load_fb(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    int flag         = (int)luaL_optinteger(L, 2, 0);
    lua_bind_push_object(L, g_item_mgr->load_fb(name, flag));
    return 1;
}
