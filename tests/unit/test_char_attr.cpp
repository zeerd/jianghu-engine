// test_char_attr.cpp - Unit tests for CharAttr.cpp (CChar attribute/exp
// queries).
//
//
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "ItemMgr.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "../config/MainConfig.h"

class CharAttrTest : public ::testing::Test {
protected:
    CChar* ch;
    IItemMgr* savedItemMgr;
    CMockItemMgr itemMgr;
    void SetUp() override
    {
        // 先起 Lua 环境再 set_skill：无环境加载的技能会以未挂脚本的形态留在
        // g_skill_list，后续按名取到的便是 query_dodge_msg/query_action
        // 恒空的僵尸
        ensure_lua_env();
        ch = new CChar();
        ch->set("name", "attr");
        ch->set("id", "attr1");
        // Ensure a valid item manager regardless of test execution order.
        savedItemMgr = g_item_mgr;
        g_item_mgr   = &itemMgr;
    }
    void TearDown() override
    {
        g_item_mgr = savedItemMgr;
        delete ch;
    }
};

// ---------------- attr(str/con/int/dex) ----------------
TEST_F(CharAttrTest, QueryStr_Basic)
{
    ch->set("str", (long)20);
    EXPECT_EQ(ch->attr("str"), 20);
}

TEST_F(CharAttrTest, QueryStr_WithApply)
{
    ch->set("str", (long)20);
    ch->set_temp("apply/str", (long)5);
    EXPECT_EQ(ch->attr("str"), 25);
}

TEST_F(CharAttrTest, QueryCon_WithForce)
{
    ch->set("con", (long)15);
    ch->set_skill("force", 100);
    EXPECT_EQ(ch->attr("con"), 15 + 100 / 10);
}

TEST_F(CharAttrTest, QueryInt_WithLiterate)
{
    ch->set("int", (long)12);
    ch->set_skill("literate", 50);
    EXPECT_EQ(ch->attr("int"), 12 + 50 / 10);
}

TEST_F(CharAttrTest, QueryDex_WithDodge)
{
    ch->set("dex", (long)18);
    ch->set_skill("dodge", 30);
    EXPECT_EQ(ch->attr("dex"), 18 + 30 / 10);
}

// ---------------- exp queries ----------------
TEST_F(CharAttrTest, QueryExpAndCombat)
{
    ch->set("combat_exp", (long)50000);
    EXPECT_EQ(ch->attr("combat_exp"), 50000);
    EXPECT_EQ(ch->query_magic_exp(), 50000u);
}

// 未知名是调用方写错键：传 ok 时交回调用方处置（不落错误日志）
TEST_F(CharAttrTest, AttrUnknownKey_HandsBackToCaller)
{
    ch->set("combat_exp", (long)50000);

    bool ok = true;
    EXPECT_EQ(ch->attr("bogus", &ok), 0);
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(ch->attr("combat_exp", &ok), 50000);
    EXPECT_TRUE(ok);
}

// 不传 ok 的调用方（C++ 侧常量键）：写错键即缺陷，落错误日志并返回 0
TEST_F(CharAttrTest, AttrUnknownKey_LogsAndReturnsZero)
{
    EXPECT_EQ(ch->attr("bogus"), 0);
}

TEST_F(CharAttrTest, QueryMagicSkill_Delegates)
{
    ch->set_skill("unarmed", 40);
    EXPECT_EQ(ch->query_magic_skill("unarmed", 1), 40);
}

TEST_F(CharAttrTest, QueryValidExp)
{
    ch->set("level", 1);
    ch->set("combat_exp", 1000);
    ch->set("sk_exp", MAPPING_VALUE_ZERO);
    // level2exp(1) == 0, so valid = 1000 - 0 - 0
    EXPECT_EQ(ch->query_valid_exp(), 1000);
}

// ---------------- per / grant / kar / loading ----------------
TEST_F(CharAttrTest, QueryPer_WithApply)
{
    ch->set("per", 10);
    ch->set_temp("apply/per", 3);
    EXPECT_EQ(ch->attr("per"), 13);
}

TEST_F(CharAttrTest, QueryGrant)
{
    ch->set("grant", 30);
    EXPECT_EQ(ch->query_grant(), 30);
}

TEST_F(CharAttrTest, IsLoading_Flag)
{
    EXPECT_EQ(ch->is_loading(), 0);
    ch->set_temp("client/loading", 1);
    EXPECT_EQ(ch->is_loading(), 1);
}

TEST_F(CharAttrTest, RefuseChannel)
{
    EXPECT_EQ(ch->refuse_channel("rumor"), 0);
    ch->set_temp("refuse/rumor", 1);
    EXPECT_EQ(ch->refuse_channel("rumor"), 1);
}

