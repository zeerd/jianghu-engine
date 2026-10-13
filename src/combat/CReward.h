/**
 * @file CReward.h
 * @brief 战斗奖励系统，管理经验/金钱分配与 PK 惩罚。
 */////////////////////////////////////////////////////////////////////

#pragma once

#include "Container.h"

class CUser;
class CChar;

/**
 * @brief 战斗奖励处理器，负责击杀奖励、PK 惩罚与组队经验分配。
 */
class CReward {
public:
    int victim_lose(CChar* killer, CChar* victim);
    void victim_penalty(CChar* victim);
    int nk_gain(CChar* killer, CChar* victim);
    void pk_reward(CChar* killer, CChar* victim);
    void killer_reward(CChar* killer, CChar* victim);

    CReward();
    virtual ~CReward() {};

private:
    int calc_nkgain_base(CChar* victim);
    int calc_party_exp(CChar* killer, int32_t nkgain, int32_t cx,
                       CUser*& player1, CUser*& player2, CUser*& player3);
    void distribute_party_exp(CChar* killer, CChar* victim, int32_t reward,
                              int32_t cx, CUser* player1, CUser* player2,
                              CUser* player3);
    void handle_pk_reward(CChar* killer, CChar* victim, int pkreward,
                          int pk_chang, int32_t lose, int32_t gain,
                          const char* killerid, CContainer* env);
    void handle_npc_kill_reward(CChar* killer, CChar* victim, int32_t reward);
};
