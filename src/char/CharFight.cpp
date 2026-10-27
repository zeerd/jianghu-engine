/**
 * @file CharFight.cpp
 * @brief implementation of CChar (CharFight). */

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
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"

// 返回武器
CContainer* CChar::query_weapon()
{
    int32_t weapon = query_equip(WIELD_RIGHT_HAND);
    if (!weapon) weapon = query_equip(WIELD_BOTH_HAND);
    if (!weapon) return 0;

    CContainer* ob = present(weapon);
    if (!ob || !ob->querystr("skill_type")[0]) return 0;

    return ob;
}

CContainer* CChar::query_left_weapon()
{
    int32_t weapon = query_equip(WIELD_LEFT_HAND);
    if (!weapon) return 0;

    CContainer* ob = present(weapon);
    if (!ob || !ob->querystr("skill_type")[0]) return 0;

    return ob;
}

void CChar::init(CChar* me)
{
    const char* vendetta_mark;

    if (is_fighting() || !query_attr(IS_LIVING) || !me
        || !me->query_attr(IS_LIVING) || me->query_temp("net_dead"))
        return;

    // Now start check the auto fight cases.
    vendetta_mark = querystr("vendetta_mark");
    char mark[80];

    snprintf(mark, 80, "vendetta/%s", vendetta_mark);

    if (is_user(me) && is_killing(me)) {
        g_auto_fight.auto_fight(this, me, "hatred");
        return;
    }
    else if (vendetta_mark[0] && me->query(mark)) {
        g_auto_fight.auto_fight(this, me, "vendetta");
        return;
    }
    else if (is_user(me) && EQUALSTR(querystr("attitude"), "aggressive")) {
        g_auto_fight.auto_fight(this, me, "aggressive");
        return;
    }
}

// 野兽攻击动作表：verb -> 战斗文案与伤害系数。
// query_action 出招与 set_beast_verbs 的合法动作集合共用此表。
static const struct BeastCombatAction {
    char verb[10];
    char action[80];
    int damage;
} beast_combat_action[5] = {
    {"hoof", "$N用后腿往$n的$l用力一蹬", 100},
    {"bite", "$N扑上来张嘴往$n的$l狠狠地一咬", 20},
    {"claw", "$N用爪子往$n的$l一抓", 0},
    {"poke", "$N用嘴往$n的$l一啄", 30},
    {"knock", "$N用头往$n的$l用力一撞", 100},
};

static bool is_beast_verb(const char* verb)
{
    for (const auto& action : beast_combat_action)
        if (strcmp(action.verb, verb) == 0) return true;
    return false;
}

CMapping* CChar::query_action()
{
    static CMapping m_action;

    static char human_combat_action[5][80] = {
        {
            "$N挥拳攻击$n的$l",
        },
        {
            "$N往$n的$l一抓",
        },
        {
            "$N往$n的$l狠狠地踢了一脚",
        },
        {
            "$N提起拳头往$n的$l捶去",
        },
        {
            "$N对准$n的$l用力挥出一拳",
        },
    };

    if (EQUALSTR(querystr("race"), "人类")) {
        m_action.set("action", human_combat_action[g_utils.random(5)]);
        m_action.set("damage", 0l);
        return &m_action;
    }

    string verb;
    CVector* v;

    if (EQUALSTR(querystr("race"), "野兽")) {
        if (!(v = queryvec("verbs"))) {
            m_action.set("action", "$N攻击$n的$l。");
            m_action.set("damage", 0l);
            return &m_action;
        }

        verb = (*v)[g_utils.random(v->count())];
        for (int i = 0; i < 5; i++)
            if (verb == beast_combat_action[i].verb) {
                m_action.set("action", beast_combat_action[i].action);
                m_action.set("damage", beast_combat_action[i].damage);
                return &m_action;
            }
    };

    m_action.set("action", "$N攻击$n的$l。");
    m_action.set("damage", 0l);
    // 缺省action
    return &m_action;
}

