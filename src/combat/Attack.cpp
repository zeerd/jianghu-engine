/**
 * @file Attack.cpp
 * @brief CAttack implementation */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "Attack.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "SetEffect.h"
#include "WeaponEffect.h"
#include "Skill.h"
#include "Channel.h"
#include "CmdHandle.h"
#include "Room.h"
#include "../util/Logger.h"
#include "../interfaces/IHittable.h"
#include "LuaHook.h"

// ====================================================================
// Execute - orchestrate the full attack pipeline
// ====================================================================
bool CAttack::execute(AttackContext& ctx)
{
    // Phase 1: Skill resolution
    select_skill(ctx);

    // Phase 2: Action lookup & ratio extraction (may exit early)
    if (!retrieve_action(ctx)) return false;

    // Phase 3: Force skill compatibility check
    validate_force(ctx);

    // Phase 4: Action ratio rules from scripts
    apply_ratio_rules(ctx);

    // Phase 5: Build action description
    build_action_description(ctx);

    // Phase 6: Dodge resolution
    resolve_dodge(ctx);

    // Phase 7: Parry resolution (or damage calculation if not dodged)
    if (ctx.damage != RESULT_DODGE) {
        resolve_parry(ctx);
    }

    // Phase 9: Replace placeholders
    replace_placeholders(ctx);

    // Phase 10: Dispatch message
    dispatch_message(ctx);

    // Phase 11: Weapon effects
    apply_weapon_effects(ctx);

    // 攻击结算后置钩子（hooks.set_effect: post_attack）
    CSetEffect::notify(ctx, SetEffectPhase::PostAttack);

    // Phase 12: Verbose debug
    send_verbose_debug(ctx);

    return true;
}

// ====================================================================
// Phase 1: Select skill based on weapon / prepare / base_skill
// ====================================================================
void CAttack::select_skill(AttackContext& ctx)
{
    ctx.prepare = ctx.me->query_skill_prepare();
    CVector v;
    ctx.prepare->keys(v);

    ctx.left_weapon = ctx.me->query_left_weapon();

    // Resolve weapon
    if (ctx.weapon == nullptr) {
        ctx.weapon = ctx.me->query_weapon();
    }

    // Determine attack_type and attack_skill
    if (ctx.base_skill == nullptr) {
        if (ctx.weapon != nullptr)
            ctx.attack_type = ctx.weapon->querystr("skill_type");
        else if (v.count() == 0)
            ctx.attack_type = "unarmed";
        else if (v.count() == 1)
            ctx.attack_type = v[0];
        else if (v.count() == 2)
            ctx.attack_type = v[ctx.me->query_temp("action_flag")];

        const char* mapped = ctx.me->query_skill_mapped(ctx.attack_type);
        if (mapped && mapped[0])
            ctx.attack_skill = mapped;
        else
            ctx.attack_skill = ctx.attack_type;
    }
    else {
        const char* mapped = ctx.me->query_skill_mapped(ctx.base_skill);
        if (mapped && mapped[0])
            ctx.attack_skill = mapped;
        else
            ctx.attack_skill = ctx.base_skill;
    }

    ctx.skill = load_skill(ctx.attack_skill);
}

// ====================================================================
// Phase 2: Retrieve action mapping and extract ratios
// Returns false if skill has no valid action (caller should return)
// ====================================================================
bool CAttack::retrieve_action(AttackContext& ctx)
{
    if (ctx.attack_skill == "unarmed") {
        ctx.action = ctx.me->query_action();
    }
    else if (!(ctx.action = ctx.skill->query_action(ctx.me))) {
        // Skill has no actions - clear prepare and abort
        ctx.prepare->clear();
        return false;
    }

    ctx.damage_ratio = ctx.action->query("damage");
    ctx.parry_ratio  = ctx.action->query("parry");
    ctx.dodge_ratio  = ctx.action->query("dodge");
    ctx.force_ratio  = ctx.action->query("force");

    return true;
}

// ====================================================================
// Phase 3: Validate force skill compatibility
// ====================================================================
void CAttack::validate_force(AttackContext& ctx)
{
    if (!ctx.skill->valid_force(ctx.me->query_skill_mapped("force"))
        && !ctx.skill->valid_force_pro(ctx.me)) {
        ctx.damage_ratio /= 2;
        ctx.force_ratio = 0;
    }
}

