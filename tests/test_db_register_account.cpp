// test_db_register_account.cpp — SqlAccount.cpp: db_register_account 与
// register_char Lua 绑定（rk 房注册专员的落库通道）
//
// g_db 指向 MockDatabase，通过 SQL 字符串断言业务逻辑：
//   - 校验：四维 10~30 且总和 84、名字合法且不重复、密码 4~22、性别男/女
//   - 落库：insert into jianghudb 一行基本资料（档案由首次登录新人分支建立）
//   - id：select max(cc_id)+1；空表回落 1000（与建表 AUTO_INCREMENT 对齐）
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>

#include "IDatabase.h"
#include "Sql.h"
#include "lua/LuaBindDecls.h"
#include "mock/mock_database.h"
#include "test_lua_env.h"

extern IDatabase* g_db;

namespace {

class DbRegisterAccountTest : public ::testing::Test {
protected:
    MockDatabase* db = nullptr;

    void SetUp() override
    {
        db   = new MockDatabase();
        g_db = db;
        // 预置 max(cc_id)=41 → 新账号应为 42（名字查重默认空结果）
        db->SetQueryResultMatching("select max(cc_id) from jianghudb",
                                   MockDatabase::MakeSingleRow({"41"}));
    }

    void TearDown() override
    {
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    long Register(const char* name, const char* passwd, const char* gender,
                  int str, int intel, int con, int dex, char* err, int errlen)
    {
        return db_register_account(name, passwd, gender, str, intel, con, dex,
                                   err, errlen);
    }
};

// ---------------- 成功路径 ----------------
TEST_F(DbRegisterAccountTest, Success_InsertsRowAndReturnsNextId)
{
    char err[256] = "x";
    long id       = Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                             sizeof(err));

    EXPECT_EQ(42, id) << "max(cc_id)+1";
    EXPECT_STREQ("", err);
    EXPECT_TRUE(db->ExecutedContaining("insert into jianghudb"));
    EXPECT_TRUE(db->ExecutedContaining("values (42, ")) << "显式 cc_id";
    EXPECT_TRUE(db->ExecutedContaining("'男性'")) << "性别入库";
    EXPECT_TRUE(db->ExecutedContaining("'abc12345'")) << "密码入库";
    EXPECT_TRUE(db->ExecutedContaining("21, 21, 21, 21")) << "四维入库";
}

TEST_F(DbRegisterAccountTest, Success_NameIsQueriedForDuplicates)
{
    char err[256];
    Register("李四", "pass1234", "女性", 20, 22, 21, 21, err, sizeof(err));
    EXPECT_TRUE(
        db->ExecutedContaining("select cc_id from jianghudb where "
                               "cc_name='李四'"))
        << "落库前先查重";
}

TEST_F(DbRegisterAccountTest, Success_EmptyTable_FallsBackTo1000)
{
    // 无 max(cc_id) 行（空表）→ 回落 1000，与建表 AUTO_INCREMENT 对齐
    db->Clear();
    char err[256];
    long id = Register("王五", "pass1234", "男性", 15, 20, 24, 25, err,
                       sizeof(err));
    EXPECT_EQ(1000, id);
    EXPECT_TRUE(db->ExecutedContaining("values (1000, "));
}

// ---------------- 校验失败路径 ----------------
TEST_F(DbRegisterAccountTest, StatSumNot84_Fails)
{
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 21, 21, 21, 20, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "84")) << "错误信息提示总和须为 84";
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
}

TEST_F(DbRegisterAccountTest, StatOutOfRange_Fails)
{
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 31, 21, 21, 11, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "10")) << "错误信息提示区间";
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
}

TEST_F(DbRegisterAccountTest, IllegalName_Fails)
{
    char err[256] = "";
    // 单字名：check_legal_name 要求 2~12 字节
    EXPECT_EQ(
        0, Register("x", "abc12345", "男性", 21, 21, 21, 21, err, sizeof(err)));
    EXPECT_STREQ("对不起，你的中文名字必须是 1 到 6 个中文字。", err);
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
}

