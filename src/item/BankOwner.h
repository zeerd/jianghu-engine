/**
 * @file BankOwner.h
 * @brief 银行出纳 NPC，提供存款/取款/贷款/余额查询。
 */

#pragma once

#include "Npc.h"

/**
 * @brief 银行出纳 NPC，处理存款、取款、贷款与余额查询。
 */
// --- test fixture friend ---
class BankOwnerTest;

class CBankOwner : public CNpc {
public:
    virtual int32_t query_attr(int attr);
    virtual int handle_action(char* comm, CChar* me, char* arg);
    int do_loan(CChar* me, int32_t amount);
    int do_withdraw(CChar* me, int32_t amount);
    int do_deposit(CChar* me, int32_t amount);
    CBankOwner();
    virtual ~CBankOwner();

    friend class BankOwnerTest;

protected:
    int do_balance(CChar* me);
};
