#include <windows.h>

#include "FightRoom.h"
#include "Combatd.h"
#include "Global.h"
#include "Room.h"
#include "User.h"
#include "PartyConfig.h"

#include "LuaBindEnums.h"

void set_heart_beat_obj(CContainer* obj, int flag);

bool CFightRoom::is_threshold_match() { return m_threshold_match; }
void CFightRoom::begin_fight(int flag, int hp_check)
{
    try {
        m_threshold_match = flag;
        m_hp_check        = hp_check;
        m_tick            = 0;
        turn              = 1;
        set_heart_beat_obj(this, 1);
    } catch (...) {
        LOG_ERROR("CFightRoom::Begin_Fight:flag-%ld:hp_check-%ld", flag,
                  hp_check);
    }

    return;
}

void CFightRoom::create(int flag)
{
    set("is_fightroom", 1);
    set("area", "fight");
    set("no_clean_up", 1);
    set("no_get", 1);
    turn = 0;
    tick = 0;
    if (flag == 1) m_fighters_max = 8;
    CItem::create(flag);  // 基类缺省补全钩子（hooks.object_create）
    return;
}

int CFightRoom::can_join(int team, CChar* me)
{
    int ret = 0;
    try {
        if (is_threshold_match()) return false;
        if (m_Team[team].fighter_num < m_fighters_max) return true;
    } catch (...) {
        LOG_ERROR("CFightRoom::Can_Join:team-%ld:me-%s", team, me->name());
    }

    return ret;
}

void CFightRoom::get_info(char* str, CChar* me)
{
    char tmp[512], msg[255];
    try {
        snprintf(tmp, 255, "N=%s&I=%s&P=%ld&a=%d&U=%ld&W=%ld&", name(), id(),
                 query("icon"), query_amount(), query_attr(CChar::UPDATE),
                 weight());

        if (EQUALSTR(querystr("status"), "locked")) {
            strcat(tmp, snprintf(msg, 40, "l=%s&", querystr("reason")));
        }

        strcat(tmp, snprintf(msg, 40, "t=%s&", title()));
        strcat(tmp, "F=1&");
        get_team_msg(tmp, 0);
        get_team_msg(tmp, 1);

        strcat(str, tmp);
    } catch (...) {
        LOG_ERROR("CFightRoom::GetInfo:str-%s:me-%s", str, me->name());
    }
    return;
}

void CFightRoom::on_leave(CContainer* me)
{
    try {
        if (me->query_attr(IS_CHAR)) do_leave_team(static_cast<CChar*>(me));
        CItem::on_leave(me);
    } catch (...) {
        LOG_ERROR("CFightRoom::OnLeave:me-%s", me->name());
    }

    return;
}

int CFightRoom::do_flee(CChar* me)
{
    int ret = 0;
    try {
        char msg[512];
        message_vision(snprintf(msg, 255, "$N慌慌张张地逃离了%s。", name()),
                       me);
        me->move(environment());
    } catch (...) {
        LOG_ERROR("CFightRoom::Do_Flee:me-%s", me->name());
    }

    return ret;
}

char* CFightRoom::get_team_msg(char* msg, int team)
{
    try {
        char tmp[512];
        bool bng = false;
        snprintf(tmp, 255, "T%c=Team %d：", team == 0 ? 'A' : 'B', team);
        int n = 0;
        for (n = 0; n < MAXFIGHTERS; n++) {
            if (m_Team[team].obj[n] != NULL) {
                if (bng) strcat(tmp, ";");
                strcat(tmp, m_Team[team].obj[n]->name());
                bng = true;
            }
        }
        strcat(tmp, "&");
        strcat(msg, tmp);

    } catch (...) {
        LOG_ERROR("CFightRoom::GetTeamMsg:msg-%s:team-%ld", msg, team);
    }
    return msg;
}

// 队伍成员选择
int CFightRoom::get_team_count(int team)
{
    if (team != 0) team = 1;
    return m_Team[team].fighter_num;
}

