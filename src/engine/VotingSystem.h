/**
 * @file VotingSystem.h
 * @brief 投票子系统 — 管理投票选项与已投记录。
 *
 *
 */

#pragma once

#include "Mapping.h"

/**
 * @brief 投票管理器，持有两个 CMapping 数据表。
 *
 * 职责：
 * - voted_players 投票选项及得票
 * - votes_casted  已投票记录
 *
 */
class CVotingSystem {
public:
    // 投票选项
    CMapping voted_players;

    // 已投票记录
    CMapping votes_casted;

    CVotingSystem() = default;
    ~CVotingSystem();

    /** @brief 清空全部数据（create 时调用）。 */
    void clear()
    {
        voted_players.clear();
        votes_casted.clear();
    }
};
