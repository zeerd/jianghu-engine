/** @file LuaCondition.cpp @brief 由 Lua 脚本定义的 Condition 实现 */

#include <windows.h>

#include "LuaBind.h"
#include "LuaCondition.h"
#include "LuaEngine.h"

#include <lua.hpp>
#include "Global.h"

CLuaCondition::CLuaCondition(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
}

CLuaCondition::~CLuaCondition() {}

// 首次挂载时调用 Lua 脚本的 init 函数（若存在）
bool CLuaCondition::init(CChar* me)
{
    // 加载脚本（若已加载且文件修改则自动热重载）
    m_script_ref
        = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_CONDITION);
    if (m_script_ref == LUA_NOREF) {
        // 脚本加载失败：拒绝挂载，调用方不应持久化该条件
        CCondition::init(me);
        return false;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "init");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        // 脚本未定义 init：可选字段，视为挂载成功
        CCondition::init(me);
        return true;
    }

    // 栈: [table][func]
    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    // 栈: [table][func][self][me]
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua condition init failed: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1);  // 弹出 table
        return false;
    }
    lua_pop(L, 1);  // 弹出 table
    return true;
}

// 调用 Lua 脚本的 update_condition 函数，写回 duration 引用
int CLuaCondition::update_condition(CChar* me, LONG& duration)
{
    if (m_script_ref == LUA_NOREF)
        return CCondition::update_condition(me, duration);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "update_condition");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CCondition::update_condition(me, duration);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushinteger(L, duration);

    // 期望两个返回值：flag、new_duration
    if (lua_pcall(L, 3, 2, 0) != LUA_OK) {
        LOG_ERROR("Lua condition update_condition failed: %s",
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return CCondition::update_condition(me, duration);
    }

    // 第一返回值：flag
    int flag = 1;  // 默认 CND_CONTINUE，安全兜底
    if (lua_isnumber(L, -2)) flag = (int)lua_tointeger(L, -2);

    // 第二返回值：new_duration（number 才写回引用；nil 则不修改）
    if (lua_isnumber(L, -1)) duration = (int32_t)lua_tointeger(L, -1);

    lua_pop(L, 3);  // 弹出 new_duration、flag、table
    return flag;
}
