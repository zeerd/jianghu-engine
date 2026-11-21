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
    // 无经验入账的统一出口：所有「这次不发经验」的分支都经它落日志，
    // 同一条前缀便于按前缀检索是哪道闸拦下的
    static exp_t log_zero_gain(const char* reason, CChar* killer,
                               CChar* victim);
    exp_t calc_nkgain_base(CChar* victim);
    // eff_dx 有效经验 = 受害者实战经验；nkgain 受害者经验档位；cx
    // 全队最高实战经验
    exp_t calc_party_exp(exp_t eff_dx, exp_t nkgain, exp_t cx);
    void distribute_party_exp(CChar* killer, CChar* victim, exp_t reward,
                              exp_t cx, CUser* player1, CUser* player2,
                              CUser* player3);
    void handle_pk_reward(CChar* killer, CChar* victim, int pkreward,
                          int pk_chang, exp_t lose, const char* killerid,
                          CContainer* env);
    void handle_npc_kill_reward(CChar* killer, CChar* victim, exp_t reward);
};
