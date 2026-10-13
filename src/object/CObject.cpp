/**
 *  @file CObject.cpp
 * @brief 游戏对象基类实现 */

#include "CObject.h"

#include "LuaBindEnums.h"

// CObject.h 属性枚举 → Lua 全局常量
static const LuaEnumEntry kObjectAttrEnums[] = {
    {"IS_ITEM", CMainObject::IS_ITEM},
    {"IS_FIGHTROOM", CMainObject::IS_FIGHTROOM},
    {"IS_CORPSE", CMainObject::IS_CORPSE},
    {"IS_EQUIP", CMainObject::IS_EQUIP},
    {"IS_COMBINED", CMainObject::IS_COMBINED},
    {"IS_MONEY", CMainObject::IS_MONEY},
    {"IS_NPC", CMainObject::IS_NPC},
    {"IS_VENDOR", CMainObject::IS_VENDOR},
    {"IS_CONTAINER", CMainObject::IS_CONTAINER},
    {"IS_ROOM", CMainObject::IS_ROOM},
    {"IS_FBROOM", CMainObject::IS_FBROOM},
    {"IS_USER", CMainObject::IS_USER},
    {"IS_CHAR", CMainObject::IS_CHAR},
    {"IS_LIVING", CMainObject::IS_LIVING},
    {"IS_MONSTER", CMainObject::IS_MONSTER},
    {"IS_FOOD", CMainObject::IS_FOOD},
    {"CAN_COMBINE", CMainObject::CAN_COMBINE},
    {"CAN_EQUIPPED", CMainObject::CAN_EQUIPPED},
    {"CAN_GET", CMainObject::CAN_GET},
    {"IS_BOX", CMainObject::IS_BOX},
    {"GRANT_PLAYER", CMainObject::GRANT_PLAYER},
    {nullptr, 0},
};

void lua_bind_reg_object_attr_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kObjectAttrEnums);
}
