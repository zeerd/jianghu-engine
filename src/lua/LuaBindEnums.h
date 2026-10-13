/**
 * @file LuaBindEnums.h
 * @brief Lua 枚举常量注册：条目结构、遍历注册函数与各模块注册入口声明。
 */
#pragma once

#include <lua.hpp>

// 单条 Lua 全局常量：name 为 Lua 侧裸名（不带 :: 前缀），value 为 C++ 枚举/宏值
struct LuaEnumEntry {
    const char* name  = nullptr;
    lua_Integer value = 0;
};

// 遍历条目表逐条注册到全局环境，表以 name == nullptr 结尾
inline void lua_bind_reg_enums(lua_State* L, const LuaEnumEntry* entries)
{
    lua_pushglobaltable(L);
    for (const LuaEnumEntry* e = entries; e->name; ++e) {
        lua_pushinteger(L, e->value);
        lua_setfield(L, -2, e->name);
    }
    lua_pop(L, 1);
}

// 各模块注册入口；条目数组定义在枚举所属头文件对应的源文件中
void lua_bind_reg_object_attr_enums(lua_State* L);  // object/CObject.h
void lua_bind_reg_char_enums(lua_State* L);         // char/Char.h
void lua_bind_reg_skill_enums(lua_State* L);        // skill/Skill.h
void lua_bind_reg_combat_enums(lua_State* L);       // combat/Combatd.h
void lua_bind_reg_wield_enums(lua_State* L);        // object/Global.h
void lua_bind_reg_fight_room_enums(lua_State* L);   // item/FightRoom.h
