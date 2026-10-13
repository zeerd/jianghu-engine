/**
 * @file SetEffect.h
 * @brief 门派套装特效：按攻击管线阶段调用脚本钩子的分派声明。
 */
#pragma once

#include "Attack.h"

// 特效触发阶段（对应攻击管线中的挂点）
enum class SetEffectPhase {
    ParryLanded,  // 招架成立后
    ParryMissed,  // 招架失败后（可接管本次攻击）
    PostDamage,   // 伤害结算后
    PostAttack,   // 本次攻击结算后（武器特效之后）
};

/**
 * @brief 套装特效分派器：按阶段调用 hooks.set_effect，
 *        返回接管标记（true=跳过后续伤害结算）。
 */
class CSetEffect {
public:
    // 分派指定阶段的脚本规则（匹配序：套装键→门派→招架映射→武器→概率）；
    // 返回 true 表示攻击被特效接管，调用方应跳过伤害结算。
    static bool dispatch(AttackContext& ctx, SetEffectPhase phase);

    // 通知型阶段：调用脚本后不消费返回值（脚本侧直接以副作用生效）。
    static void notify(const AttackContext& ctx, SetEffectPhase phase);
};
