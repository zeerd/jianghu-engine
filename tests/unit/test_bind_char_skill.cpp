// test_bind_char_skill.cpp — binds in char/CharSkill.cpp
#include "test_bind_common.h"

#include "Skill.h"

// 继承 BindEngineTest：set_skill → load_skill 需要 Lua 环境，否则技能以未挂
// 脚本的形态留在 g_skill_list，毒化后续按名取技能的套件
class BindCharSkillTest : public BindEngineTest {
protected:
    CChar ch;
};

TEST_F(BindCharSkillTest, SetQuerySkill)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    lua_pushinteger(L, 50);
    int k = call(l_set_skill, 3);
    ASSERT_EQ(1, k) << "set_skill 返回成败";
    EXPECT_TRUE(lua_toboolean(L, -1)) << "技能设置成功";
    lua_pop(L, k);
    EXPECT_EQ(50, ch.query_skill("unarmed", 1));

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    lua_pushinteger(L, 1);  // raw -> skills.query
    EXPECT_EQ(50, CallInt(l_query_skill, 3));
}

TEST_F(BindCharSkillTest, MiniSkill)
{
    // 契约：最低武功等级（前导 index.mini_skill 组合 query_skills）；
    // 无技能回 0（吸收原 CharSkillTest.MiniSkill_None）
    lua_bind_push_object(L, &ch);
    int k = call_method(L, "mini_skill", 1);
    ASSERT_EQ(1, k);
    EXPECT_EQ(0, lua_tointeger(L, -1));
    lua_pop(L, k);

    ch.set_skill("unarmed", 50);
    ch.set_skill("dodge", 10);
    ch.set_skill("force", 30);
    lua_bind_push_object(L, &ch);
    k = call_method(L, "mini_skill", 1);
    ASSERT_EQ(1, k);
    EXPECT_EQ(10, lua_tointeger(L, -1)) << "多技能取最小";
    lua_pop(L, k);
}

TEST_F(BindCharSkillTest, ClearCondition)
{
    // 契约：前导 index.clear_condition 清持久 + 临时 condition 整键
    ch.apply_condition("poison", 5, 10);
    ASSERT_GT(ch.query_condition("poison"), 0);

    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, call_method(L, "clear_condition", 1));
    EXPECT_EQ(ch.query_condition("poison"), 0);
}

TEST_F(BindCharSkillTest, DeleteSkill)
{
    ch.set_skill("dodge", 20);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "dodge");
    int k = call(l_delete_skill, 2);
    ASSERT_EQ(1, k) << "delete_skill 返回成败";
    EXPECT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, k);
    EXPECT_EQ(0, ch.query_skill("dodge", 1));

    // 空技能名 -> false
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "");
    k = call(l_delete_skill, 2);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);
}

TEST_F(BindCharSkillTest, ApplyCondition)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "poison");
    lua_pushinteger(L, 5);
    int k = call(l_apply_condition, 3);
    ASSERT_EQ(1, k) << "apply_condition 返回成败";
    EXPECT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, k);
    EXPECT_GE(ch.query_condition("poison"), 0);

    // 毒盾拒绝 -> false
    ch.set_temp("apply/poi_shield", (int32_t)1);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "poison");
    lua_pushinteger(L, 5);
    k = call(l_apply_condition, 3);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);
    ch.delete_temp("apply/poi_shield");
}

// ---- 守卫路径：无效 self 也必须返回一个 false ----
TEST_F(BindCharSkillTest, GuardsReturnFalse)
{
    // 对象但非 CChar -> dynamic_cast 失败
    CContainer box;
    box.set_name("box", "skill_guard_box");

    lua_bind_push_object(L, &box);
    lua_pushstring(L, "unarmed");
    lua_pushinteger(L, 1);
    int k = call(l_set_skill, 3);
    ASSERT_EQ(1, k) << "set_skill 守卫路径也压值";
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);

    lua_bind_push_object(L, &box);
    lua_pushstring(L, "dodge");
    k = call(l_delete_skill, 2);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);

    lua_bind_push_object(L, &box);
    lua_pushstring(L, "poison");
    lua_pushinteger(L, 5);
    k = call(l_apply_condition, 3);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, k);
}

TEST_F(BindCharSkillTest, QuerySkillsTable)
{
    ch.set_skill("force", 10);
    lua_bind_push_object(L, &ch);
    int k = l_query_skills(L);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_istable(L, -1));
    lua_pop(L, k + 1);
}