std::unique_ptr<TeamList> CFightRoom::get_team_list(int team, CChar* target,
                                                    int num)
{
    int tmp1[MAXFIGHTERS];
    for (int i = 0; i < MAXFIGHTERS; i++) {
        tmp1[i] = 0;
    }
    if (team < 0 || team > 1 || target) {
        if (!target) return NULL;
        team = -1;
        for (int t = 0; t < 2 && team == -1; t++) {
            for (int i = 0; i < MAXFIGHTERS; i++) {
                if (m_Team[t].obj[i] == target) {
                    team    = t;
                    tmp1[i] = 1;
                    break;
                }
            }
        }
        if (team == -1) return NULL;
    }
    if (num < 1) return NULL;

    auto tmp = std::make_unique<TeamList>();
    int sn   = 0;
    if (target) {
        tmp->push_back(target);
        sn++;
    }

    if (num >= get_team_count(team)) {
        for (int i = 0; i < MAXFIGHTERS; i++) {
            if (m_Team[team].obj[i] != NULL && m_Team[team].obj[i] != target)
                tmp->push_back(m_Team[team].obj[i]);
        }
    }
    else {
        int turntimes = 3;
        while (sn < num && turntimes) {
            for (int i = 0; i < MAXFIGHTERS; i++) {
                if (m_Team[team].obj[i] != NULL && g_utils.random(2) == 1) {
                    if (!tmp1[i]) {
                        tmp->push_back(m_Team[team].obj[i]);
                        tmp1[i] = 1;
                        sn++;
                    }
                }
            }
            turntimes--;
        }
        if (sn < num) {
            for (int i = 0; i < MAXFIGHTERS; i++) {
                if (m_Team[team].obj[i] != NULL) {
                    if (!tmp1[i]) {
                        tmp->push_back(m_Team[team].obj[i]);
                        sn++;
                        if (sn == num) break;
                    }
                }
            }
        }
    }

    return tmp;
}

CChar* CFightRoom::select_member(int team)
{
    CChar* ret = NULL;
    try {
        int idx = g_utils.random((long)m_Team[team].fighter_num);
        if (idx >= m_Team[team].fighter_num) idx = m_Team[team].fighter_num - 1;
        int num = 0;
        for (int i = 0; i < MAXFIGHTERS; i++) {
            if (m_Team[team].obj[i] != NULL) {
                if (num == idx) {
                    return m_Team[team].obj[i];
                }
                num++;
            }
        }
    } catch (...) {
        LOG_ERROR("CFightRoom::Select_Member:team-%ld", team);
    }
    return ret;
}

void CFightRoom::leave_team(CChar* obj) { do_leave_team(obj); }

// 清除某队槽位：结算经验提示、扣人数并删除战斗临时变量
void CFightRoom::clear_team_slot(int team, int idx, CChar* obj)
{
    if (is_user(obj)) {
        char msg[512];
        tell_object(
            obj,
            snprintf(msg, 255, "$YEL此次战斗耗时%d秒，获得%d点经验。",
                     g_current_time - obj->query_temp("fight/join_time"),
                     obj->query_exp() - obj->query_temp("fight/join_exp")));
    }
    m_Team[team].obj[idx] = NULL;
    m_Team[team].fighter_num--;
    obj->delete_temp("fight/is_fighting");
    obj->delete_temp("fight/fight_team");
    obj->delete_temp("fight/ap");
    obj->delete_temp("fight/join_time");
    obj->delete_temp("fight/join_exp");
    obj->delete_temp("pending/PK");  // 战斗结束，删除PK标记
}

void CFightRoom::do_leave_team(CChar* obj)
{
    try {
        for (int i = 0; i < MAXFIGHTERS; i++) {
            for (int t = 0; t < 2; t++) {
                if (m_Team[t].obj[i] == obj) clear_team_slot(t, i, obj);
            }
        }
        obj->evaluate_call_out_combat();
        obj->send_state(false);
        obj->send_state();
        obj->flush_my_info();

        {
            DTItemList* t = environment()->get_item_list();
            for (auto ob2 : *t) {
                if (ob2->query_attr(IS_USER)) download((CChar*)ob2);
            }
        }

    } catch (...) {
        LOG_TRACE("CFightRoom::Do_LeaveTeam:obj", obj->name());
    }

    return;
}

int32_t CFightRoom::query_attr(int attr)
{
    switch (attr) {
        case CAN_GET:
            return 0;
        case IS_FIGHTROOM:
            return 1;
    }

    return CItem::query_attr(attr);
}

