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
    exp_t victim_lose(CChar* killer, CChar* victim);
    void victim_penalty(CChar* victim);
    exp_t nk_gain(CChar* killer, CChar* victim);
    void pk_reward(CChar* killer, CChar* victim);
    void killer_reward(CChar* killer, CChar* victim);

    CReward();
    virtual ~CReward() {};

private:
    exp_t calc_nkgain_base(CChar* victim);
    exp_t calc_party_exp(CChar* killer, exp_t nkgain, exp_t cx, CUser*& player1,
                         CUser*& player2, CUser*& player3);
    void distribute_party_exp(CChar* killer, CChar* victim, exp_t reward,
                              exp_t cx, CUser* player1, CUser* player2,
                              CUser* player3);
    void handle_pk_reward(CChar* killer, CChar* victim, int pkreward,
                          int pk_chang, exp_t lose, const char* killerid,
                          CContainer* env);
    void handle_npc_kill_reward(CChar* killer, CChar* victim, exp_t reward);
};
