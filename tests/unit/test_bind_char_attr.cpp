// test_bind_char_attr.cpp — binds in CharAttr.cpp + CharLevel.cpp +
// CharLook.cpp
#include "test_bind_common.h"

class BindCharAttrTest : public BindTest {
protected:
    CChar ch;
};

// ---- CharAttr.cpp ----
TEST_F(BindCharAttrTest, QueryAttributes)
{
    ch.set("str", (long)20);
    ch.set("con", (long)15);
    ch.set("int", (long)12);
    ch.set("per", (long)18);

    lua_bind_push_object(L, &ch);
    EXPECT_EQ(20, CallInt(l_query_str, 1));
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(15, CallInt(l_query_con, 1));
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(12, CallInt(l_query_int, 1));
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(18, CallInt(l_query_per, 1));
}

TEST_F(BindCharAttrTest, QueryApply)
{
    ch.set_temp("apply/str", (long)5);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "str");
    EXPECT_EQ(5, CallInt(l_query_apply, 2));
}

TEST_F(BindCharAttrTest, AddBalance)
{
    lua_bind_push_object(L, &ch);
    lua_pushinteger(L, 100);
    CallVoid(l_add_balance, 2);
    EXPECT_EQ(100, ch.query("balance"));
}

TEST_F(BindCharAttrTest, QueryExp)
{
    ch.set("combat_exp", (long)50000);
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(50000, CallInt(l_query_exp, 1));
    lua_bind_push_object(L, &ch);
    EXPECT_GE(CallInt(l_query_valid_exp, 1), 0);
    lua_bind_push_object(L, &ch);
    EXPECT_GE(CallInt(l_query_combat_exp, 1), 0);
}

TEST_F(BindCharAttrTest, LearnBonusAndInvalid)
{
    lua_bind_push_object(L, &ch);
    CallInt(l_learn_bonus, 1);
    lua_bind_push_object(L, &ch);
    EXPECT_GE(CallInt(l_is_invalid, 1), 0);
}

TEST_F(BindCharAttrTest, RespectRudeSelf)
{
    // 称谓算法在 data/rank.lua（hooks.rank_rule）：绑定桥需要真实 Lua 环境。
    ASSERT_NE(ensure_lua_env(), nullptr);
    ch.set("name", "xiaoke");
    ch.set("gender", "男性");
    // 契约：query_respect 对已命名角色必返回非空称谓（Rank.cpp → rank_rule）
    std::string s;
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_query_respect, 1, s));
    EXPECT_FALSE(s.empty());
    // 桥接契约：绑定返回值与 Lua 直调 rank_rule 一致（男/age<20 → 小兄弟）
    EXPECT_EQ("小兄弟", s);
    // 其余称谓桥接必须返回字符串而非 nil
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_query_rude, 1, s));
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_query_self, 1, s));
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_query_self_rude, 1, s));
}

// ---- CharLevel.cpp (static conversions: no self, just an int arg) ----
TEST_F(BindCharAttrTest, SkillLevelExpConversions)
{
    lua_pushinteger(L, 1000);
    EXPECT_GE(CallInt(l_skill2level, 1), 0);

    lua_pushinteger(L, 10);
    EXPECT_GE(CallInt(l_level2skill, 1), 0);

    lua_pushinteger(L, 10);
    EXPECT_GE(CallInt(l_level2exp, 1), 0);

    lua_pushinteger(L, 100000);
    EXPECT_GE(CallInt(l_exp2level, 1), 0);
}

// ---- CharLook.cpp (static) ----
TEST_F(BindCharAttrTest, EffStatusMsg)
{
    lua_pushinteger(L, 50);
    std::string s;
    EXPECT_TRUE(CallStr(l_eff_status_msg, 1, s));
    // 契约：ratio 50 → 40~60 区间描述（CChar::eff_status_msg 分派）
    EXPECT_EQ("气息粗重，动作开始散乱，看来所受的伤著实不轻。", s);
}
