// test_char_rules.cpp - CharRules.cpp 的数据集规则取值契约
//
// 查询点 char_rule_yes 共用 hooks.char_rule：
//   - char_rule_yes：钩子返回 {yes=true} 则放行，默认 false。
// 缺席/nil 走默认支，已由 test_char_attr / test_cmd_standard_life 覆盖；
// 数值覆盖型（{limit=N}）支改由前导 index.is_invalid 承载，
// 见 test_char_attr 的 IsInvalid_* 三例。
//
//
#include "pch.h"

#include "Char.h"
#include "CharRules.h"
#include "Skill.h"
#include "test_lua_env.h"

class CharRulesTest : public ::testing::Test {
protected:
    CChar ch;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        ch.set_name("规则探针", "rules1");
        ch.set("id", "rules1");
    }
};

TEST_F(CharRulesTest, Yes_HookTableDrivesDecision)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(luaL_dostring(L,
                            "__orig_char_rule = hooks.char_rule;"
                            "hooks.char_rule = function(phase, actor, target) "
                            "return {yes = true} end"),
              LUA_OK);

    EXPECT_TRUE(char_rule_yes("sleep_anywhere", &ch, nullptr))
        << "钩子表值放行(28-30)";

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);
}

// cast_force：target 为被施法的技能对象；钩子返回 {yes=true} 即额外放行。
TEST_F(CharRulesTest, Yes_CastForcePhaseDrivesGate)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(
        luaL_dostring(L,
                      "__orig_char_rule = hooks.char_rule;"
                      "hooks.char_rule = function(phase, actor, target) "
                      "if phase == \"cast_force\" then return {yes = true} end "
                      "return nil end"),
        LUA_OK);

    CSkill probe;
    EXPECT_TRUE(char_rule_yes("cast_force", &ch, &probe))
        << "cast_force 命中时放行，且 target 可为技能对象";

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);
}