void CFightRoom::end_fight()
{
    try {
        DTItemList search;
        CContainer* env;
        // CMapping * m;
        // int count = 0;
        env = environment();
        if (!env) env = load_room("room_void");

        copy_list(&search, get_item_list());
        try {
            for (auto item2 : search) {
                if (item2->query_attr(IS_CHAR)) do_leave_team((CChar*)item2);
                if (query("mine")
                    && !is_user(item2)) {  // 如果是地雷战斗，只有玩家出战斗
                    destruct(item2);
                    continue;
                }
                if (!item2->move(env) && !is_user(item2)) destruct(item2);
            }
        } catch (...) {
            ;
        }
        char msg[512];
        message_vision(snprintf(msg, 255, "%s结束了。", name()), this);
        set_heart_beat_obj(this, 0);
        DESTRUCT_OB(this, "CFightRoom::EndFight");
    } catch (...) {
        LOG_TRACE("CFightRoom::End_Fight");
    }
    return;
}

// 两队全员累积本回合 AP：首回合翻倍，500 封顶，busy 者不增长
void CFightRoom::advance_ap()
{
    for (int i = 0; i < MAXFIGHTERS; i++) {
        for (int t = 0; t < 2; t++) {
            CChar* c = m_Team[t].obj[i];
            if (c == NULL) continue;
            int ap = c->query_temp("fight/ap");
            int tp = query_turn_ap(c) / 2;
            if (turn == 1) {
                tp *= 2;
            }
            if (ap < 500) {
                ap += tp;
                if (ap > 500) tp = tp - (ap - 500);
            }
            else {
                tp = 0;
            }
            if (!c->is_busy()) c->add_temp("fight/ap", tp);
        }
    }
}

// 执行 myTeam 一侧的回合动作；返回 false 表示战斗提前结束，不再推进回合
bool CFightRoom::run_team_turn(int myTeam)
{
    const int enemy_team = 1 - myTeam;
    for (int i = 0; i < MAXFIGHTERS; i++) {
        CChar* self = m_Team[myTeam].obj[i];
        if (self == NULL) continue;
        CChar* obj = select_member(enemy_team);
        if (!obj) {
            // 都死了战斗快要结束了
            m_tick++;
            if (m_tick >= 3) {
                end_fight();
                return false;
            }
            break;
        }
        m_tick = 0;
        if (is_threshold_match()) {
            int wimpy_ratio = self->query("hp") * 100 / self->query("max_hp");
            if (wimpy_ratio <= m_hp_check) {
                CChar* enemy = m_Team[enemy_team].obj[0];
                if (enemy) {
                    enemy->is_defeated(enemy, self);
                    self->is_defeated(enemy, self);
                }
                if (enemy) self->command("halt");
                return false;
            }
        }
        if (!self->is_busy()) self->attack(obj);
    }
    return true;
}

void CFightRoom::heart_beat()
{
    if (tick < 1) {
        tick++;
        return;
    }
    tick = 0;
    try {
        if (m_Team[0].fighter_num == 0 && m_Team[1].fighter_num == 0) {
            end_fight();
            return;
        }
        // 截镖等战斗或比试没人马上结束
        if (!EQUALSTR(id(), "fight_room") || is_threshold_match()) {
            if (m_Team[0].fighter_num == 0 || m_Team[1].fighter_num == 0) {
                end_fight();
                return;
            }
        }
        advance_ap();

        const int acting_team = turn % 2 ? 0 : 1;
        if (!run_team_turn(acting_team)) return;
        turn++;
        CItem::heart_beat();
    } catch (...) {
        LOG_ERROR("CFightRoom::heart_beat");
    }
}

