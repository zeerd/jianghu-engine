/**
 * @file LuaBindTypes.h
 * @brief Lua 绑定的共享 userdata 辅助类型与对象提取函数。
 */
#pragma once
#include <lua.hpp>
class CMainObject;

struct LuaObject {
    CMainObject* obj;
};

static inline CMainObject* get_object(lua_State* L, int idx)
{
    LuaObject* ud = (LuaObject*)luaL_checkudata(L, idx, "jianghu_object");
    if (!ud) return NULL;
    return ud->obj;
}

class CChar;
// helper: fetch CChar* (or NULL) from a userdata arg. Defined in LuaBind.cpp.
CChar* lua_check_char(lua_State* L, int idx);
