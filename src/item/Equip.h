/**
 * @file Equip.h
 * @brief 可装备物品，管理穿戴/脱卸与动作处理。
 */

#pragma once

#include "Item.h"

/**
 * @brief 可装备物品，支持穿戴、脱卸与装备相关动作。
 */
class CEquip : public CItem {
public:
    virtual int do_action(int act, CChar* me = NULL);
    virtual int32_t query_attr(int attr);
    virtual int can_equip(CChar* me);
    CEquip();
    virtual ~CEquip();

    virtual void on_moved(CContainer* me);

protected:
    int do_un_wield();
    int do_wield();
};