TEST_F(DbRegisterAccountTest, DuplicateName_Fails)
{
    DbResultSet dup = MockDatabase::MakeSingleRow({"7"});
    db->SetQueryResultMatching("select cc_id from jianghudb where cc_name=",
                               dup);

    char err[256] = "";
    EXPECT_EQ(0, Register("赵六", "abc12345", "男性", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "已经有人用")) << "重名提示";
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
}

TEST_F(DbRegisterAccountTest, PasswordTooShort_Fails)
{
    char err[256] = "";
    EXPECT_EQ(
        0, Register("张三", "abc", "男性", 21, 21, 21, 21, err, sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "密码"));
}

TEST_F(DbRegisterAccountTest, PasswordTooLong_Fails)
{
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "0123456789012345678901234", "男性", 21, 21,
                          21, 21, err, sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "密码"));
}

TEST_F(DbRegisterAccountTest, PasswordQuote_IsSanitizedLikeLogin)
{
    char err[256] = "";
    long id
        = Register("张三", "ab'cd12", "男性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(42, id);
    EXPECT_TRUE(db->ExecutedContaining("'ab cd12'"))
        << "单引号替换为空格，与登录侧剔除逻辑一致";
}

TEST_F(DbRegisterAccountTest, IllegalGender_Fails)
{
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "人妖", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "性别"));
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
}

// ---------------- DB 故障路径 ----------------
TEST_F(DbRegisterAccountTest, InsertFails_ReturnsZero)
{
    db->SetExecuteFailAt(0);  // insert 是第一条 execute
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "注册失败"));
}

TEST_F(DbRegisterAccountTest, NameQueryFails_ReturnsZero)
{
    db->SetQueryErrorCode(1);  // 所有 query 失败
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "注册失败"));
}

TEST_F(DbRegisterAccountTest, NullDb_Fails)
{
    g_db          = nullptr;
    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "数据库"));
    g_db = db;  // 恢复，交给 TearDown
}

TEST_F(DbRegisterAccountTest, ThrowingDb_DoesNotEscape)
{
    ThrowingDatabase thrower;
    thrower.throwOnQuery = true;
    g_db                 = &thrower;

    char err[256] = "";
    EXPECT_EQ(0, Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                          sizeof(err)));
    EXPECT_NE(nullptr, strstr(err, "注册失败"));
    g_db = db;
}

// ---------------- Lua 绑定 ----------------
TEST_F(DbRegisterAccountTest, LuaBinding_PushesIdAndError)
{
    lua_State* L = ensure_lua_env();
    ASSERT_NE(nullptr, L);

    char err[256] = "";
    ASSERT_EQ(42, Register("张三", "abc12345", "男性", 21, 21, 21, 21, err,
                           sizeof(err)));

    int top = lua_gettop(L);
    lua_getglobal(L, "register_char");
    ASSERT_TRUE(lua_isfunction(L, -1)) << "register_char 已注册为全局函数";
    lua_pushstring(L, "孙七");
    lua_pushstring(L, "pass9999");
    lua_pushstring(L, "女性");
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 21);
    ASSERT_EQ(LUA_OK, lua_pcall(L, 7, 2, 0));
    EXPECT_EQ(42, lua_tointeger(L, -2));
    EXPECT_STREQ("", lua_tostring(L, -1));
    lua_settop(L, top);
}

TEST_F(DbRegisterAccountTest, LuaBinding_FailurePushesZeroAndMessage)
{
    lua_State* L = ensure_lua_env();
    ASSERT_NE(nullptr, L);

    int top = lua_gettop(L);
    lua_getglobal(L, "register_char");
    ASSERT_TRUE(lua_isfunction(L, -1));
    lua_pushstring(L, "孙七");
    lua_pushstring(L, "pass9999");
    lua_pushstring(L, "女性");
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 21);
    lua_pushinteger(L, 20);  // 总和 83 非法
    ASSERT_EQ(LUA_OK, lua_pcall(L, 7, 2, 0));
    EXPECT_EQ(0, lua_tointeger(L, -2));
    EXPECT_STREQ("天赋属性非法：四项总和必须为 84。", lua_tostring(L, -1));
    lua_settop(L, top);
}

}  // namespace
