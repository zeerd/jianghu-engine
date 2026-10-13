/**
 * @file CombatTypes.h
 * @brief 战斗攻击类型枚举与结算哨兵值定义。
 */
#pragma once

// Attack type constants used across combat modules.
enum CombatAttackType {
    COMBAT_TYPE_QUICK   = 0,  // 法术攻击，传文字描述
    COMBAT_TYPE_REGULAR = 1,  // 普通攻击，传攻击
    COMBAT_TYPE_RIPOSTE = 2,  // 反击
    COMBAT_TYPE_NORMAL  = 3,  // 接触性物理攻击
    COMBAT_TYPE_REMOTE  = 4,  // 远程攻击
    COMBAT_TYPE_MAGIC   = 5,  // 魔法攻击
    COMBAT_TYPE_HIT     = 6,  // 必然命中的攻击
};

// 攻击结算哨兵值：ctx.damage 的特殊负值标记
constexpr int RESULT_DODGE = -1;  // 攻击被闪避/被套装特效接管
constexpr int RESULT_PARRY = -2;  // 攻击被招架