// ====================================================================
// Phase 4: Action ratio rules - scripts may overwrite the four ratios
// ====================================================================
void CAttack::apply_ratio_rules(AttackContext& ctx)
{
    CLuaHookCall hook("attack_ratios");
    hook.add_object(ctx.me)
        .add_object(ctx.victim)
        .add_object(ctx.skill)
        .add_int(ctx.damage_ratio)
        .add_int(ctx.parry_ratio)
        .add_int(ctx.dodge_ratio)
        .add_int(ctx.force_ratio);
    if (!hook.call() || hook.result_is_nil()) return;

    int64_t value = 0;
    if (hook.table_field_int("damage", value)) ctx.damage_ratio = (int)value;
    if (hook.table_field_int("parry", value)) ctx.parry_ratio = (int)value;
    if (hook.table_field_int("dodge", value)) ctx.dodge_ratio = (int)value;
    if (hook.table_field_int("force", value)) ctx.force_ratio = (int)value;
}

// ====================================================================
// Phase 5: Build action description string
// ====================================================================
void CAttack::build_action_description(AttackContext& ctx)
{
    if (ctx.me->query_temp("action_flag")) ctx.result += "紧跟着";

    const char* tmp_action = ctx.me->querystr_temp("action_desc");
    if (EQUALSTR(tmp_action, "")) {
        ctx.result += ctx.action->querystr("action");
    }
    else {
        ctx.result += tmp_action;
    }
    ctx.result += "$COM\n";
}

// ====================================================================
// Phase 6: Calculate AP/DP and resolve dodge
// ====================================================================
void CAttack::resolve_dodge(AttackContext& ctx)
{
    // Calculate AP
    ctx.ap = CCombatCalc::skill_power(ctx.me, ctx.attack_type.c_str(),
                                      SKILL_USAGE_ATTACK);
    if (ctx.ap < 1) ctx.ap = 1;

    // Calculate DP
    ctx.dp = CCombatCalc::skill_power(ctx.victim, "dodge", SKILL_USAGE_DEFENSE);

    ctx.mod_val             = 0;
    std::string dodge_skill = ctx.dodge_skill;

    if (is_living(ctx.victim)) {
        dodge_skill     = ctx.victim->query_skill_mapped("dodge");
        ctx.dodge_skill = dodge_skill;
    }

    if (dodge_skill != "") {
        CSkill* dodge_s = load_skill(dodge_skill);
        ctx.dodge_msg   = dodge_s->query_dodge_msg(ctx.victim);

        if (!ctx.dodge_msg || !ctx.dodge_msg[0]) {
            CMapping* victim_action = dodge_s->query_action(ctx.victim);
            if (victim_action) {
                ctx.mod_val   = victim_action->query("dodge");
                ctx.dodge_msg = victim_action->querystr("action");
            }
        }
    }

    if (ctx.victim->is_busy()) ctx.dp /= 3;
    if (ctx.dodge_ratio) ctx.mod_val += ctx.dodge_ratio;

    if (ctx.dp > 1000000)
        ctx.dp = ctx.dp / 100 * (100 + ctx.mod_val);
    else
        ctx.dp = (100 + ctx.mod_val) * ctx.dp / 100;

    if (ctx.dp < 1) ctx.dp = 1;
    if (ctx.flag == CCombatd::TYPE_HIT) ctx.dp = 1;

    // Dodge check: urandom(ap+dp) < dp means dodge
    if (g_utils.urandom(ctx.ap + ctx.dp) < ctx.dp) {
        if (ctx.dodge_msg && ctx.dodge_msg[0])
            ctx.result += ctx.dodge_msg;
        else
            ctx.result += "结果被$n躲了过去。";

        // Give dodge experience
        if (ctx.dp < ctx.ap
            && (!ctx.me->query_attr(CChar::IS_USER)
                || !ctx.victim->query_attr(CChar::IS_USER))
            && g_utils.random(50) < ctx.victim->query_int()) {
            ctx.add_exp_victim
                = 1 + g_utils.random(ctx.me->learn_bonus() / 20 + 1);
            ctx.victim->add("combat_exp", ctx.add_exp_victim);
            ctx.victim->improve_skill("dodge", 1);
        }

        // NPC attacker gains exp when dodged
        if (ctx.ap <= ctx.dp && !is_user(ctx.me)) {
            ctx.add_exp_me = 1 + g_utils.random(ctx.me->query("level") / 5 + 1);
            if (g_utils.random(ctx.me->query_int()) > 15)
                ctx.me->add("combat_exp", ctx.add_exp_me);
            ctx.me->improve_skill(ctx.attack_type,
                                  g_utils.random(ctx.me->query("int")));
        }

        ctx.damage = RESULT_DODGE;
    }
}