TEST_F(BindCharSkillTest, MapAndQueryMapped)
{
    ch.set_skill("unarmed", 10);
    ch.set_skill("parry", 5);  // target must be learned
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "set");
    lua_pushstring(L, "unarmed");
    lua_pushstring(L, "parry");
    CallVoid(l_skill_map, 4);

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "query");
    lua_pushstring(L, "unarmed");
    std::string s;
    CallStr(l_skill_map, 3, s);
    EXPECT_EQ("parry", s);

    // clear 后映射清空（与上面的写入成对，clear 才有契约意义）
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "clear");
    EXPECT_EQ(0, CallRetCount(l_skill_map, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "query");
    lua_pushstring(L, "unarmed");
    s = "keep";
    CallStr(l_skill_map, 3, s);
    EXPECT_TRUE(s.empty());
}

TEST_F(BindCharSkillTest, ValidForce)
{
    // load_skill 需要引擎已起（valid_force 失败分支会 load_skill 取内功名）
    ASSERT_NE(ensure_lua_env(), nullptr);

    CSkill skill;
    skill.set("valid_force", "zixia_shengong");

    lua_bind_push_object(L, &skill);
    lua_pushstring(L, "zixia_shengong");
    EXPECT_EQ(1, CallInt(l_valid_force, 2));

    lua_bind_push_object(L, &skill);
    lua_pushstring(L, "beiming_shengong");
    EXPECT_EQ(0, CallInt(l_valid_force, 2));
}

TEST_F(BindCharSkillTest, MapListAndPrepareList)
{
    ch.map_skill("unarmed", "parry");
    ch.prepare_skill("blade", "sword");
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "list");
    EXPECT_GE(CallRetCount(l_skill_map, 2), 1);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "list");
    EXPECT_GE(CallRetCount(l_skill_prepare, 2), 1);

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "query");
    lua_pushstring(L, "blade");
    std::string s;
    CallStr(l_skill_prepare, 3, s);
}

TEST_F(BindCharSkillTest, IncAndImproveSkill)
{
    ch.set_skill("claw", 10);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "claw");
    lua_pushinteger(L, 5);
    CallVoid(l_inc_skill, 3);
    EXPECT_EQ(15, ch.query_skill("claw", 1));

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "claw");
    lua_pushinteger(L, 100);
    EXPECT_EQ(0, CallRetCount(l_improve_skill, 3));
    // 契约：练习点数入账（100 < (10+1)^2 不升级，等级保持 15）
    EXPECT_EQ(100, ch.query_learned()->query("claw"));
    EXPECT_EQ(15, ch.query_skill("claw", 1));
}

TEST_F(BindCharSkillTest, PrepareSkillAndClears)
{
    ch.set_skill("cuff", 10);
    ch.set_skill("strike", 10);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "set");
    lua_pushstring(L, "cuff");
    lua_pushstring(L, "strike");
    EXPECT_EQ(0, CallRetCount(l_skill_prepare, 4));

    // 契约：prepare 写入可读回
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "query");
    lua_pushstring(L, "cuff");
    std::string s;
    EXPECT_TRUE(CallStr(l_skill_prepare, 3, s));
    EXPECT_EQ("strike", s);

    // clear 后读回空串
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "clear");
    EXPECT_EQ(0, CallRetCount(l_skill_prepare, 2));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "query");
    lua_pushstring(L, "cuff");
    s = "keep";
    CallStr(l_skill_prepare, 3, s);
    EXPECT_TRUE(s.empty());

    // clear_skill 清空已学技能表
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_clear_skill, 1));
    EXPECT_EQ(0, ch.query_skills()->get_count());
}

// 未登记 ACTION 是脚本写错，与 dbase/load 的未知 action 同律报错
TEST_F(BindCharSkillTest, UnknownActionErrors)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "nope");
    EXPECT_EQ(-1, call(l_skill_map, 2));

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "nope");
    EXPECT_EQ(-1, call(l_skill_prepare, 2));
}

TEST_F(BindCharSkillTest, SkillNeedPoints)
{
    // 契约：无该技能时 (0+1)^2 - 0 = 1
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    EXPECT_EQ(1, CallInt(l_skill_need_points, 2));

    ch.set_skill("unarmed", 40);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    // 契约：已学 40 级时 (40+1)^2 - 0 = 1681
    EXPECT_EQ(1681, CallInt(l_skill_need_points, 2));
}
