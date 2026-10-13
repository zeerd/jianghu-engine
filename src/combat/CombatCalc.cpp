/**
 * @file CombatCalc.cpp
 * @brief Combat power calculations */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "CombatCalc.h"
#include "Skill.h"
#include "Channel.h"
#include "Message.h"

double CCombatCalc::skill_power(CChar* ob, const char* skill, int usage)
{
    int level;
    double p, power;

    if (!ob->query_attr(CChar::IS_LIVING)) return 0;

    level = ob->query_skill(skill);
    switch (usage) {
        case SKILL_USAGE_ATTACK:
            level += ob->query_temp("apply/attack");
            break;
        case SKILL_USAGE_DEFENSE:
            level += ob->query_temp("apply/defense");
            break;
    }

    int32_t combat_exp = ob->query("combat_exp");

    if (userp(ob) && combat_exp > 10000)
        combat_exp = 6000 + combat_exp / 2;  // 玩家的经验值只有1/2是有效的。

    if (!level) return combat_exp / 2;

    power = (double)level * (double)level * (double)level / 3.0;

    int32_t abi;
    if (usage == SKILL_USAGE_ATTACK) {
        abi = 10 + (ob->query("str") - 11) + (ob->query("dex") - 11)
              + ob->query_temp("apply/str") + ob->query_unarmed() / 10;
    }
    else
        abi = ob->query_dex();
    p = (power + combat_exp) / 30 * abi;

    if (wizardp(ob) && EQUALSTR(ob->querystr("env/combat"), "verbose")) {
        char msg[255];
        _snprintf(msg, 255,
                  "p:%.2f, skill：%s, level=%d, power=%.2f, exp=%ld, abi=%ld",
                  p, skill, level, power, combat_exp, abi);
        message_vision_room(ob, "tell", msg);
    }

    return p;
}
