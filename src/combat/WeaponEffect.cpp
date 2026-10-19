/**
 * @file WeaponEffect.cpp
 * @brief Weapon interaction effects during combat */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "WeaponEffect.h"
#include "CombatTypes.h"
#include "Item.h"
#include "Channel.h"
#include "Message.h"

void CWeaponEffect::bash_weapon(CChar* me, CChar* victim, CContainer* weapon,
                                int32_t damage)
{
    int32_t wap, wdp;
    char msg[255];
    CContainer* ob = victim->query_weapon();

    if (weapon && damage == RESULT_PARRY && ob && !ob->query("no_duo")) {
        wap = weapon->weight() / 500 + weapon->query("rigidity")
              + me->query("str");
        wdp = ob->weight() / 500 + ob->query("rigidity") + victim->query("str");
        wap = g_utils.random(wap);
        if (wap > 2 * wdp) {
            message_vision(
                snprintf(msg, 255, "$N只觉得手中%s把持不定，脱手飞出！",
                         ob->name()),
                victim);
            ob->move(victim->environment());
        }
        else if (wap > wdp) {
            message_vision(snprintf(msg, 255, "$N只觉得手中%s一震，险些脱手！",
                                    ob->name()),
                           victim);
        }
        else if (wap > wdp / 2) {
            message_vision(
                snprintf(msg, 255,
                         "只听见「啪」地一声，$N手中的%s已经断为两截！",
                         ob->name()),
                victim);
            ob->move(victim->environment());
            ob->set_name(snprintf(msg, 255, "断掉的%s", ob->name(1)));
            ob->set("value", 0l);
            ob->del("apply");
        }
    }
}

void CWeaponEffect::throw_weapon(CChar* me, CChar* victim, CContainer* weapon,
                                 int32_t damage)
{
    char msg[255];

    if (weapon) {
        // 判断是否自制兵器！！
        if (weapon->querystr("owner")[0] || weapon->query("no_duo")) {
        }
        else {
            if (weapon->query_amount() == 1) {
                tell_object(
                    me, snprintf(msg, 255, "你的%s用完了！", weapon->name(1)));
            }

            static_cast<CItem*>(weapon)->add_amount(-1);
        }
    }
}

// 取角色身上第一件暗器（skill_type == "throwing"）
CContainer* get_first_throwing_weapon(CChar* me, const char* name)
{
    if (!me) return 0;

    CContainer* weapon = NULL;
    if ((weapon = me->query_weapon())
        && EQUALSTR(weapon->querystr("skill_type"), "throwing"))
        return weapon;

    weapon           = NULL;
    CContainer* obj  = NULL;
    DTItemList* list = me->get_item_list();
    for (auto val : *list) {
        obj = val;
        if ((obj != NULL)
            && EQUALSTR(obj->querystr("skill_type"), "throwing")) {
            if (weapon == NULL) {
                weapon = obj;
            }
            if (name == NULL) {
                break;
            }
            else {
                if (EQUALSTR(obj->querystr("id"), name)) {
                    weapon = obj;
                    break;
                }
            }
        }
    }

    return weapon;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// get_first_throwing_weapon(me[, name]) -> object
// 返回身上第一件暗器（可指定名字），无则返回 nil
int l_get_first_throwing_weapon(lua_State* L)
{
    CChar* me = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CChar*>(get_object(L, 1));
    if (!me) return 0;
    const char* name = lua_isstring(L, 2) ? luaL_checkstring(L, 2) : NULL;
    CContainer* w    = get_first_throwing_weapon(me, name ? name : NULL);
    lua_bind_push_object(L, w);
    return 1;
}

// throw_weapon(me, victim, weapon[, damage])
// （CCombatd::throw_weapon：消耗暗器） CCombatd::throw_weapon
// 仅处理暗器数量消耗与"用尽"提示，不结算伤害
int l_throw_weapon(lua_State* L)
{
    CChar* me          = NULL;
    CChar* victim      = NULL;
    CContainer* weapon = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CChar*>(get_object(L, 1));
    if (lua_isuserdata(L, 2)) victim = dynamic_cast<CChar*>(get_object(L, 2));
    if (lua_isuserdata(L, 3))
        weapon = dynamic_cast<CContainer*>(get_object(L, 3));
    int32_t damage = (int32_t)luaL_optinteger(L, 4, 0);
    if (me && weapon) CWeaponEffect::throw_weapon(me, victim, weapon, damage);
    return 0;
}
