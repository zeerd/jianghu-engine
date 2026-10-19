

/**
 * @file WeaponEffect.h
 * @brief 武器交互效果（格挡碰撞/投掷：找暗器与投掷消耗）。
 */
#pragma once

#include <cstdint>

class CChar;
class CContainer;

/**
 * @brief 武器效果处理器，提供格挡碰撞与投掷消耗逻辑。
 */
class CWeaponEffect {
public:
    // Weapon collision on parry (may disarm or break victim's weapon)
    static void bash_weapon(CChar* me, CChar* victim, CContainer* weapon,
                            int32_t damage);

    // Consumable weapon decay (e.g. throwing weapons)
    static void throw_weapon(CChar* me, CChar* victim, CContainer* weapon,
                             int32_t damage);
};

// 取角色身上第一件暗器（skill_type == "throwing"）：
// 已装备的优先，其次背包首个；指定 name 时按名字点选，找不到则回落首个。
// 投掷暗器的前一步（找到暗器 → 投出去）都在这里。
CContainer* get_first_throwing_weapon(CChar* me, const char* name = NULL);
