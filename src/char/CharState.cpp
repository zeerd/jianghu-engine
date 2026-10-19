/**
 * @file CharState.cpp
 * @brief implementation of CChar (CharState). */

#include <windows.h>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Skill.h"
#include "CmdHandle.h"
#include "Room.h"
#include "Condition.h"
#include "Channel.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "AutoFight.h"
#include "CombatConfig.h"
#include "ServerConfig.h"
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"

// 死了
void CChar::die()
{
    CChar* killer;
    CChar* owner;

    g_combat_announce.announce(this, "dead");
    if (!query_attr(IS_LIVING)) revive(1);

    del("condition");  // 清除所有condition
    delete_temp("condition");

    if (query_temp("fb_nodie") && userp(this))  // 非死亡副本
    {
        remove_all_killer();

        set("hp", 1);
        set("eff_hp", query("max_hp") / 2);
        set("mp", 0l);
        message_vision("$N身负重伤，无法再继续战斗，只能回家休养了。", this);
        delete_temp("team/dungeon");

        CRoom* room = load_room(querystr("start_city"));
        if (room) {
            move(room);
        }
        else {
            LOG_ERROR("用户%s没有环境%ld", name(1), query("start_city"));
            move(load_room(g_start_room));
        }

        flush_my_info();
        flush_my_skill(this);
        return;
    }

    // 救命金丹
    if ((query("kill/flee") - query("kill/death")) > 0) {
        add("kill/death", 1);

        remove_all_killer();

        set("hp", 1);
        set("eff_hp", query("max_hp") / 2);
        set("mp", 0l);

        drop_all(this);
        message_vision(
            "$N似乎有神灵保佑，垂死之下转而复生，竟拖着伤重的身体，向远方逃去"
            "。",
            this);
        move(load_room(querystr("start_city")));
        return;
    }
    add("death_times", 1);  // 死亡次数记录

    // 死亡惩罚
    if ((killer = (CChar*)find_object(query_temp("last_damage_from"), 1))) {
        // 检查owner
        owner = (CChar*)find_object(query_temp("fight/owner"), 1);
        if (owner && owner->query_temp("fight/will_nk") == object_id())
            killer = owner;

        if (killer->querystr("owner") != "") {
            owner = (CChar*)find_online(killer->querystr("owner"));
            if (owner) {
                killer = owner;
            }
        }

        g_reward.killer_reward(killer, this);
        killer->remove_killer(this);
        // 给存盘
        if (userp(killer)) killer->save();

        killer->finish_kill(this);
    }
    else {
        // 没有killer，如中毒
        if (query_attr(IS_USER)) {
            g_reward.victim_penalty(this);
        }
    }

    CContainer* env = this;
    while (env->environment()) env = env->environment();

    if (!env->query("PK")) CCorpse::make_corpse(this, killer);

    if (userp(this)) save();

    if (query_attr(IS_USER)) {
        remove_all_killer();

        set("hp", 1);
        set("eff_hp", query("max_hp") / 2);
        set("mp", 0l);

        CRoom* room = load_room(querystr("start_city"));
        if (room) {
            move(room);
        }
        else {
            LOG_ERROR("用户%s没有环境%ld", name(1), query("start_city"));
        }

        flush_my_info();
        flush_my_skill(this);
    }
    else
        DESTRUCT_OB(this, "CChar::die");
}

// 昏迷
void CChar::unconcious()
{
    if (!query_attr(IS_LIVING)) return;

    message_vision_room(this, "tell", "你的眼前一黑，接着什么也不知道了．．．");
    disable_player("昏迷不醒");
    set("hp", 0L);
    set("mp", 0L);
    set_temp("block_msg/all", 1);
    g_combat_announce.announce(this, "unconcious");
    m_revive = g_utils.random(100 - query("con")) + 30;  // 30多心跳后苏醒
}

// 苏醒
void CChar::revive(int quiet)
{
    m_revive = 0;

    while (environment()->query_attr(IS_CHAR))
        move(environment()->environment());

    enable_player();
    if (!quiet) {
        g_combat_announce.announce(this, "revive");
        delete_temp("block_msg/all");
        delete_temp("last_fainted_from");
        message_vision_room(this, "tell", "慢慢地你终于又有了知觉．．．");
    }
    else
        delete_temp("block_msg/all");
}