// ====================================================================
// Phase 7: Calculate PP and resolve parry (only if not dodged)
// ====================================================================
void CAttack::resolve_parry(AttackContext& ctx)
{
    std::string parry_skill = ctx.parry_skill;

    // Calculate PP based on whether victim has weapon
    if (ctx.victim->query_weapon()) {
        ctx.pp = CCombatCalc::skill_power(ctx.victim, "parry",
                                          SKILL_USAGE_DEFENSE);
        if (!ctx.weapon) ctx.pp *= 2;
        parry_skill     = ctx.victim->query_skill_mapped("parry");
        ctx.parry_skill = parry_skill;
    }
    else {
        if (ctx.weapon)
            ctx.pp = 0;
        else
            ctx.pp = CCombatCalc::skill_power(ctx.victim, "unarmed",
                                              SKILL_USAGE_DEFENSE);
        parry_skill     = ctx.victim->query_skill_mapped("unarmed");
        ctx.parry_skill = parry_skill;
    }

    if (!parry_skill[0]) parry_skill = "parry";
    ctx.parry_skill = parry_skill;  // 套装还击取用（含空映射回落 "parry"）

    if (ctx.victim->is_busy()) ctx.pp /= 3;
    if (ctx.pp < 1) ctx.pp = 1;
    if (ctx.flag == CCombatd::TYPE_HIT) ctx.pp = 1;

    ctx.mod_val = ctx.parry_ratio;

    if (ctx.pp > 1000000)
        ctx.pp = ctx.pp / 100 * (100 + ctx.mod_val);
    else
        ctx.pp = (100 + ctx.mod_val) * ctx.pp / 100;

    if (ctx.pp < 1) ctx.pp = 1;

    // Parry check
    if (g_utils.urandom(ctx.ap + ctx.pp) < ctx.pp) {
        const char* ptr = load_skill(parry_skill)->query_parry_msg(ctx.weapon);
        if (ptr && ptr[0])
            ctx.result += ptr;
        else
            ctx.result += "结果被$n格开了。";

        // Give parry experience
        if (ctx.pp < ctx.ap
            && (!ctx.me->query_attr(CChar::IS_USER)
                || !ctx.victim->query_attr(CChar::IS_USER))
            && g_utils.random(50) < ctx.victim->query_int()) {
            ctx.add_exp_victim
                = 1 + g_utils.random(ctx.me->learn_bonus() / 20 + 1);
            ctx.victim->add("combat_exp", ctx.add_exp_victim);
            ctx.victim->improve_skill("parry", 1);
        }

        ctx.damage = RESULT_PARRY;

        CSetEffect::dispatch(ctx, SetEffectPhase::ParryLanded);
    }
    else if (!CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed)) {
        // Not dodged, not parried, not intercepted by set effects -> hit.
        calculate_damage(ctx);
    }

    // 招架后置增强钩子（hooks.resolve_parry）
    CLuaHookCall hook("resolve_parry");
    hook.add_object(ctx.me)
        .add_object(ctx.victim)
        .add_int(ctx.damage)
        .add_str(ctx.parry_skill.c_str());
    if (hook.call() && !hook.result_is_nil()) {
        int64_t damage = ctx.damage;
        if (hook.table_field_int("damage", damage)) ctx.damage = (int)damage;
        std::string text;
        if (hook.table_field_str("text", text)) ctx.result += text;
    }
}

