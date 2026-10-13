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
