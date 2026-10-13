/**
 * @file Combatd.h
 * @brief 战斗协调器，负责攻击分发与战斗循环。
 */
//---------------------------------------------------------

#pragma once

#include "CombatTypes.h"
#include "Attack.h"

class CChar;

/**
 * @brief 战斗协调器，管理攻击分发与每回合战斗循环。
 */
class CCombatd {
public:
    // Attack dispatch (delegates to CAttack)
    void do_attack(CChar* me, CChar* victim, int flag);
    void do_attack_weapon(CChar* me, CChar* victim, CContainer* weapon,
                          int flag);
    void do_attack_unarmed(CChar* me, CChar* victim, const char* skill,
                           int flag);
    void do_attack_special(CChar* me, CChar* victim, CContainer* weap,
                           const char* skill, int flag);

    // Main fight loop (one combat turn)
    void fight(CChar* me, CChar* victim);

    // Backward-compatible aliases for TYPE_* enum (use CombatType::* directly)
    enum {
        TYPE_QUICK   = COMBAT_TYPE_QUICK,
        TYPE_REGULAR = COMBAT_TYPE_REGULAR,
        TYPE_RIPOSTE = COMBAT_TYPE_RIPOSTE,
        TYPE_NORMAL  = COMBAT_TYPE_NORMAL,
        TYPE_REMOTE  = COMBAT_TYPE_REMOTE,
        TYPE_MAGIC   = COMBAT_TYPE_MAGIC,
        TYPE_HIT     = COMBAT_TYPE_HIT,
    };

    CCombatd() {};
    virtual ~CCombatd() {};
};
