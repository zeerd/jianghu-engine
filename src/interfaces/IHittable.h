/**
 * @file IHittable.h
 * @brief 战斗参与者接口 — 可被攻击或造成伤害的对象。
 */

#pragma once

#include <cstdint>

class CChar;

/**
 * @brief 战斗参与者接口。
 *
 * 实现者：CChar（角色身体）、CItem（武器）。
 * 调用方：Attack.cpp、Corpse.cpp。
 */
class IHittable {
public:
    virtual ~IHittable() = default;

    /**
     * @brief 被攻击时的特效处理。
     * @param me      攻击者
     * @param victim  被击中者
     * @param damage_bonus  伤害加成
     * @return 描述文字，nullptr 或空串表示无特效
     */
    virtual const char* hit_ob(CChar* me, CChar* victim, int32_t damage_bonus)
        = 0;

    /**
     * @brief 持有者被击杀时的回调（武器掉落、装备消失等）。
     * @param killer  击杀者
     */
    virtual void owner_is_killed(CChar* killer) = 0;
};