// 比试结束时呼叫。
void CChar::is_defeated(CChar* winner, CChar* failer)
{
    static char winner_msg[11][80] = {
        "\n$N哈哈大笑，拱手说道：承让了！\n",
        "\n$N双手一拱，笑著说道：知道我的利害了吧！\n",
        "\n$N胜了这招，笑道：胜败乃兵家常事，不必在意。\n",
        "\n$n向后退了几步，说道：咱们来日方长！\n",
        "\n$n向后一纵，恨恨地说道：君子报仇，十年不晚！\n",
        "\n$N哈哈大笑，说道：承让了！\n",
        "\n$N双手一拱，笑著说道：承让！\n",
        "\n$N胜了这招，向后跃开三尺，笑道：承让！\n",
        "\n$n脸色微变，说道：佩服，佩服！\n",
        "\n$n向后退了几步，说道：这场比试算我输了，佩服，佩服！\n",
        "\n$n向后一纵，躬身做揖说道：阁下武艺不凡，果然高明！\n",
    };

    if (winner == this) {
        message_vision(winner_msg[g_utils.random(11)], winner, failer);
    }
}

// 返回盾牌 Designed For Lingering Tale
CContainer* CChar::query_shield()
{
    int32_t weapon = query_equip(WIELD_LEFT_HAND);  // 盾牌在左手
    if (!weapon) return 0;

    CContainer* ob = present(weapon);
    if (!ob || DIFFERSTR(ob->querystr("armor_type"), "shield")) return 0;

    return ob;
}

int CChar::query_unarmed()
{
    int improve = query_skill("unarmed", 1);

    for (const char* skill : {"strike", "finger", "cuff", "hand", "claw"}) {
        int level = query_skill(skill, 1);
        if (level >= improve) improve = level;
    }

    return improve;
}

