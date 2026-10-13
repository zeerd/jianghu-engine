/**
 * @file LuaBind.h
 * @brief C++ 对象到 Lua 的绑定初始化，注册 metatable 和全局函数。
 */

//---------------------------------------------------------

#pragma once

struct lua_State;
class CMainObject;

// 初始化绑定（注册 metatable 和全局函数）
void lua_bind_init(lua_State* L);

// 把一个 C++ 对象压入 Lua 栈（作为 userdata）
void lua_bind_push_object(lua_State* L, CMainObject* obj);

// 从 Lua 栈取一个对象（检查类型）
CMainObject* lua_bind_check_object(lua_State* L, int idx);
