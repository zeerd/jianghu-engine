/**
 * @file Force.cpp
 * @brief implementation of the CForce class. */

#include <windows.h>

#include "Force.h"
#include "ServerConfig.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CForce::CForce() {}

CForce::~CForce() {}

void CForce::create()
{
    set_name("基本内功");

    Magic_t* magic;

    magic             = add_magic("recover", do_recover);
    magic->name       = "复元术";
    magic->mp         = 20;
    magic->target     = CAST_TO_ME;
    magic->valid_cast = CAST_IN_BOTH;
    magic->hostile    = 0;  // 自身调息复元，不构成敌意行为
}

char* CForce::hit_ob(CChar* me, CChar* victim, int32_t& damage_bonus)
{
    int32_t damage, factor = me->query("mp_factor");
    int desc               = 0;
    static char chat[][80] = {
        "$HIY$N受到$n的内力反震，闷哼一声。",
        "$HIR$N被$n以内力反震，「嘿」地一声退了两步。",
        "$HIR$N被$n以内力一震，胸口有如受到一记重击，连退了五六步！",
        "$RED$N被$n的内力一震，眼前一黑，身子向后飞出丈许！！",
    };

    if (userp(me)) me->add("mp", -factor);

    // 计算上限
    int mp     = me->query("mp");
    int mp_you = victim->query("mp");
    if (mp < 0) mp = 0;
    if (mp_you < 0) mp_you = 0;

    if (mp_you > mp * 3) {
        // 内力超级多的NPC太多了，搞得加力功能都废了。必须要限制一下。
        mp_you = mp * 3;
    }

    damage = mp / 30 + factor - mp_you / 100;

    if (damage < 0) {
        if (!me->query_weapon()
            && g_utils.random(victim->query_skill("force") + 1)
                   > me->query_skill("force") / 2) {
            damage = -damage;
            me->receive_damage("hp", damage * 2, victim);
            me->receive_wound("hp", damage, victim);

            if (damage < 10)
                desc = 0;
            else if (damage < 20)
                desc = 1;
            else if (damage < 40)
                desc = 2;
            else
                desc = 3;
            return chat[desc];
        }

        damage_bonus += damage;
        if (damage_bonus < 0) damage_bonus = 0;
        return 0;
    }

    damage -= victim->query_temp("apply/armor_vs_force");

    if (damage_bonus + damage < 0)
        damage_bonus = 0;
    else if (g_utils.random(me->query_skill("force")) < damage)
        damage_bonus += damage;

    return 0;
}

int CForce::do_recover(CChar* me, CContainer* target)
{
    int n, q;
    if (me->query("mp") < 20) return notify_fail("你的内力不够。");

    q = me->query("eff_hp") - me->query("hp");
    if (q < 10) return notify_fail("你现在气力充沛。");
    if (!me->query_skill("force")) return notify_fail("你不会内功。");

    n = 100 * q / me->query_skill("force");
    if (n < 20) n = 20;
    if (me->query("mp") < n) {
        q = q * me->query("mp") / n;
        n = me->query("mp");
    }

    me->add("mp", -n);
    me->receive_healing("hp", q);

    message_vision("$N深深吸了几口气，脸色看起来好多了。", me);

    if (me->is_fighting()) me->start_busy(1);

    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// config.ini [server] no_force_conflict 开关（开启返回 1，否则返回 0）
int l_get_no_force_conflict(lua_State* L)
{
    lua_pushinteger(L, CServerConfig::no_force_conflict() ? 1 : 0);
    return 1;
}