int CChar::cmd_hit_ob(CChar* who, char* arg)
{
    CContainer* msprite = NULL;
    CContainer* env;
    CFightRoom* room;
    char namestr[21], msg[255];
    int protect;

    env = environment();
    if (!env || env->query("no_fight")) return notify_fail("此地禁止战斗!");

    if (env != who->environment()) return 0;  // 不在同一场景中

    if (is_user(this) && is_user(who)
        && (query("level") < 75 || who->query("level") < 75))
        return notify_fail("才入江湖，何必动刀动枪。");

    if (who->query_attr(CChar::IS_CORPSE)) return 0;
    if (env->query_attr(CChar::IS_FIGHTROOM)) {
        room = static_cast<CFightRoom*>(env);

        // 已经在战斗场景了。我没战斗，且对手在战斗
        if (is_fighting())  // 我在战斗
        {
            if (who->is_fighting()) return 1;

            // 对手没在战斗,对手加入战队
            int team = 1 - query_team();
            if (!room->can_join(team, who)) return 1;
            room->join_team(who, team);
            tell_room(room,
                      snprintf(msg, 255, "%s加入战队%d.", who->name(), team));
        }
        else  // 我没战斗,对方也没战斗
        {
            if (!who->is_fighting()) return 1;

            int team = 1 - who->query_team();
            if (!room->can_join(team, this)) return 1;
            room->join_team(this, team);
            tell_room(room, snprintf(msg, 255, "%s加入战队%d.", name(), team));
        }

        return 1;
    }

    if (who == this) return 0;  // 不能杀自己。

    if (who->query_temp("net_dead"))
        return notify_fail("不能向断线玩家发动进攻。");

    if (is_busy()) return notify_fail("你现在正忙着。");

    protect = env->query("pk_protect");
    if (protect && who->query_attr(CChar::IS_USER) && query("level") > protect
        && who->query("level") <= protect)

        return notify_fail("等级%d以下玩家在此地受保护。", protect);

    snprintf(msg, 255, "%s对%s突然发动袭击!", name(), who->name());
    message_vision_room(this, "chat", msg);

    // who中止busy
    if (who->is_busy()) who->interrupt_me(this, "kill");

    room = (CFightRoom*)g_item_mgr->load_item("fight_room");  // 载入战斗精灵
    strncpy(namestr, name(), 15)[14] = 0;                     // 取玩家姓名
    strcat(namestr, "的战斗");
    room->set_name(namestr);

    //----------------------------------------
    // 坐标信息
    int32_t x = 0, y = 0;
    if (strlen(arg)) {
        char szY[40];
        analyse_string(arg, szY, 40);
        x = atol(arg);
        y = atol(szY);
    }

    if (!x || !y) {
        x = query_temp("map/ex");
        y = query_temp("map/ey");
    }

    set_live_point(room, x, y);
    //----------------------------------------

    room->move(env);

    move(room);
    room->join_team(this, CFightRoom::FIGHT_TEAM_A);  // 加入战队

    who->move(room);
    room->join_team(who, CFightRoom::FIGHT_TEAM_B);

    // 记录NK经验onwer
    if (!(is_user(who))) {
        set_temp("fight/will_nk", who->object_id());
        who->set_temp("fight/owner", object_id());
    }

    // 环境内其他本帮派的加入战斗。
    if (!is_user(who)) {
        DTItemList search;
        const char *bangpai, *mbang, *ebang;

        copy_list(&search, env->get_item_list());

        mbang = querystr("family/family_name");
        ebang = who->querystr("family/family_name");

        for (auto val : search) {
            msprite = val;
            if (!msprite->query_attr(CChar::IS_CHAR)) continue;
            bangpai = msprite->querystr("family/family_name");
            if (bangpai[0]) {
                if (EQUALSTR(bangpai, ebang)) {
                    snprintf(msg, 255, "join %ld 1", room->object_id());
                    if (!is_user(msprite))  // NPC帮忙
                        static_cast<CChar*>(msprite)->command(msg);
                }
            }

            if (static_cast<CChar*>(msprite)->is_killing(this))
                static_cast<CChar*>(msprite)->command(
                    snprintf(msg, 255, "join %d 1", room->object_id()));

            if (static_cast<CChar*>(msprite)->is_killing(who))
                static_cast<CChar*>(msprite)->command(
                    snprintf(msg, 255, "join %d 0", room->object_id()));
        }

        search.clear();
    }

    room->begin_fight(0);  // 开始战斗

    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:query_weapon() -> object
int l_query_weapon(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_bind_push_object(L, ch->query_weapon());
    return 1;
}

// self:is_fighting(target) -> boolean (带可选目标)
int l_is_fighting_target(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CContainer* target = NULL;
    if (lua_isuserdata(L, 2))
        target = dynamic_cast<CContainer*>(get_object(L, 2));
    lua_pushboolean(L, ch->is_fighting(target));
    return 1;
}

// self:query_left_weapon() -> object | nil - 查询左手武器
int l_query_left_weapon(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    CContainer* w = ch->query_left_weapon();
    if (w)
        lua_bind_push_object(L, w);
    else
        lua_pushnil(L);
    return 1;
}

// self:init(me) - 触发对象的 C++ init（对 Lua NPC 会分派到其脚本 init 函数）
// 用于复刻 cell.h 中 npc->init(me) 的调用
int l_init(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    c->init(me);
    return 0;
}

// Do_BiShi(me, victim, name, check) -> room
int l_do_bishi(lua_State* L)
{
    CChar* me        = dynamic_cast<CChar*>(get_object(L, 1));
    CChar* victim    = dynamic_cast<CChar*>(get_object(L, 2));
    const char* name = luaL_optstring(L, 3, "比武");
    int check        = (int)luaL_optinteger(L, 4, 50);
    char nameBuf[128];
    strncpy(nameBuf, name, sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = 0;
    if (!me || !victim) {
        lua_pushnil(L);
        return 1;
    }
    CContainer* env = do_bishi(me, victim, nameBuf, check);
    lua_bind_push_object(L, env);
    return 1;
}

// self:set_beast_verbs({"bite","claw"}) - 设置野兽可用的攻击动作白名单
// （dbase "verbs"，战斗时 query_action 随机选用其一）。动物没有技能体系，
// 不走 set_skill；动作必须在 beast_combat_action 表内，否则抛 Lua 错误，
// 防止拼写错误静默退化成无伤害的默认攻击。
int l_set_beast_verbs(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    luaL_checktype(L, 2, LUA_TTABLE);
    int n = (int)lua_rawlen(L, 2);
    CVector v;
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, 2, i);
        const char* verb = lua_tostring(L, -1);
        if (!verb || !is_beast_verb(verb)) {
            lua_pop(L, 1);
            return luaL_error(L, "set_beast_verbs: bad verb #%d", i);
        }
        v.append(verb);
        lua_pop(L, 1);
    }
    obj->set("verbs", v);
    return 0;
}