// ====================================================================
// Phase 8: Calculate damage with special effects
// ====================================================================
void CAttack::calculate_damage(AttackContext& ctx)
{
    // Base damage from apply/damage + skill level
    int damage_temp = ctx.me->query_temp("apply/damage");
    if (damage_temp < 0) damage_temp = 0;
    ctx.damage_base = damage_temp;
    ctx.damage      = damage_temp + ctx.me->query_skill(ctx.attack_type) / 4;
    if (ctx.damage) ctx.damage = (ctx.damage + g_utils.random(ctx.damage)) / 2;

    // Apply damage ratio and level scaling
    ctx.damage = ctx.damage * (100 + ctx.damage_ratio) / 100;
    ctx.damage = ctx.damage * (100 + ctx.me->query("level")) / 100;

    ctx.damage_bonus = ctx.me->query_str();

    // Force skill hit_ob effect
    if (ctx.me->query("mp_factor")
        && (ctx.me->query("mp") >= ctx.me->query("mp_factor"))) {
        const char* force_skill = ctx.me->query_skill_mapped("force");
        if (force_skill[0]) {
            const char* ptr
                = load_skill(force_skill)
                      ->hit_ob(ctx.me, ctx.victim, ctx.damage_bonus);
            if (ptr && ptr[0]) {
                ctx.result += ptr;
                ctx.result += "\n";
            }
        }
    }

    // Apply force ratio to damage_bonus
    ctx.damage_bonus = ctx.damage_bonus * (100 + ctx.force_ratio) / 100;

    // Attack skill hit_ob effect
    const char* ptr = load_skill(ctx.attack_skill)
                          ->hit_ob(ctx.me, ctx.victim, ctx.damage_bonus);
    if (ptr && ptr[0]) {
        ctx.result += ptr;
        ctx.result += "\n";
    }

    // Weapon or monster special hit_ob
    if (ctx.weapon) {
        ptr = dynamic_cast<IHittable*>(ctx.weapon)
                  ->hit_ob(ctx.me, ctx.victim, ctx.damage_bonus);
        if (ptr && ptr[0]) {
            ctx.result += ptr;
            ctx.result += "\n";
        }
    }
    else {
        ptr = ctx.me->hit_ob(ctx.me, ctx.victim, ctx.damage_bonus);
        if (ptr && ptr[0]) {
            ctx.result += ptr;
            ctx.result += "\n";
        }
    }

    // Apply damage_bonus to damage
    if (ctx.damage_bonus > 0)
        ctx.damage
            = ctx.damage
              + (ctx.damage_bonus + g_utils.random(ctx.damage_bonus)) / 2;

    if (ctx.damage <= 0) ctx.damage = 0;

    // Defense factor from combat exp
    unsigned long defense_factor
        = CChar::level2_exp(ctx.victim->query("level")) + 1;
    unsigned long my_exp = CChar::level2_exp(ctx.me->query("level"));
    if (my_exp < 0) my_exp = 1;

    while ((unsigned long)RANDOM(defense_factor, "combatd") > my_exp) {
        ctx.damage -= ctx.damage / 3;
        defense_factor /= 2;
        if (defense_factor <= 0) defense_factor = 1;
    }

    // 伤害结算前的增强钩子（hooks.calculate_damage）
    CLuaHookCall hook("calculate_damage");
    hook.add_object(ctx.me).add_object(ctx.victim).add_int(ctx.damage);
    if (hook.call() && !hook.result_is_nil()) {
        if (hook.result_is_number()) {
            ctx.damage = (int)hook.result_int();
        }
        else {
            int64_t damage = ctx.damage;
            if (hook.table_field_int("damage", damage))
                ctx.damage = (int)damage;
            std::string text;
            if (hook.table_field_str("text", text)) ctx.result += text;
        }
    }

    // Apply damage to victim
    ctx.damage = ctx.victim->receive_damage("hp", ctx.damage, ctx.me);

    // Wound mechanic: reduce max_hp
    if (ctx.damage
        && RANDOM(ctx.damage, "combatd") > ctx.victim->query_temp("apply/armor")
        && ((ctx.me->is_killing(ctx.victim))
                && ((!ctx.weapon) && !g_utils.random(4)
                    || ctx.weapon && !g_utils.random(2))
            || ((!ctx.weapon) && !g_utils.random(7)
                || ctx.weapon && !g_utils.random(4)))) {
        ctx.victim->receive_wound(
            "hp", ctx.damage - ctx.victim->query_temp("apply/armor"), ctx.me);
    }

    // Damage description
    ctx.result += damge_to_string(ctx.damage, false);

    CSetEffect::dispatch(ctx, SetEffectPhase::PostDamage);

    // Experience gain (NPC vs player)
    if (!ctx.me->query_attr(CChar::IS_USER)
        || !ctx.victim->query_attr(CChar::IS_USER)) {
        if (ctx.ap < ctx.dp && g_utils.random(100 + ctx.me->query_int()) > 30) {
            ctx.add_exp_me = 1 + g_utils.random(2 + ctx.me->learn_bonus() / 15);
            ctx.me->add("combat_exp", ctx.add_exp_me);
            ctx.me->improve_skill(ctx.attack_type, 1);
        }

        if (RANDOM(ctx.victim->query("max_hp") + ctx.victim->query("hp"),
                   "combatd")
            < ctx.damage) {
            ctx.add_exp_victim = 1;
            ctx.victim->add("combat_exp", ctx.add_exp_victim);
        }
    }
}

