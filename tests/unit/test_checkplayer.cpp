// test_checkplayer.cpp — Unit tests for CheckPlayer (LuaLogin)
//
// Tests the CheckPlayer function which calls login.on_login in Lua.
// Verifies attribute validation: total == 84 (pass) vs total > 84 (fail).
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Container.h"
#include "LuaLogin.h"
#include "test_lua_env.h"

// ====================================================================
// Test fixture: initializes Lua engine from test_config.ini
// ====================================================================
class CheckPlayerTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    void TearDown() override
    {
        // Don't shutdown — the singleton persists across tests
    }

    // Helper: create a CChar with specified 4 attributes
    CChar* MakeChar(int str, int con, int intel, int dex)
    {
        CChar* ch = new CChar();
        ch->set("str", (long)str);
        ch->set("con", (long)con);
        ch->set("int", (long)intel);
        ch->set("dex", (long)dex);
        ch->set("reward/attrib", (long)0);
        return ch;
    }

    // 销毁角色：属性超限时 login.on_login 会把角色移进缓存房间"jy"，
    // 必须先脱身再销毁，否则房间物件表里留下悬垂指针，后续用例踩到即崩
    static void DestroyChar(CChar* ch)
    {
        ch->move(nullptr);
        delete ch;
    }
};

// ====================================================================
// Test: 4 attributes sum to exactly 84 → should PASS (return 0)
// ====================================================================
// ====================================================================

TEST_F(CheckPlayerTest, LoginScript_MissingOnLogin_FailsGracefully)
{
    ScopedLoginScript guard("return { }");
    CChar* ch = MakeChar(20, 21, 22, 21);
    EXPECT_EQ(-1, check_player(ch, 1)) << "无 on_login → -1(24-25)";
    DestroyChar(ch);
}

TEST_F(CheckPlayerTest, LoginScript_OnLoginRuntimeError_FailsGracefully)
{
    ScopedLoginScript guard(
        "return { on_login = function(me, newenter) error('boom') end }");
    CChar* ch = MakeChar(20, 21, 22, 21);
    EXPECT_EQ(-1, check_player(ch, 1)) << "钩子运行期报错 → -1(33-35)";
    DestroyChar(ch);
}
TEST_F(CheckPlayerTest, AttributesEqual84_Pass)
{
    // 20+21+22+21 = 84
    CChar* ch  = MakeChar(20, 21, 22, 21);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0) << "Attributes sum to 84, should pass";
    DestroyChar(ch);
}

// ====================================================================
// Test: 4 attributes sum to less than 84 → should PASS (return 0)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesLessThan84_Pass)
{
    // 10+10+10+10 = 40
    CChar* ch  = MakeChar(10, 10, 10, 10);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0) << "Attributes sum to 40 (< 84), should pass";
    DestroyChar(ch);
}

// ====================================================================
// Test: 4 attributes sum to 85 (> 84) → should FAIL (return 1)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesExceed84_Fail)
{
    // 21+21+22+21 = 85
    CChar* ch  = MakeChar(21, 21, 22, 21);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1) << "Attributes sum to 85 (> 84), should fail";
    DestroyChar(ch);
}

// ====================================================================
// Test: 4 attributes sum to 100 (way over) → should FAIL (return 1)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesWayOver_Fail)
{
    // 25+25+25+25 = 100
    CChar* ch  = MakeChar(25, 25, 25, 25);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1) << "Attributes sum to 100, should fail";
    DestroyChar(ch);
}

// ====================================================================
// Test: with reward/attrib bonus, higher total is allowed
// ====================================================================
TEST_F(CheckPlayerTest, WithBonus_Pass)
{
    // 25+25+25+25 = 100, but reward/attrib = 20 → limit = 104
    CChar* ch = MakeChar(25, 25, 25, 25);
    ch->set("reward/attrib", (long)20);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0)
        << "Attributes sum to 100 with bonus 20 (limit=104), should pass";
    DestroyChar(ch);
}

// ====================================================================
// Test: with reward/attrib bonus, still fails if over limit
// ====================================================================
TEST_F(CheckPlayerTest, WithBonus_StillFail)
{
    // 25+25+25+25 = 100, reward/attrib = 10 → limit = 94
    CChar* ch = MakeChar(25, 25, 25, 25);
    ch->set("reward/attrib", (long)10);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1)
        << "Attributes sum to 100 with bonus 10 (limit=94), should fail";
    DestroyChar(ch);
}

// ====================================================================
// Test: 拒绝准入码 2（login.on_login 三态契约）原样透传
// ====================================================================
TEST_F(CheckPlayerTest, DenyCode2_Propagates)
{
    ScopedLoginScript guard(
        "return { on_login = function(me, newenter) return 2 end }");
    CChar* ch = MakeChar(20, 21, 22, 21);
    EXPECT_EQ(2, check_player(ch, 1)) << "准入拒绝码 2 原样透传给引擎";
    DestroyChar(ch);
}
