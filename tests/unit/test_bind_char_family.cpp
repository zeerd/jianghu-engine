// test_bind_char_family.cpp — binds in char/CharFamily.cpp (门派/师徒/招募).
#include "test_bind_common.h"
#include "mock/mock_database.h"  // SetQueryResult/SetShouldFail（recruit 深路）

#include "test_char_common.h"  // CharMockDb

class BindCharFamilyTest : public BindTest {
protected:
    CChar master;
    CChar appr;
    void SetUp() override
    {
        BindTest::SetUp();
        master.set_name("m", "m1");
        master.set("id", "m1");
        appr.set_name("a", "a1");
        appr.set("id", "a1");
    }
};

TEST_F(BindCharFamilyTest, CreateFamilyAndQuery)
{
    lua_bind_push_object(L, &master);
    lua_pushstring(L, "emei");
    lua_pushinteger(L, 1);
    lua_pushstring(L, "弟子");
    CallVoid(l_create_family, 4);
    EXPECT_STREQ("emei", master.querystr("family/family_name"));
}

TEST_F(BindCharFamilyTest, SetLeader)
{
    lua_bind_push_object(L, &master);
    lua_bind_push_object(L, &appr);
    EXPECT_EQ(0, CallRetCount(l_set_leader, 2));
    EXPECT_EQ(&appr, master.query_leader());  // m_leader 赋值可观察
}

TEST_F(BindCharFamilyTest, AssignApprentice)
{
    master.create_family("emei", 1, "弟子");
    lua_bind_push_object(L, &master);
    lua_pushstring(L, "弟子");
    lua_pushinteger(L, 0);
    EXPECT_EQ(0, CallRetCount(l_assign_apprentice, 3));
    // 契约：assign_apprentice 覆盖 title/privs（create_family 预置 privs=-1）
    CMapping* family = master.querymap("family");
    ASSERT_NE(family, nullptr);
    EXPECT_STREQ("弟子", family->querystr("title"));
    EXPECT_EQ(0, family->query("privs"));
}

TEST_F(BindCharFamilyTest, RecruitNum)
{
    lua_bind_push_object(L, &master);
    EXPECT_GE(CallInt(l_recruit_num, 1), 0);
}

TEST_F(BindCharFamilyTest, IsApprenticeOf)
{
    lua_bind_push_object(L, &appr);
    lua_bind_push_object(L, &master);
    EXPECT_EQ(0, CallInt(l_is_apprentice_of, 2));
}

TEST_F(BindCharFamilyTest, RecruitApprentice)
{
    master.create_family("emei", 1, "弟子");
    lua_bind_push_object(L, &master);
    lua_bind_push_object(L, &appr);
    EXPECT_EQ(1, CallInt(l_recruit_apprentice, 2));
    // 契约：收徒建立师徒链（is_apprentice_of 全链成立）
    EXPECT_EQ(1, appr.is_apprentice_of(&master));
    EXPECT_STREQ("m1", appr.querystr("family/master_id"));
    EXPECT_STREQ("emei", appr.querystr("family/family_name"));
}

TEST_F(BindCharFamilyTest, RecognizeApprentice)
{
    // 契约：绑定必须 push 整数结果（当前实现恒 0）
    lua_bind_push_object(L, &master);
    lua_bind_push_object(L, &appr);
    int k = call(l_recognize_apprentice, 2);
    ASSERT_EQ(1, k);
    EXPECT_EQ(0, lua_tointeger(L, -1));
    lua_pop(L, k);
}

TEST_F(BindCharFamilyTest, PreventLearn)
{
    // 契约：非嫡传弟子学本门功夫被拒 → 返回 1
    lua_bind_push_object(L, &master);
    lua_bind_push_object(L, &appr);
    lua_pushstring(L, "sword");
    int k = call(l_prevent_learn, 3);
    ASSERT_EQ(1, k);
    EXPECT_EQ(1, lua_tointeger(L, -1));
    lua_pop(L, k);
}

TEST_F(BindCharFamilyTest, AttemptApprentice)
{
    // 空实现（CharFamily.cpp）：void 转发、pcall 不抛
    lua_bind_push_object(L, &master);
    lua_bind_push_object(L, &appr);
    EXPECT_EQ(0, CallRetCount(l_attempt_apprentice, 2));
}

TEST_F(BindCharFamilyTest, ReleaseMaster)
{
    // 无 family/user 的玩家师父记录 → guard 早退（void 无返回）
    lua_bind_push_object(L, &appr);
    EXPECT_EQ(0, CallRetCount(l_release_master, 1));
}

// recruit board lives in g_Channel; query_recruit needs a live g_db.
TEST_F(BindCharFamilyTest, QueryRecruitMissing)
{
    CharMockDb db;
    g_db = &db;  // empty result -> returns &recruit_exp (empty) -> table
    lua_pushstring(L, "no_such_master_zzz");
    int k = call(l_query_recruit, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_istable(L, -1));
    lua_pop(L, k);
    g_db = nullptr;
}

TEST_F(BindCharFamilyTest, UpdateThenQueryRecruit)
{
    CharMockDb db;
    g_db = &db;
    lua_pushstring(L, "bindtest_master");
    lua_pushstring(L, "stu1");
    lua_pushstring(L, "student");
    lua_pushinteger(L, 100);
    lua_pushinteger(L, 0);
    CallVoid(l_update_recruit, 5);

    lua_pushstring(L, "bindtest_master");
    int k = call(l_query_recruit, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_istable(L, -1));
    lua_pop(L, k);
    g_db = nullptr;
}

// ---- CharFamily.cpp: l_query_recruit 失败 nil支(355-356) + 行循环(362-375)
// ----
TEST_F(BindCharFamilyTest, QueryRecruitNilFailAndRowIteration)
{
    MockDatabase mdb;
    g_db = &mdb;

    // 1) db 查询失败 → tracker 返回 nullptr → pushnil(355-356)
    mdb.SetShouldFail(true);
    lua_pushstring(L, "zzz_fail_master");
    int k = call(l_query_recruit, 1);
    ASSERT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1)) << "查询失败 → nil(355-356)";
    lua_pop(L, k);
    mdb.SetShouldFail(false);

    // 2) 命中一行 → keys 循环建表(362-375)
    DbResultSet rs;
    DbRow row;
    row.columns.push_back("ap1");     // re_id
    row.columns.push_back("学徒甲");  // re_name
    row.columns.push_back("100");     // re_exp
    row.columns.push_back("1");       // re_apprentice
    rs.rows.push_back(row);
    mdb.SetQueryResult(
        "select re_id, re_name, re_exp, re_apprentice from "
        "recruit_exp where re_master=mstr1",
        rs);

    lua_pushstring(L, "mstr1");
    k = call(l_query_recruit, 1);
    ASSERT_EQ(1, k);
    ASSERT_TRUE(lua_istable(L, -1)) << "命中 → 表";

    // 深断言：表["ap1"]["name"] 可读 → 循环体完整执行
    lua_getfield(L, -1, "ap1");
    ASSERT_TRUE(lua_istable(L, -1)) << "条目子表已建(366)";
    lua_getfield(L, -1, "name");
    EXPECT_STREQ("学徒甲", lua_tostring(L, -1)) << "name 字段透传(368-369)";
    lua_pop(L, 2);  // name + 子表
    lua_pop(L, 1);  // 主表

    g_db = nullptr;
}
