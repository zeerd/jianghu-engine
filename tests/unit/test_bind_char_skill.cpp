// test_bind_char_skill.cpp — binds in char/CharSkill.cpp
#include "test_bind_common.h"

#include "Skill.h"

class BindCharSkillTest : public BindTest {
protected:
    CChar ch;
};

TEST_F(BindCharSkillTest, SetQuerySkill)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    lua_pushinteger(L, 50);
    CallVoid(l_set_skill, 3);
    EXPECT_EQ(50, ch.query_skill("unarmed", 1));

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    lua_pushinteger(L, 1);  // raw -> skills.query
    EXPECT_EQ(50, CallInt(l_query_skill, 3));
}

TEST_F(BindCharSkillTest, MiniSkill)
{
    ch.set_skill("unarmed", 30);
    lua_bind_push_object(L, &ch);
    // 契约：最低武功等级（单技能时即该技能）
    EXPECT_EQ(30, CallInt(l_mini_skill, 1));
}

TEST_F(BindCharSkillTest, DeleteSkill)
{
    ch.set_skill("dodge", 20);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "dodge");
    CallVoid(l_delete_skill, 2);
    EXPECT_EQ(0, ch.query_skill("dodge", 1));
}

TEST_F(BindCharSkillTest, ApplyCondition)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "poison");
    lua_pushinteger(L, 5);
    CallVoid(l_apply_condition, 3);
    EXPECT_GE(ch.query_condition("poison"), 0);
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
    lua_pushstring(L, "unarmed");
    lua_pushstring(L, "parry");
    CallVoid(l_map_skill, 3);

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    std::string s;
    CallStr(l_query_skill_mapped, 2, s);
    EXPECT_EQ("parry", s);

    // clear_map 后映射清空（与上面的写入成对，clear 才有契约意义）
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_clear_skill_map, 1));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    s = "keep";
    CallStr(l_query_skill_mapped, 2, s);
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
    EXPECT_GE(CallRetCount(l_query_skill_map_list, 1), 1);
    lua_bind_push_object(L, &ch);
    EXPECT_GE(CallRetCount(l_query_skill_prepare_list, 1), 1);

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "blade");
    std::string s;
    CallStr(l_query_skill_prepared, 2, s);
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
    lua_pushstring(L, "cuff");
    lua_pushstring(L, "strike");
    EXPECT_EQ(0, CallRetCount(l_prepare_skill, 3));

    // 契约：prepare 写入可读回
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "cuff");
    std::string s;
    EXPECT_TRUE(CallStr(l_query_skill_prepared, 2, s));
    EXPECT_EQ("strike", s);

    // clear_prepare 后读回空串
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_clear_skill_prepare, 1));
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "cuff");
    s = "keep";
    CallStr(l_query_skill_prepared, 2, s);
    EXPECT_TRUE(s.empty());

    // clear_skill 清空已学技能表
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_clear_skill, 1));
    EXPECT_EQ(0, ch.query_skills()->get_count());
}

TEST_F(BindCharSkillTest, SkillNeedPointsAndHardcore)
{
    // 契约：无该技能时 (0+1)^2 - 0 = 1
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "unarmed");
    EXPECT_EQ(1, CallInt(l_skill_need_points, 2));

    ch.set_skill("unarmed", 40);
    lua_bind_push_object(L, &ch);
    int k = call(l_query_hardcore_skills, 1);
    // 契约：无 hard-core 标记的技能返回 nil（l_query_hardcore_skills 尾部）
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);
}
