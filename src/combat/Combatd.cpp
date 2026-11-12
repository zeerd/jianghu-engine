/**
 * @file Combatd.cpp
 * @brief Combat coordinator (attack dispatch + fight loop)
 */
#include <windows.h>

#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "WeaponEffect.h"
#include "CmdHandle.h"
#include "Skill.h"
#include "Channel.h"
#include "FightRoom.h"
#include "Money.h"
#include "Room.h"

#include "LuaBindEnums.h"
#include "LuaHook.h"
#include "../config/MainConfig.h"

// 战斗之进攻
void CCombatd::do_attack(CChar* me, CChar* victim, int flag)
{
    do_attack_special(me, victim, NULL, NULL, flag);
}

void CCombatd::do_attack_weapon(CChar* me, CChar* victim, CContainer* weap,
                                int flag)
{
    do_attack_special(me, victim, weap, NULL, flag);
}

void CCombatd::do_attack_unarmed(CChar* me, CChar* victim, const char* skill,
                                 int flag)
{
    do_attack_special(me, victim, NULL, skill, flag);
}

// 战斗之进攻 (delegates to CAttack)
void CCombatd::do_attack_special(CChar* me, CChar* victim, CContainer* weap,
                                 const char* base_skill, int flag)
{
    CAttack attack;
    AttackContext ctx;
    ctx.me         = me;
    ctx.victim     = victim;
    ctx.weapon     = weap;
    ctx.base_skill = base_skill;
    ctx.flag       = flag;
    attack.execute(ctx);
}

void CCombatd::fight(CChar* me, CChar* victim)
{
    int prepare_count = (me->query_skill_prepare())->get_count();

    if (!me->query_attr(CChar::IS_LIVING)) return;

    // 进攻！
    do_attack(me, victim, TYPE_REGULAR);

    // 这句话放在Do_Attack下面，防止武器已经被打掉或用光出错。
    CContainer* weapon = me->query_weapon();
    // 是否能连击
    if (!weapon && prepare_count > 1) {
        me->set_temp("action_flag", 1);
        do_attack(me, victim, TYPE_QUICK);
        me->set_temp("action_flag", MAPPING_VALUE_ZERO);
    }

    // 追加攻击钩子（hooks.fight）
    CLuaHookCall hook("fight");
    hook.add_object(me).add_object(victim);
    hook.call();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// combat_attack(kind, me, target, ...) - 按 KIND 派发一次攻击：
//   normal(me, target[, flag])
//   unarmed(me, target, skill[, flag])
//   weapon(me, target, weapon[, flag])
// flag 默认 TYPE_QUICK（0），脚本禁止传数字枚举，用全局常量
// TYPE_QUICK/TYPE_HIT 等；未登记 KIND 报错
int l_combat_attack(lua_State* L)
{
    const char* kind = luaL_checkstring(L, 1);
    CChar* me        = NULL;
    CChar* target    = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (lua_isuserdata(L, 3)) target = dynamic_cast<CChar*>(get_object(L, 3));
    if (strcmp(kind, "normal") == 0) {
        int flag = (int)luaL_optinteger(L, 4, CCombatd::TYPE_QUICK);
        if (me && target) g_combatd.do_attack(me, target, flag);
    }
    else if (strcmp(kind, "unarmed") == 0) {
        const char* skill = luaL_checkstring(L, 4);
        int flag          = (int)luaL_optinteger(L, 5, CCombatd::TYPE_QUICK);
        if (me && target) g_combatd.do_attack_unarmed(me, target, skill, flag);
    }
    else if (strcmp(kind, "weapon") == 0) {
        CContainer* weapon = NULL;
        if (lua_isuserdata(L, 4))
            weapon = dynamic_cast<CContainer*>(get_object(L, 4));
        int flag = (int)luaL_optinteger(L, 5, CCombatd::TYPE_QUICK);
        if (me && target && weapon)
            g_combatd.do_attack_weapon(me, target, weapon, flag);
    }
    else {
        return luaL_error(L, "unknown combat_attack kind: %s", kind);
    }
    return 0;
}

// combatd_query(key) -> number - 查询全局战斗/系统参数（对应 g_Combatd.query）
int l_combatd_query(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    lua_pushinteger(L, g_combat_config.query(key));
    return 1;
}

// Combatd.h 攻击类型枚举 → Lua 全局常量
static const LuaEnumEntry kCombatTypeEnums[] = {
    {"TYPE_QUICK", CCombatd::TYPE_QUICK},
    {"TYPE_REGULAR", CCombatd::TYPE_REGULAR},
    {"TYPE_RIPOSTE", CCombatd::TYPE_RIPOSTE},
    {"TYPE_NORMAL", CCombatd::TYPE_NORMAL},
    {"TYPE_REMOTE", CCombatd::TYPE_REMOTE},
    {"TYPE_MAGIC", CCombatd::TYPE_MAGIC},
    {"TYPE_HIT", CCombatd::TYPE_HIT},
    // 攻击结算哨兵值（脚本侧表达闪避/招架结果）
    {"RESULT_DODGE", RESULT_DODGE},
    {"RESULT_PARRY", RESULT_PARRY},
    {nullptr, 0},
};

void lua_bind_reg_combat_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kCombatTypeEnums);
}
