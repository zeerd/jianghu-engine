/**
 * @file LuaCondition.h
 * @brief 由 Lua 脚本定义的 Condition 实现，重写 init() 与 update_condition()。
 */

/////////////////////////////////////////////////////////////////////

#pragma once

#include "Condition.h"

/**
 * @brief Lua 脚本驱动的持续效果，将 init/update_condition 转发到 Lua 层。
 */
class CLuaCondition : public CCondition {
public:
    CLuaCondition(const char* scriptName);
    virtual ~CLuaCondition();

    // 重写 init：首次挂载时调用 Lua 脚本的 init 函数（若存在）；
    // 脚本加载失败或 init 抛错返回 false，调用方拒绝写入条件项
    virtual bool init(CChar* me);

    // 重写 update_condition：调用 Lua 脚本的 update_condition 函数
    // 返回 flag，并将第二返回值（number）写回 duration 引用
    virtual int update_condition(CChar* me, LONG& duration);

    // 脚本名
    const char* get_script_name() { return m_script_name.c_str(); }

    // 脚本引用
    int get_script_ref() { return m_script_ref; }

private:
    std::string m_script_name;
    int m_script_ref;  // 脚本 table 的 registry 引用（LUA_NOREF 表示失败）
};
