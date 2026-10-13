/**
 * @file Condition.h
 * @brief 持续效果基类，管理中毒/增益等状态效果的挂载与更新。
 */

#pragma once

#include "Char.h"

/**
 * @brief 持续效果基类，子类重写 init() 与 update_condition() 实现具体效果。
 */
class CCondition : public CMainObject {
public:
    // 挂载初始化：返回 false 表示脚本/实现缺失，调用方不应持久化该条件。
    virtual bool init(CChar* me) { return true; }
    virtual int update_condition(CChar* me, LONG& duration) { return 0; }
    CCondition() {}
    virtual ~CCondition() {}
};
