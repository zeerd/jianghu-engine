/**
 * @file Money.h
 * @brief 金钱物品，支持自动合并与支付操作。
 */

#pragma once

#include "Item.h"

/**
 * @brief 金钱物品，同一位置自动合并，支持支付与收款。
 */
class CMoney : public CItem {
public:
    virtual int query_autoload() { return 1; };
    static char* money_str(money_t amount);
    static int player_pay(CChar* me, money_t value);
    static int pay_player(CChar* me, money_t value);
    virtual int32_t query_attr(int attr);
    virtual int move(CContainer* me);
    CMoney();
    virtual ~CMoney();
};

class CMCoin : public CMoney {
public:
    virtual void create(int flag = 0);
    CMCoin() {}
    virtual ~CMCoin() {}
};