// ====================================================================
// Phase 9: Replace $N/$n/$w/$W/$d/$P/$p/$l placeholders
// ====================================================================
void CAttack::replace_placeholders(AttackContext& ctx)
{
    char str[512];
    strncpy(str, ctx.result.c_str(), 512);

    replace_string(str, "$l", "身体", 512);

    // $w = attacker's weapon name
    if (ctx.me->query_temp("action_flag") && ctx.left_weapon
        && EQUALSTR(ctx.left_weapon->id(1), "yang dao")) {
        replace_string(str, "$w", ctx.left_weapon->name(), 512);
    }
    else if (ctx.weapon) {
        replace_string(str, "$w", ctx.weapon->name(), 512);
    }
    else {
        if (ctx.left_weapon) {
            replace_string(str, "$w", ctx.left_weapon->name(), 512);
        }
    }

    // $W = victim's weapon name
    CContainer* weapon2 = ctx.victim->query_weapon();
    if (weapon2) replace_string(str, "$W", weapon2->name(), 512);

    // $d = damage value
    char vs[10];
    snprintf(vs, 10, "%d", (int)ctx.damage);
    replace_string(str, "$d", vs, 512);

    // Names and pronouns
    replace_string(str, "$N", ctx.me->name(), 512);
    replace_string(str, "$n", ctx.victim->name(), 512);
    replace_string(str, "$P", gender_pronoun(ctx.me->querystr("gender")), 512);
    replace_string(str, "$p", gender_pronoun(ctx.victim->querystr("gender")),
                   512);

    ctx.result = str;
}

// ====================================================================
// Phase 10: Dispatch combat message to room viewers
// ====================================================================
void CAttack::dispatch_message(AttackContext& ctx)
{
    int max_hp = ctx.victim->query("max_hp");
    int hp     = 0;
    if (max_hp > 0) hp = ctx.victim->query("hp") * 100 / max_hp;

    if (ctx.flag == CCombatd::TYPE_REGULAR) {
        char msg[512];
        snprintf(msg, 512, "&C=%d&A=%d&V=%d&S=%d&m=%d&D=%d&ap=%d&T=%s&M=%s&",
                 COMMON_ATTACK, ctx.me->object_id(), ctx.victim->object_id(),
                 hp, (int)ctx.me->query("mp"), (int)ctx.damage,
                 ctx.me->query_temp("fight/ap"), ctx.attack_type.c_str(),
                 ctx.result.c_str());

        CContainer* env = ctx.me->environment();
        MAP2USER ulist;
        iterator_user pos;
        env->get_users("combat", &ulist);

        int havesend = 0;
        for (pos = ulist.begin(); pos != ulist.end(); pos++) {
            CUser* obj = pos->second;
            obj->send_command(msg, SEND_WAIT_IMMEDIATE);
            if (obj == ctx.victim) havesend = 1;
        }

        if (!havesend) {
            ctx.victim->send_state(false);
        }

        const char* ch = env->querystr("channel_cast");
        if (ch[0]) {
            g_channel.do_channel(env, NULL, ch, ctx.result.c_str());
        }
    }
    else {
        message_channel("combat", ctx.result.c_str(), ctx.me);
    }

    // Tell experience gains
    char msg[512];
    if (ctx.add_exp_me)
        tell_object(ctx.me, snprintf(msg, 512, "(exp + %d)", ctx.add_exp_me));
    if (ctx.add_exp_victim)
        tell_object(ctx.victim,
                    snprintf(msg, 512, "(exp + %d)", ctx.add_exp_victim));
}