int CFightRoom::join_team(CChar* ob, int Team)  // Join_Team(me, FIGHT_TEAM_A);
{
    int ret = true;
    try {
        const int team = (Team == FIGHT_TEAM_B)   ? 1
                         : (Team == FIGHT_TEAM_A) ? 0
                                                  : -1;
        if (team >= 0) {
            if (m_Team[team].fighter_num == m_fighters_max) return false;
            while (m_Team[team].obj[m_Team[team].num] != NULL) {
                m_Team[team].num++;
                if (m_Team[team].num == MAXFIGHTERS) m_Team[team].num = 0;
            }
            m_Team[team].obj[m_Team[team].num] = ob;
            m_Team[team].fighter_num++;
        }
        ob->set_temp("fight/is_fighting", 1);
        ob->set_temp("fight/fight_team", Team);
        ob->set_temp("fight/join_time", g_current_time);
        ob->set_temp("fight/join_exp", ob->query_exp());
        ob->set_temp("fight/ap", ob->query_dex() / 2);
        ob->send_state(false);
        ob->send_state();
        ob->flush_my_info();
        char msg[255];

        if (!is_threshold_match() && !m_single) {
            if (CPartyConfig::auto_follow_fight() && is_user(ob)
                && ob->query_temp("party/number")) {
                DTItemList search;
                CContainer* env;
                env = environment();

                copy_list(&search, env->get_item_list());

                for (auto obj2 : search) {
                    if (!is_living(obj2)) continue;
                    if (obj2 == ob) continue;
                    if (obj2->environment() != env) continue;  // 已经移动
                    // 检查队伍中的跟随情况
                    if (ob->query_temp(snprintf(msg, 255, "party/friend_%s",
                                                obj2->querystr("id")))
                        && obj2->query_temp("party/follow")) {
                        ((CChar*)obj2)
                            ->command(snprintf(msg, 255, "join %ld %d",
                                               object_id(), Team));
                    }
                }

                search.clear();
            }
            else if (CPartyConfig::auto_follow_fight() && is_user(ob)) {
                if (ob->querystr_temp("party/master")[0]
                    && ob->query_temp("party/follow")) {
                    CChar* pary_master = (CChar*)find_online(
                        ob->querystr_temp("party/master"));
                    if (pary_master
                        && environment()->present(pary_master->id(1), 1)) {
                        pary_master->command(snprintf(msg, 255, "join %ld %d",
                                                      object_id(), Team));
                    }
                }
            }
        }
        // 通知同一环境内玩家，增加了物件。
        {
            DTItemList* t = environment()->get_item_list();
            for (auto obj2 : *t) {
                if (obj2->query_attr(IS_USER)) download((CChar*)obj2);
            }
        }
    } catch (...) {
        LOG_TRACE("CFightRoom::Join_Team:ob-%s:team-%ld", ob->name(), Team);
    }
    return ret;
}

CFightRoom::CFightRoom()
{
    // set_name("");
    try {
        set("infinity", 1);  // 不受容量限制
        set("max_items", 20);
        m_fighters_max        = 4;
        m_Team[0].fighter_num = 0;
        m_Team[1].fighter_num = 0;
        m_Team[0].num         = 0;
        m_Team[1].num         = 0;
        for (int i = 0; i < MAXFIGHTERS; i++) {
            m_Team[0].obj[i] = NULL;
            m_Team[1].obj[i] = NULL;
        }
        m_threshold_match = false;
        m_single          = false;
        m_tick            = 0;
        turn              = 0;
        tick              = 0;
        m_hp_check        = 0;
    } catch (...) {
        LOG_ERROR("CFightRoom::CFightRoom");
    }
}

CFightRoom::~CFightRoom() {}

int32_t CFightRoom::query_turn_ap(CChar* me)
{
    int32_t ret = 2;
    try {
        if (!me->is_busy() && me->query_attr(IS_LIVING)) ret = me->query_dex();
    } catch (...) {
        LOG_TRACE("CFightRoom::Query_Turn_AP:me-%s", me->name());
    }
    if (ret < 2) ret = 2;
    return ret;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:Select_Member(team) -> object - 选取战斗房间某队伍成员
int l_select_member(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    int team = (int)luaL_optinteger(L, 2, 0);
    lua_bind_push_object(L, fr->select_member(team));
    return 1;
}

// self:Can_Join(team) -> boolean - 战斗房间是否可加入该队
int l_can_join(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) {
        lua_pushboolean(L, 0);
        return 1;
    }
    int team = (int)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, fr->can_join(team) ? 1 : 0);
    return 1;
}

// self:Join_Team(ob, team)
int l_join_team(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    CChar* ob = NULL;
    if (lua_isuserdata(L, 2)) ob = dynamic_cast<CChar*>(get_object(L, 2));
    int team = (int)luaL_optinteger(L, 3, 0);
    if (ob) fr->join_team(ob, team);
    return 0;
}

// self:Begin_Fight([flag[, hp_check]]) - 开始战斗
int l_begin_fight(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    int flag     = (int)luaL_optinteger(L, 2, 0);
    int hp_check = (int)luaL_optinteger(L, 3, 50);
    fr->begin_fight(flag, hp_check);
    return 0;
}

// self:set_single(flag) - 设置单人作战标志（CFightRoom::m_single）
int l_set_single(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    fr->m_single = lua_toboolean(L, 2) != 0;
    return 0;
}

