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

    // 契约：attr(name) 分派到 CChar 同名查询（含 apply/ 修正与技能加成）
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "str");
    EXPECT_EQ(20, CallInt(l_attr, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "con");
    EXPECT_EQ(15, CallInt(l_attr, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "int");
    EXPECT_EQ(12, CallInt(l_attr, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "per");
    EXPECT_EQ(18, CallInt(l_attr, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "dex");
    EXPECT_EQ(0, CallInt(l_attr, 2));

    // 未登记的属性名报错（call() 捕获为 -1）
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "mp");
    EXPECT_EQ(-1, call(l_attr, 2));
    // 缺 name 参数同样报错
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(-1, call(l_attr, 1));
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
    // 契约：combat_exp 走 attr 分派；历练与可用经验同源
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "combat_exp");
    EXPECT_EQ(50000, CallInt(l_attr, 2));
    // 契约：可用经验对普通 CChar 同样可读（玩家侧升级/学技能的扣费依据），
    // level/sk_exp 未设时 = 经验总额
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(50000, CallInt(l_query_valid_exp, 1));
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
    // 契约：respect("respect", ob) 对已命名角色必返回非空称谓（Rank.cpp →
    // rank_rule）
    std::string s;
    lua_pushstring(L, "respect");
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_respect, 2, s));
    EXPECT_FALSE(s.empty());
    // 桥接契约：绑定返回值与 Lua 直调 rank_rule 一致（男/age<20 → 小兄弟）
    EXPECT_EQ("小兄弟", s);
    // 其余称谓必须返回字符串而非 nil
    lua_pushstring(L, "rude");
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_respect, 2, s));
    lua_pushstring(L, "self");
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_respect, 2, s));
    lua_pushstring(L, "self_rude");
    lua_bind_push_object(L, &ch);
    EXPECT_TRUE(CallStr(l_respect, 2, s));
    // 非角色入参返回空串
    lua_pushstring(L, "respect");
    lua_pushinteger(L, 1);
    EXPECT_TRUE(CallStr(l_respect, 2, s));
    EXPECT_TRUE(s.empty());
    // 未登记 kind / 缺 kind 报错（call() 捕获为 -1）
    lua_pushstring(L, "bogus");
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(-1, call(l_respect, 2));
    EXPECT_EQ(-1, call(l_respect, 0));
}

// ---- CharLevel.cpp：等级换算由内容层前导实现（引擎状态含前导定义） ----
TEST_F(BindCharAttrTest, SkillLevelExpConversions)
{
    ASSERT_NE(ensure_lua_env(), nullptr);

    lua_State* E = CLuaEngine::instance()->get_state();

    auto lua_int = [&E](const char* expr) -> int64_t {
        const std::string code = std::string("return ") + expr;

        if (luaL_dostring(E, code.c_str()) != LUA_OK) {
            ADD_FAILURE() << expr << ": " << lua_tostring(E, -1);

            lua_pop(E, 1);

            return 0;
        }

        int64_t v = (int64_t)lua_tointeger(E, -1);

        lua_pop(E, 1);

        return v;
    };

    // 契约：前导实现与引擎内部函数同式同源（同一张 data.level_exp 表）
    EXPECT_EQ(CChar::skill2level(1000), lua_int("skill2level(1000)"));

    EXPECT_EQ(CChar::level2skill(10), lua_int("level2skill(10)"));

    EXPECT_EQ(CChar::level2_exp(3), lua_int("Level2Exp(3)"));

    EXPECT_EQ(CChar::level2_exp(100), lua_int("Level2Exp(100)"));

    EXPECT_EQ(CChar::level2_exp(175), lua_int("Level2Exp(175)"));

    EXPECT_EQ(CChar::exp2level(100), lua_int("exp2level(100)"));

    EXPECT_EQ(CChar::exp2level(350), lua_int("exp2level(350)"));

    EXPECT_EQ(0, lua_int("Level2Exp(0)"));

    // 176 级起外推：每级 +1 亿（表覆盖到 175，公式语义不落 int32 截断）
    EXPECT_EQ(1917735009LL + 176 * 100000000LL, lua_int("Level2Exp(176)"));
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