// 发送角色状态
// exe-打坐 dis-disable状态 net-断线 poi-中毒 idl-发呆
void CChar::send_state(bool broadcast)
{
    // 非广播时只向自己私发；没有客户端的对象 send_command 必然是空操作，
    // 整个函数不会有任何产出，直接返回，避免无谓的状态拼装与攻防计算。
    if (!broadcast && !has_client()) return;

    char tmp[128];
    std::string msg;
    msg += snprintf(tmp, static_cast<int>(sizeof(tmp)), "&C=%d&T=%ld&", STATE,
                    object_id());
    get_state_msg(msg);

    // 广播：把状态标志精简包发给房间内其他玩家（不含 hp/攻防）。
    if (broadcast) message_vision_room(this, "command", msg.c_str(), this);

    // 没有客户端就不必再拼装私发的完整状态包（含攻防计算），到此为止。
    if (!has_client()) return;

    msg += snprintf(tmp, static_cast<int>(sizeof(tmp)),
                    "hp=%ld&mp=%ld&eh=%ld&mm=%ld&", query("hp"), query("mp"),
                    query("eff_hp"), query("max_mp"));

    // 计算攻防
    const char* skill_type = "unarmed";
    CContainer* weapon;
    unsigned long ap, dp, pp;

    CVector v;
    CMapping* prepare = query_skill_prepare();
    prepare->keys(v);

    // 1, 选用哪种动作：依赖武器的技能。
    if ((weapon = query_weapon()))
        skill_type = weapon->querystr("skill_type");
    else if (v.count() == 0)
        skill_type = "unarmed";
    else
        skill_type = v[0];

    ap = static_cast<unsigned long>(
        CCombatCalc::skill_power(this, skill_type, SKILL_USAGE_ATTACK));
    pp = static_cast<unsigned long>(
        CCombatCalc::skill_power(this, skill_type, SKILL_USAGE_DEFENSE));
    dp = static_cast<unsigned long>(
        CCombatCalc::skill_power(this, "dodge", SKILL_USAGE_DEFENSE));

    msg += snprintf(
        tmp, static_cast<int>(sizeof(tmp)), "att=%ld&def=%ld&",
        static_cast<long>(ap / 100 + 1),
        static_cast<long>((dp + (weapon ? pp : (pp / 10))) / 100 + 1));

    // 加力
    if (query("mp_factor")) {
        msg += snprintf(tmp, static_cast<int>(sizeof(tmp)), "jl=%ld&",
                        query("mp_factor"));
    }

    send_command(msg.c_str(), SEND_WAIT_IMMEDIATE);
}

void CChar::get_state_msg(std::string& msg)
{
    char tmp[64];

    // 将战斗信息放在这里
    if (is_fighting()) {
        if (query_temp("fight/fight_team")) msg += "tea=1&";
        if (query_temp("fight/ap")) {
            msg += snprintf(tmp, static_cast<int>(sizeof(tmp)), "ap=%d&",
                            query_temp("fight/ap"));
        }
    }
    else
        msg += "tea=-1&";

    int max_hp = query("max_hp");
    if (max_hp) {
        msg += snprintf(tmp, static_cast<int>(sizeof(tmp)), "sta=%ld&",
                        query("hp") * 100 / max_hp);
    }

    const char* disable = querystr_temp("disabled");
    if (disable[0])
        msg += snprintf(tmp, static_cast<int>(sizeof(tmp)), "dis=%s&", disable);

    if (query_temp("pending/exercising"))  // 在打坐
        msg += "exe=1&";

    if (query_temp("net_dead"))  // 掉线了
        msg += "net=1&";

    // 检查是否中毒
    CMapping* conditions = querymap("condition");

    if (conditions) {
        CVector v;

        conditions->keys(v);
        for (int i = 0; i < v.count(); i++) {
            if (strstr(v[i], "poison")) {
                msg += "poi=1&";
                break;
            }
        }
    }

    // 检查是否发呆，战斗中不发呆。
    if (query_attr(IS_USER) && static_cast<CUser*>(this)->query_idle() > 180
        && !is_fighting())
        msg += "idl=1&";
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:unconcious()
int l_unconcious(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->unconcious();
    return 0;
}

// self:die()
int l_die(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->die();
    return 0;
}

// self:die_base() - 非虚调用基类 CChar::die()（避免 Lua die 递归）
int l_die_base(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->CChar::die();
    return 0;
}

// self:SendState()        - 广播状态给房间内其他玩家并私发完整状态
// self:SendState(other)   - 仅刷新自身，不广播（尊重 Lua 第二参数）
int l_send_state(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    bool broadcast = (lua_gettop(L) < 2);
    ch->send_state(broadcast);
    return 0;
}

// self:revive(quiet) - 复活该角色
int l_revive(lua_State* L)
{
    CChar* ch = dynamic_cast<CChar*>(get_object(L, 1));
    if (!ch) return 0;
    int quiet = (int)luaL_optinteger(L, 2, 0);
    ch->revive(quiet);
    return 0;
}
