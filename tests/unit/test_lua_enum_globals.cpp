// test_lua_enum_globals.cpp — Lua 枚举常量全局名契约测试
//
// 契约：LuaBind_Init 导出的每个枚举/宏常量必须以裸名存在于 Lua 全局环境，
// 且取值等于其权威定义（C++ 枚举或协议常量表），独立于各模块注册数组，
// 因此能捕捉：条目丢失/改名、模块注册函数未被调用、注册遍历 bug、取值错位。
#include "pch.h"

#include <lua.hpp>

#include <cstddef>

#include "Char.h"
#include "Combatd.h"
#include "FightRoom.h"
#include "Global.h"
#include "LuaBind.h"
#include "Skill.h"

namespace {

struct EnumCase {
    const char* name;
    lua_Integer expected;
};

class LuaEnumGlobals : public ::testing::Test {
protected:
    void SetUp() override
    {
        L = luaL_newstate();
        ASSERT_NE(L, nullptr);
        luaL_openlibs(L);
        lua_bind_init(L);
    }
    void TearDown() override
    {
        if (L) lua_close(L);
    }

    // 以编译期数组长度遍历，不依赖哨兵，避免越界读
    template <std::size_t N>
    void ExpectGlobals(const EnumCase (&cases)[N])
    {
        for (const EnumCase* c = cases; c != cases + N; ++c) {
            lua_getglobal(L, c->name);
            EXPECT_TRUE(lua_isinteger(L, -1)) << c->name << " missing";
            if (lua_isinteger(L, -1)) {
                EXPECT_EQ(lua_tointeger(L, -1), c->expected) << c->name;
            }
            lua_pop(L, 1);
        }
    }

