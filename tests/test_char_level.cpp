// test_char_level.cpp - Unit tests for CharLevel.cpp.
//
// Covers level<->skill<->exp conversion tables, death penalty/recover and
// Remove_Betrayer.  level_exp 数据来自 lua-default/data/level_exp.lua。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "test_lua_env.h"

class CharLevelTest : public ::testing::Test {
protected:
    CChar* ch;
    void SetUp() override
    {
        ch = new CChar();
        ch->set("name", "lvl");
        ch->set("id", "lvl1");

        // 重置缓存，确保从 Lua 重新加载
        ASSERT_NE(ensure_lua_env(), nullptr);
        CChar::reset_level_exp_cache();
    }
    void TearDown() override { delete ch; }
};

// ---------------- level2skill ----------------
TEST_F(CharLevelTest, Level2Skill_Low)
{
    EXPECT_EQ(CChar::level2skill(1), 8);
    EXPECT_EQ(CChar::level2skill(4), 20);
}
TEST_F(CharLevelTest, Level2Skill_Mid)
{
    EXPECT_EQ(CChar::level2skill(5), 24);
    EXPECT_EQ(CChar::level2skill(15), 54);
}
TEST_F(CharLevelTest, Level2Skill_High)
{
    EXPECT_EQ(CChar::level2skill(25), 84);
    EXPECT_EQ(CChar::level2skill(30), 101);
}

// ---------------- skill2level ----------------
TEST_F(CharLevelTest, Skill2Level_All)
{
    EXPECT_EQ(CChar::skill2level(20), 4);
    EXPECT_EQ(CChar::skill2level(54), 15);
    EXPECT_EQ(CChar::skill2level(105), 31);
}

// ---------------- Level2Exp ----------------
TEST_F(CharLevelTest, Level2Exp_Table)
{
    EXPECT_EQ(CChar::level2_exp(1), 0);
    EXPECT_EQ(CChar::level2_exp(2), 100);
    EXPECT_EQ(CChar::level2_exp(3), 350);
    EXPECT_EQ(CChar::level2_exp(0), 0);
    EXPECT_EQ(CChar::level2_exp(-5), 0);
}
TEST_F(CharLevelTest, Level2Exp_AboveTable)
{
    // lv >= 176 uses the linear extrapolation branch.
    int32_t v = CChar::level2_exp(176);
    EXPECT_NE(v, CChar::level2_exp(175));
    // Verify the extrapolation formula: 1917735009 + lv * 100000000
    EXPECT_EQ(v, (int32_t)(1917735009 + (int64_t)176 * 100000000));
}
TEST_F(CharLevelTest, Level2Exp_HighLevels)
{
    // Spot-check a few high levels to ensure full array coverage.
    EXPECT_EQ(CChar::level2_exp(50), 508800);
    EXPECT_EQ(CChar::level2_exp(100), 5147800);
    EXPECT_EQ(CChar::level2_exp(150), 18714900);
    EXPECT_EQ(CChar::level2_exp(175), 1917735009);
}

// ---------------- exp2level ----------------
TEST_F(CharLevelTest, Exp2Level)
{
    EXPECT_EQ(CChar::exp2level(0), 1);
    EXPECT_EQ(CChar::exp2level(100), 2);
    EXPECT_EQ(CChar::exp2level(200), 2);
    EXPECT_EQ(CChar::exp2level(350), 3);
}

// ---------------- skill_death_penalty ----------------
TEST_F(CharLevelTest, DeathPenalty_ReturnsOne)
{
    EXPECT_EQ(ch->skill_death_penalty(), 1);
}

// ---------------- skill_death_recover ----------------
TEST_F(CharLevelTest, DeathRecover_SpecificSkill)
{
    ch->set_skill("unarmed", 10);
    ch->set("death/skill_name", "unarmed");
    // learned empty for unarmed -> sets learned amount
    EXPECT_EQ(ch->skill_death_recover(), 1);
}

TEST_F(CharLevelTest, DeathRecover_SpecificSkill_WithLearned)
{
    ch->set_skill("unarmed", 10);
    ch->query_learned()->set("unarmed", (long)999);
    ch->set("death/skill_name", "unarmed");
    EXPECT_EQ(ch->skill_death_recover(), 1);
    EXPECT_EQ(ch->query_skill("unarmed", 1), 11);
}

// ---------------- 算法近似回退（Lua 不可用） ----------------
TEST_F(CharLevelTest, Level2Exp_FallbackNoOverflow)
{
    // 守卫：只让 require("data.level_exp") 失败，不动共享引擎，
    // 测试结束后恢复原始 require，缓存回 Lua 真值。
    struct RequireBreaker {
        bool broken = false;
        ~RequireBreaker()
        {
            if (!broken) return;
            lua_State* L = CLuaEngine::instance()->get_state();
            if (!L) return;
            lua_getfield(L, LUA_REGISTRYINDEX, "test_saved_require");
            lua_setglobal(L, "require");
            lua_pushnil(L);
            lua_setfield(L, LUA_REGISTRYINDEX, "test_saved_require");
        }
    } breaker;

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_NE(L, nullptr);
    lua_getglobal(L, "require");
    lua_setfield(L, LUA_REGISTRYINDEX, "test_saved_require");
    ASSERT_EQ(luaL_loadstring(L,
                              "return function() error('level_exp "
                              "forced unavailable') end"),
              LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);
    lua_setglobal(L, "require");
    breaker.broken = true;

    CChar::reset_level_exp_cache();

    // 旧指数公式在 lv=158 即溢出 int32（转换结果为负垃圾值）
    EXPECT_GE(CChar::level2_exp(158), 1000000);  // 真值 22215798，同量级
    int32_t v175 = CChar::level2_exp(175);
    EXPECT_GT(v175, 1917735009 / 2);            // 不低于真值一半
    EXPECT_LT((double)v175, 1917735009 * 1.5);  // 不高于真值 1.5 倍
    EXPECT_LT(CChar::level2_exp(158), v175);    // 随等级单调增
}

TEST_F(CharLevelTest, DeathRecover_All)
{
    ch->set_skill("unarmed", 10);
    ch->set_skill("dodge", 5);
    ch->set("death/skill_name", "all");
    EXPECT_EQ(ch->skill_death_recover(), 1);
}

// ---------------- Remove_Betrayer ----------------
TEST_F(CharLevelTest, RemoveBetrayer_LowersSkills)
{
    CChar* me = new CChar();
    me->set_skill("unarmed", 100);
    me->set_skill("dodge", 10);  // <= lv*20 -> deleted
    me->set("betrayer", (long)2);

    CChar::remove_betrayer(me);

    EXPECT_EQ(me->query("betrayer"), 0);
    EXPECT_EQ(me->query_skill("unarmed", 1), 60);  // 100 - 2*20
    EXPECT_EQ(me->query_skill("dodge", 1), 0);
    delete me;
}