// ---------------- learn_bonus branches ----------------
TEST_F(CharAttrTest, LearnBonus_Zero)
{
    ch->set("level", 0);
    ch->set("int", 0);
    EXPECT_EQ(ch->learn_bonus(), 1);
}

TEST_F(CharAttrTest, LearnBonus_Small)
{
    // pt = level*int/50 ; level=10 int=20 -> 4 (<=30)
    ch->set("level", (long)10);
    ch->set("int", (long)20);
    EXPECT_EQ(ch->learn_bonus(), 4);
}

TEST_F(CharAttrTest, LearnBonus_Mid)
{
    // pt in (30,50]: level=100 int=35 -> 70? no; choose level=60 int=30 -> 36
    ch->set("level", (long)60);
    ch->set("int", (long)30);
    // pt = 60*30/50 = 36 -> 30 + (36-30)/2 = 33
    EXPECT_EQ(ch->learn_bonus(), 33);
}

TEST_F(CharAttrTest, LearnBonus_High)
{
    // pt in (50,80]: level=100 int=40 -> 80 -> 45 + (80-50)/3 = 55
    ch->set("level", (long)100);
    ch->set("int", (long)40);
    EXPECT_EQ(ch->learn_bonus(), 55);
}

TEST_F(CharAttrTest, LearnBonus_VeryHigh)
{
    // pt > 80: level=100 int=60 -> 120 -> 55 + (120-80)/5 = 63
    ch->set("level", (long)100);
    ch->set("int", (long)60);
    EXPECT_EQ(ch->learn_bonus(), 63);
}

// ---------------- is_invalid（前导 index.is_invalid，含超限播报副作用）
// ----------------
TEST_F(CharAttrTest, IsInvalid_False)
{
    ch->set("mp", (long)1000);
    lua_State* L = ensure_lua_env();
    lua_bind_push_object(L, ch);
    int k = call_method(L, "is_invalid", 1);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);
}

TEST_F(CharAttrTest, IsInvalid_True)
{
    lua_State* L = ensure_lua_env();
    ch->set("mp", (long)70000);
    lua_bind_push_object(L, ch);
    int k = call_method(L, "is_invalid", 1);
    ASSERT_EQ(1, k);
    EXPECT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, k);
}

// 钩子表 {limit=N} 覆盖默认上限
TEST_F(CharAttrTest, IsInvalid_HookLimitOverrides)
{
    lua_State* L = ensure_lua_env();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_char_rule = hooks.char_rule;"
                            "hooks.char_rule = function(phase, actor, target) "
                            "return {limit = 42} end"),
              LUA_OK);

    ch->set("mp", (long)41);
    lua_bind_push_object(L, ch);
    int k = call_method(L, "is_invalid", 1);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1)) << "41 ≤ 42 未超限";
    lua_pop(L, k);

    ch->set("mp", (long)43);
    lua_bind_push_object(L, ch);
    k = call_method(L, "is_invalid", 1);
    ASSERT_EQ(1, k);
    EXPECT_TRUE(lua_toboolean(L, -1)) << "钩子表值 42 覆盖默认 60000";
    lua_pop(L, k);

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);
}

// 钩子缺席时回退到引擎默认基准：门派豁免不再生效
TEST_F(CharAttrTest, IsInvalid_FallbackWithoutHook)
{
    lua_State* L = ensure_lua_env();
    ASSERT_EQ(
        luaL_dostring(
            L, "__orig_char_rule = hooks.char_rule; hooks.char_rule = nil"),
        LUA_OK);

    ch->set("family/family_name", "星宿派");
    ch->set("mp", (long)70000);  // 缺席 → 基准 60000
    lua_bind_push_object(L, ch);
    int k = call_method(L, "is_invalid", 1);
    ASSERT_EQ(1, k);
    EXPECT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, k);

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);
}

// ---------------- add_balance ----------------
TEST_F(CharAttrTest, AddBalance_Positive)
{
    ch->set("balance", (long)100);
    ch->add_balance(50);
    EXPECT_EQ(ch->query("balance"), 150);
}

TEST_F(CharAttrTest, AddBalance_Negative)
{
    ch->set("balance", (long)100);
    ch->add_balance(-30);
    EXPECT_EQ(ch->query("balance"), 70);
}

TEST_F(CharAttrTest, AddBalance_OverflowTakesMoneyPath)
{
    // balance near the value-domain cap so the overflow branch (Pay_Player)
    // runs
    ch->set("balance", (mapping_value_t)(MAPPING_VALUE_MAX - 10));
    ch->add_balance(1000);
    // Pay_Player loads "coin" via mock item mgr (returns null) -> no crash
    EXPECT_GE(ch->query("balance"), 0);
}