    lua_State* L = nullptr;
};

// CObject.h 属性枚举（CMainObject 类内）
TEST_F(LuaEnumGlobals, ObjectAttr)
{
    static const EnumCase kCases[] = {
        {"IS_ITEM", (lua_Integer)CMainObject::IS_ITEM},
        {"IS_FIGHTROOM", (lua_Integer)CMainObject::IS_FIGHTROOM},
        {"IS_CORPSE", (lua_Integer)CMainObject::IS_CORPSE},
        {"IS_EQUIP", (lua_Integer)CMainObject::IS_EQUIP},
        {"IS_COMBINED", (lua_Integer)CMainObject::IS_COMBINED},
        {"IS_MONEY", (lua_Integer)CMainObject::IS_MONEY},
        {"IS_NPC", (lua_Integer)CMainObject::IS_NPC},
        {"IS_VENDOR", (lua_Integer)CMainObject::IS_VENDOR},
        {"IS_CONTAINER", (lua_Integer)CMainObject::IS_CONTAINER},
        {"IS_ROOM", (lua_Integer)CMainObject::IS_ROOM},
        {"IS_FBROOM", (lua_Integer)CMainObject::IS_FBROOM},
        {"IS_USER", (lua_Integer)CMainObject::IS_USER},
        {"IS_CHAR", (lua_Integer)CMainObject::IS_CHAR},
        {"IS_LIVING", (lua_Integer)CMainObject::IS_LIVING},
        {"IS_MONSTER", (lua_Integer)CMainObject::IS_MONSTER},
        {"IS_FOOD", (lua_Integer)CMainObject::IS_FOOD},
        {"CAN_COMBINE", (lua_Integer)CMainObject::CAN_COMBINE},
        {"CAN_EQUIPPED", (lua_Integer)CMainObject::CAN_EQUIPPED},
        {"CAN_GET", (lua_Integer)CMainObject::CAN_GET},
        {"IS_BOX", (lua_Integer)CMainObject::IS_BOX},
        {"IS_RESIDENT", (lua_Integer)CMainObject::IS_RESIDENT},
        {"GRANT_PLAYER", (lua_Integer)CMainObject::GRANT_PLAYER},
    };
    ExpectGlobals(kCases);
}

// Char.h 动作枚举（CChar 类内）
TEST_F(LuaEnumGlobals, CharActions)
{
    static const EnumCase kCases[] = {
        {"DO_WIELD", (lua_Integer)CChar::DO_WIELD},
        {"DO_UNWIELD", (lua_Integer)CChar::DO_UNWIELD},
        {"DO_USE", (lua_Integer)CChar::DO_USE},
        {"DO_LIST", (lua_Integer)CChar::DO_LIST},
        {"DO_TALK", (lua_Integer)CChar::DO_TALK},
    };
    ExpectGlobals(kCases);
}

// Char.h condition 标志位（全局宏）
TEST_F(LuaEnumGlobals, CharConditions)
{
    static const EnumCase kCases[] = {
        {"CND_CONTINUE", (lua_Integer)CND_CONTINUE},
        {"CND_NO_HEAL_UP", (lua_Integer)CND_NO_HEAL_UP},
    };
    ExpectGlobals(kCases);
}

// Skill.h 施法时机/目标枚举（CSkill 类内）
TEST_F(LuaEnumGlobals, SkillCast)
{
    static const EnumCase kCases[] = {
        {"CAST_IN_NONE", (lua_Integer)CSkill::CAST_IN_NONE},
        {"CAST_IN_COMMON", (lua_Integer)CSkill::CAST_IN_COMMON},
        {"CAST_IN_COMBAT", (lua_Integer)CSkill::CAST_IN_COMBAT},
        {"CAST_IN_BOTH", (lua_Integer)CSkill::CAST_IN_BOTH},
        {"CAST_TO_NONE", (lua_Integer)CSkill::CAST_TO_NONE},
        {"CAST_TO_ME", (lua_Integer)CSkill::CAST_TO_ME},
        {"CAST_TO_OTHER", (lua_Integer)CSkill::CAST_TO_OTHER},
        {"CAST_TO_RANDOM", (lua_Integer)CSkill::CAST_TO_RANDOM},
        {"CAST_TO_FRIEND", (lua_Integer)CSkill::CAST_TO_FRIEND},
        {"CAST_TO_TEAM", (lua_Integer)CSkill::CAST_TO_TEAM},
        {"CAST_TO_OTHER_TEAM", (lua_Integer)CSkill::CAST_TO_OTHER_TEAM},
    };
    ExpectGlobals(kCases);
}

// Combatd.h 攻击类型枚举（CCombatd 类内）
TEST_F(LuaEnumGlobals, CombatAttackType)
{
    static const EnumCase kCases[] = {
        {"TYPE_QUICK", (lua_Integer)CCombatd::TYPE_QUICK},
        {"TYPE_REGULAR", (lua_Integer)CCombatd::TYPE_REGULAR},
        {"TYPE_RIPOSTE", (lua_Integer)CCombatd::TYPE_RIPOSTE},
        {"TYPE_NORMAL", (lua_Integer)CCombatd::TYPE_NORMAL},
        {"TYPE_REMOTE", (lua_Integer)CCombatd::TYPE_REMOTE},
        {"TYPE_MAGIC", (lua_Integer)CCombatd::TYPE_MAGIC},
        {"TYPE_HIT", (lua_Integer)CCombatd::TYPE_HIT},
    };
    ExpectGlobals(kCases);
}

// Global.h 装备位置枚举（全局 enum）
TEST_F(LuaEnumGlobals, WieldPos)
{
    static const EnumCase kCases[] = {
        {"WIELD_NONE", (lua_Integer)WIELD_NONE},
        {"WIELD_HEAD", (lua_Integer)WIELD_HEAD},
        {"WIELD_LEFT_ERR", (lua_Integer)WIELD_LEFT_ERR},
        {"WIELD_RIGHT_ERR", (lua_Integer)WIELD_RIGHT_ERR},
        {"WIELD_ERR", (lua_Integer)WIELD_ERR},
        {"WIELD_LEFT_HAND", (lua_Integer)WIELD_LEFT_HAND},
        {"WIELD_RIGHT_HAND", (lua_Integer)WIELD_RIGHT_HAND},
        {"WIELD_HAND", (lua_Integer)WIELD_HAND},
        {"WIELD_ARMOR", (lua_Integer)WIELD_ARMOR},
        {"WIELD_NECK", (lua_Integer)WIELD_NECK},
        {"WIELD_WAIST", (lua_Integer)WIELD_WAIST},
        {"WIELD_BOOT", (lua_Integer)WIELD_BOOT},
        {"WIELD_MANTLE", (lua_Integer)WIELD_MANTLE},
        {"WIELD_ARMOR_HANDS", (lua_Integer)WIELD_ARMOR_HANDS},
        {"WIELD_BOTH_HAND", (lua_Integer)WIELD_BOTH_HAND},
        {"WIELD_RING", (lua_Integer)WIELD_RING},
        {"WIELD_CLOTH", (lua_Integer)WIELD_CLOTH},
    };
    ExpectGlobals(kCases);
}

// FightRoom.h 战斗锁宏（全局宏）
TEST_F(LuaEnumGlobals, FightRoomLock)
{
    static const EnumCase kCases[] = {
        {"FIGHT_LOCKED", (lua_Integer)FIGHT_LOCKED},
        {"FIGHT_NO_FLEE", (lua_Integer)FIGHT_NO_FLEE},
        {"FIGHT_NO_GUARD", (lua_Integer)FIGHT_NO_GUARD},
    };
    ExpectGlobals(kCases);
}

// FightRoom.h 战队枚举（CFightRoom 类内）
TEST_F(LuaEnumGlobals, FightRoomTeam)
{
    static const EnumCase kCases[] = {
        {"FIGHT_TEAM_A", (lua_Integer)CFightRoom::FIGHT_TEAM_A},
        {"FIGHT_TEAM_B", (lua_Integer)CFightRoom::FIGHT_TEAM_B},
    };
    ExpectGlobals(kCases);
}

}  // namespace