// self:End_Fight() - 结束战斗
int l_end_fight(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    fr->end_fight();
    return 0;
}

// self:Leave_Team(ob) - 战斗房间中离开队伍
int l_leave_team(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    if (!fr) return 0;
    CChar* ob = NULL;
    if (lua_isuserdata(L, 2)) ob = dynamic_cast<CChar*>(get_object(L, 2));
    if (ob) fr->leave_team(ob);
    return 0;
}

// self:is_fight_room() -> boolean - 是否战斗房间
int l_is_fight_room(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CFightRoom* fr = dynamic_cast<CFightRoom*>(obj);
    lua_pushboolean(L, fr ? 1 : 0);
    return 1;
}

// get_team_list(me, team, target, num) -> table|nil
// 返回战斗房间中指定队伍的成员数组（最多 num 个）。
// team 传 -1 时自动取 target 所在队伍的敌对队伍；target 可为 nil。
// 与 CFightRoom::getTeamList 语义一致。
int l_get_team_list(lua_State* L)
{
    CMainObject* mo = get_object(L, 1);
    CChar* me       = mo ? dynamic_cast<CChar*>(mo) : NULL;
    if (!me) {
        lua_pushnil(L);
        return 1;
    }
    int team      = (int)luaL_optinteger(L, 2, -1);
    CChar* target = NULL;
    if (lua_isuserdata(L, 3)) target = dynamic_cast<CChar*>(get_object(L, 3));
    int num        = (int)luaL_optinteger(L, 4, 4);
    CFightRoom* fr = dynamic_cast<CFightRoom*>(me->environment());
    if (!fr) {
        lua_pushnil(L);
        return 1;
    }
    auto list = fr->get_team_list(team, target, num);
    if (!list) {
        lua_pushnil(L);
        return 1;
    }
    int cnt = (int)list->size();
    if (cnt <= 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, cnt, 0);
    for (int i = 0; i < cnt; i++) {
        CChar* enemy = (*list)[i];
        lua_bind_push_object(L, enemy);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// get_fight_team_num(me) - 返回 me 所在战斗队伍的人数
int l_get_fight_team_num(lua_State* L)
{
    CChar* me = lua_check_char(L, 1);
    if (!me) {
        lua_pushinteger(L, 0);
        return 1;
    }
    CFightRoom* fr = dynamic_cast<CFightRoom*>(me->environment());
    if (!fr) {
        lua_pushinteger(L, 0);
        return 1;
    }
    int team = me->query_temp("fight/fight_team");
    lua_pushinteger(L, fr->get_team_count(team));
    return 1;
}

// get_fight_team_char(me, i) -> obj  - 返回 me 所在战斗队伍第 i 个成员
int l_get_fight_team_char(lua_State* L)
{
    CChar* me = lua_check_char(L, 1);
    if (!me) {
        lua_pushnil(L);
        return 1;
    }
    CFightRoom* fr = dynamic_cast<CFightRoom*>(me->environment());
    if (!fr) {
        lua_pushnil(L);
        return 1;
    }
    int team  = me->query_temp("fight/fight_team");
    int idx   = (int)luaL_checkinteger(L, 2);
    auto list = fr->get_team_list(team);
    if (!list) {
        lua_pushnil(L);
        return 1;
    }
    CChar* target = (*list)[idx];
    if (target)
        lua_bind_push_object(L, target);
    else
        lua_pushnil(L);
    return 1;
}

// FightRoom.h 战斗锁宏 → Lua 全局常量
static const LuaEnumEntry kFightLockEnums[] = {
    {"FIGHT_LOCKED", FIGHT_LOCKED},
    {"FIGHT_NO_FLEE", FIGHT_NO_FLEE},
    {"FIGHT_NO_GUARD", FIGHT_NO_GUARD},
    {nullptr, 0},
};

// FightRoom.h 战队枚举 → Lua 全局常量
static const LuaEnumEntry kFightTeamEnums[] = {
    {"FIGHT_TEAM_A", CFightRoom::FIGHT_TEAM_A},
    {"FIGHT_TEAM_B", CFightRoom::FIGHT_TEAM_B},
    {nullptr, 0},
};

void lua_bind_reg_fight_room_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kFightLockEnums);
    lua_bind_reg_enums(L, kFightTeamEnums);
}