// ====================================================================
// Phase 11: Weapon effects (bash/throw)
// ====================================================================
void CAttack::apply_weapon_effects(AttackContext& ctx)
{
    if (EQUALSTR(ctx.attack_type, "hammer") || EQUALSTR(ctx.attack_type, "club")
        || EQUALSTR(ctx.attack_type, "staff"))
        CWeaponEffect::bash_weapon(ctx.me, ctx.victim, ctx.weapon, ctx.damage);

    if (EQUALSTR(ctx.attack_type, "throwing"))
        CWeaponEffect::throw_weapon(ctx.me, ctx.victim, ctx.weapon, ctx.damage);
}

// ====================================================================
// Phase 12: Wizard verbose debug output
// ====================================================================
void CAttack::send_verbose_debug(AttackContext& ctx)
{
    char msg[255];

    if (is_wizard(ctx.me)
        && EQUALSTR(ctx.me->querystr("env/combat"), "verbose")) {
        if (ctx.damage > 0) {
            _snprintf(
                msg, 255, "AP：%.2f，DP：%.2f，PP：%.2f，Mod：%d，伤害力：%d",
                ctx.ap, ctx.dp, ctx.pp, (int)ctx.mod_val, (int)ctx.damage);
        }
        else {
            _snprintf(msg, 255, "AP：%.2f，DP：%.2f，PP：%.2f，Mod：%d", ctx.ap,
                      ctx.dp, ctx.pp, (int)ctx.mod_val);
        }
        message_vision_room(ctx.me, "tell", msg);
    }

    if (is_wizard(ctx.victim)
        && EQUALSTR(ctx.victim->querystr("env/combat"), "verbose")) {
        if (ctx.damage > 0) {
            _snprintf(
                msg, 255, "AP：%.2f，DP：%.2f，PP：%.2f，Mod：%d，伤害力：%d",
                ctx.ap, ctx.dp, ctx.pp, (int)ctx.mod_val, (int)ctx.damage);
        }
        else {
            _snprintf(msg, 255, "AP：%.2f，DP：%.2f，PP：%.2f，Mod：%d", ctx.ap,
                      ctx.dp, ctx.pp, (int)ctx.mod_val);
        }
        message_vision_room(ctx.victim, "tell", msg);
    }
}

// ====================================================================
// damge_to_string - format damage into colored description
// ====================================================================
string CAttack::damge_to_string(int32_t damage, bool real)
{
    string result = "";

    char d[256] = "$d";
    if (real) {
        snprintf(d, 255, "%d", damage);
    }

    if (damage > 10000) {
        result += "结果造成了$HIR" + string(d) + "$COM点的伤害。";
    }
    else if (damage > 5000) {
        result += "结果造成了$HIY" + string(d) + "$COM点的伤害。";
    }
    else if (damage > 1000) {
        result += "结果造成了$HIG" + string(d) + "$COM点的伤害。";
    }
    else {
        result += "结果造成了" + string(d) + "点的伤害。";
    }
    return result;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// damge_to_string(damage[, real]) -> string  （CCombatd::damge_to_string）
// 把伤害数值格式化为带颜色的描述字符串
int l_damge_to_string(lua_State* L)
{
    int32_t damage = (int32_t)luaL_optinteger(L, 1, 0);
    // C++ 默认 real=true（显示具体数值），Lua 不传第二参时保持一致
    bool real = (lua_isnoneornil(L, 2)) ? true : lua_toboolean(L, 2);
    string s  = CAttack::damge_to_string(damage, real);
    lua_pushstring(L, s.c_str());
    return 1;
}
