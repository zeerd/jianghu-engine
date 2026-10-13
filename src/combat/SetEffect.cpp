/**
 * @file SetEffect.cpp
 * @brief 门派套装特效按阶段调用脚本钩子 hooks.set_effect 的实现。
 */

#include "SetEffect.h"

#include <string>

#include "Char.h"
#include "LuaHook.h"
#include "Skill.h"

// ====================================================================
// 阶段名（Lua 协议面字符串）
// ====================================================================
static const char* phase_name(SetEffectPhase phase)
{
    switch (phase) {
        case SetEffectPhase::ParryLanded:
            return "parry_landed";
        case SetEffectPhase::ParryMissed:
            return "parry_missed";
        case SetEffectPhase::PostDamage:
            return "post_damage";
        case SetEffectPhase::PostAttack:
            return "post_attack";
    }
    return "";
}

// ====================================================================
// 实参压入：按 hooks.set_effect 契约顺序压入阶段名与攻击上下文
// ====================================================================
static void push_args(CLuaHookCall& hook, SetEffectPhase phase,
                      const AttackContext& ctx, const char* parry_msg)
{
    hook.add_str(phase_name(phase))
        .add_object(ctx.me)
        .add_object(ctx.victim)
        .add_bool(ctx.weapon != nullptr)
        .add_str(ctx.parry_skill.c_str())
        .add_str(parry_msg)
        .add_int(ctx.damage)
        .add_int(ctx.damage_base);
}

// ====================================================================
// 分派：按阶段调用 hooks.set_effect，消费接管标记与写回
// ====================================================================
bool CSetEffect::dispatch(AttackContext& ctx, SetEffectPhase phase)
{
    // 招架未命中阶段：为脚本规则预取招架文案
    std::string parry_msg;
    if (phase == SetEffectPhase::ParryMissed) {
        const char* ptr
            = load_skill(ctx.parry_skill)->query_parry_msg(ctx.weapon);
        if (ptr) parry_msg = ptr;
    }

    CLuaHookCall hook("set_effect");
    push_args(hook, phase, ctx, parry_msg.c_str());
    if (!hook.call() || hook.result_is_nil()) return false;

    bool handled = false;
    hook.table_field_bool("handled", handled);
    int64_t damage = ctx.damage;
    if (hook.table_field_int("damage", damage)) ctx.damage = (int)damage;
    std::string text;
    if (hook.table_field_str("text", text)) ctx.result += text;
    return handled;
}

// ====================================================================
// 通知型阶段：只调用脚本，返回值不消费（脚本以副作用生效）
// ====================================================================
void CSetEffect::notify(const AttackContext& ctx, SetEffectPhase phase)
{
    CLuaHookCall hook("set_effect");
    push_args(hook, phase, ctx, "");
    hook.call();
}
