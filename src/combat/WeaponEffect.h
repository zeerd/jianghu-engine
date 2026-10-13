

/**
 * @file WeaponEffect.h
 * @brief 武器交互效果（格挡碰撞/投掷消耗）。
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
