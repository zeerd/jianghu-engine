
/**
 * @file Attack.h
 * @brief 攻击结算管线，将一次攻击拆分为多个阶段处理。
 */

#pragma once

#include <string>
#include "Container.h"
#include "CombatTypes.h"

class CChar;
class CSkill;
class CMapping;

// ====================================================================
// AttackContext - intermediate state shared across attack phases
// ====================================================================
/**
 * @brief 攻击上下文，在攻击管线各阶段间传递中间状态。
 */
struct AttackContext {
    // Input parameters
    CChar* me;
    CChar* victim;
    CContainer* weapon;
    const char* base_skill;
    int flag;

    // Skill resolution results
    CContainer* left_weapon;
    std::string attack_type;
    std::string attack_skill;
    std::string dodge_skill;
    std::string parry_skill;
    std::string force_skill;
    CSkill* skill;
    CMapping* action;
    CMapping* prepare;

    // Action ratios extracted from action mapping
    int damage_ratio;
    int parry_ratio;
    int dodge_ratio;
    int force_ratio;

    // Combat values
    double ap;
    double dp;
    double pp;
    int32_t mod_val;
    int32_t damage;
    int32_t damage_bonus;
    int32_t damage_base;  // apply/damage 快照（伤害结算前）

    // Message & experience
    std::string result;
    const char* dodge_msg;
    const char* parry_msg;
    int add_exp_me;
    int add_exp_victim;

    AttackContext()
        : me(nullptr),
          victim(nullptr),
          weapon(nullptr),
          base_skill(nullptr),
          flag(0),
          left_weapon(nullptr),
          attack_type("unarmed"),
          attack_skill("unarmed"),
          dodge_skill("dodge"),
          parry_skill("parry"),
          force_skill("force"),
          skill(nullptr),
          action(nullptr),
          prepare(nullptr),
          damage_ratio(0),
          parry_ratio(0),
          dodge_ratio(0),
          force_ratio(0),
          ap(0),
          dp(0),
          pp(0),
          mod_val(0),
          damage(0),
          damage_bonus(0),
          damage_base(0),
          result("\n"),
          dodge_msg(nullptr),
          parry_msg(nullptr),
          add_exp_me(0),
          add_exp_victim(0)
    {
    }
};

// ====================================================================
// CAttack - attack resolution pipeline
// ====================================================================
class CAttack {
public:
    // Execute the full attack pipeline. Returns false if skill was invalid
    // and the function exited early.
    bool execute(AttackContext& ctx);

    // Format damage value into colored description string
    static string damge_to_string(int32_t damage, bool real = true);

private:
    friend struct AttackTestPeer;

    // Phase 1: Skill resolution
    void select_skill(AttackContext& ctx);

    // Phase 2: Action lookup & ratio extraction
    // Returns false if skill has no valid action (early exit)
    bool retrieve_action(AttackContext& ctx);

    // Phase 3: Force skill compatibility check
    void validate_force(AttackContext& ctx);

    // Phase 4: Action ratio rules from scripts (hooks.attack_ratios)
    void apply_ratio_rules(AttackContext& ctx);

    // Phase 5: Build action description string
    void build_action_description(AttackContext& ctx);

    // Phase 6: AP/DP calculation & dodge resolution
    void resolve_dodge(AttackContext& ctx);

    // Phase 7: PP calculation & parry resolution
    void resolve_parry(AttackContext& ctx);

    // Phase 8: Damage calculation with special effects
    void calculate_damage(AttackContext& ctx);

    // Phase 9: Replace $N/$n/$w/$W/$d/$P/$p/$l placeholders
    void replace_placeholders(AttackContext& ctx);

    // Phase 10: Dispatch combat message to viewers
    void dispatch_message(AttackContext& ctx);

    // Phase 11: Weapon effects (bash/throw)
    void apply_weapon_effects(AttackContext& ctx);

    // Phase 12: Wizard verbose debug output
    void send_verbose_debug(AttackContext& ctx);
};
