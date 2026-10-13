/**
 * @file IEquippable.h
 * @brief 可装备接口 — 可被穿戴或握持的对象。
 */

#pragma once

/**
 * @brief 可装备接口。
 *
 * 实现者：CItem（武器/防具）。
 * 调用方：Npc.cpp、Lua 脚本。
 */
class IEquippable {
public:
    virtual ~IEquippable() = default;

    /** @brief 穿上装备。 @return 成功返回非零 */
    virtual int wear() = 0;

    /** @brief 握持装备。 @return 成功返回非零 */
    virtual int wield() = 0;
};
