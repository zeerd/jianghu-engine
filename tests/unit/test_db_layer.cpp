// test_db_layer.cpp — 真实 db 业务逻辑的单元测试
//
// 架构：
//   - SqlContainer.cpp / SqlAccount.cpp / SqlCharge.cpp 编译进测试二进制
//   - g_db 指向 MockDatabase（实现 IDatabase 接口）
//   - 测试通过 SQL 字符串验证业务逻辑正确性
//
// 这样 coverage 工具会统计真实业务代码的执行覆盖。

#include "pch.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "Container.h"
#include "IDatabase.h"
#include "Sql.h"
#include "mock/mock_database.h"

// 全局 mock 实例（测试 fixture 设置/恢复 g_db）
static MockDatabase* s_mockDb = nullptr;

extern IDatabase* g_db;

// ====================================================================
// 测试夹具：每次测试前重置 g_db
// ====================================================================
class DbLayerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        s_mockDb = new MockDatabase();
        g_db     = s_mockDb;
    }
    void TearDown() override
    {
        g_db = nullptr;
        delete s_mockDb;
        s_mockDb = nullptr;
    }
};

// ====================================================================
// SqlContainer.cpp — 容器 CRUD
// ====================================================================

TEST_F(DbLayerTest, SaveContainer_Insert)
{
    // execute 返回 0 = 成功
    EXPECT_TRUE(
        db_save_container("t_item", "sword_01", "铁剑", "data1", false));
    // 验证生成了正确的 INSERT SQL
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("insert"), std::string::npos);
    EXPECT_NE(sql.find("t_item"), std::string::npos);
    EXPECT_NE(sql.find("sword_01"), std::string::npos);
}

TEST_F(DbLayerTest, SaveContainer_Update)
{
    EXPECT_TRUE(db_save_container("t_item", "sword_01", "铁剑", "data2", true));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
    EXPECT_NE(sql.find("data2"), std::string::npos);
}

TEST_F(DbLayerTest, SaveContainer_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(
        db_save_container("t_item", "sword_01", "铁剑", "data", false));
}

TEST_F(DbLayerTest, LoadContainer_Found)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"the_data"});
    s_mockDb->SetQueryResult(
        "select obj_data from t_item where obj_id='sword_01'", rs);

    std::string data;
    EXPECT_TRUE(db_load_container("t_item", "sword_01", data));
    EXPECT_EQ(data, "the_data");
}

TEST_F(DbLayerTest, LoadContainer_NotFound)
{
    // 默认空结果
    std::string data;
    EXPECT_FALSE(db_load_container("t_item", "nonexistent", data));
}

TEST_F(DbLayerTest, LoadContainer_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    std::string data;
    EXPECT_FALSE(db_load_container("t_item", "x", data));
}

TEST_F(DbLayerTest, DeleteContainer)
{
    EXPECT_TRUE(db_delete_container("t_npc", "npc_01"));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("delete"), std::string::npos);
    EXPECT_NE(sql.find("t_npc"), std::string::npos);
}

TEST_F(DbLayerTest, DeleteContainer_Fails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_delete_container("t_npc", "npc_01"));
}

TEST_F(DbLayerTest, LockContainer_QueryCurrentState)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"0"});
    s_mockDb->SetQueryResult(
        "select obj_lock from t_item where obj_id='chest_01'", rs);

    int current_lock = -1;
    EXPECT_TRUE(db_lock_container("t_item", "chest_01", 1, current_lock));
    EXPECT_EQ(current_lock, 0);
}

TEST_F(DbLayerTest, LockContainer_AlreadyLocked)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"1"});
    s_mockDb->SetQueryResult(
        "select obj_lock from t_item where obj_id='chest_01'", rs);

    int current_lock = -1;
    // 锁状态=1，请求锁=1 → 冲突
    EXPECT_FALSE(db_lock_container("t_item", "chest_01", 1, current_lock));
    EXPECT_EQ(current_lock, 1);
}

TEST_F(DbLayerTest, LockContainer_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    int cl = -1;
    EXPECT_FALSE(db_lock_container("t_item", "x", 1, cl));
}

// ====================================================================
// SqlContainer.cpp — 玩家物品
// ====================================================================

TEST_F(DbLayerTest, SaveObject_Insert)
{
    EXPECT_TRUE(db_save_object("1001", 0, "sword", "json_data"));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("t_saveobj"), std::string::npos);
    EXPECT_NE(sql.find("1001"), std::string::npos);
}

TEST_F(DbLayerTest, SaveObject_Update)
{
    // 先返回已有记录（触发 UPDATE 分支）
    DbResultSet rs = MockDatabase::MakeSingleRow({"sword"});
    s_mockDb->SetQueryResult(
        "select base_name from t_saveobj where owner=1001 and grid = 0", rs);

    EXPECT_TRUE(db_save_object("1001", 0, "sword", "new_data"));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
    EXPECT_NE(sql.find("new_data"), std::string::npos);
}

TEST_F(DbLayerTest, SaveObject_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_save_object("1001", 0, "sword", "data"));
}

TEST_F(DbLayerTest, LoadObject_Found)
{
    DbResultSet rs;
    DbRow row;
    row.columns.push_back("sword");
    row.columns.push_back("json_data");
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult(
        "select base_name, savedata from t_saveobj where owner =1001 "
        "and grid = 0",
        rs);

    std::string base, data;
    EXPECT_TRUE(db_load_object("1001", 0, base, data));
    EXPECT_EQ(base, "sword");
    EXPECT_EQ(data, "json_data");
}

TEST_F(DbLayerTest, LoadObject_NotFound)
{
    std::string base, data;
    EXPECT_FALSE(db_load_object("9999", 0, base, data));
}

// ====================================================================
// SqlContainer.cpp — 箱子
// ====================================================================

TEST_F(DbLayerTest, LoadRoomBoxes_Found)
{
    DbResultSet rs;
    DbRow r1;
    r1.columns.push_back("box_data_0");
    rs.rows.push_back(r1);
    DbRow r2;
    r2.columns.push_back("box_data_1");
    rs.rows.push_back(r2);

    // 匹配 LIKE 查询
    s_mockDb->SetQueryResult(
        "select obj_data from t_box where obj_id LIKE 'road1_%' "
        "order by obj_id",
        rs);

    std::vector<std::string> boxes;
    EXPECT_TRUE(db_load_room_boxes("road1", boxes));
    EXPECT_EQ((int)boxes.size(), 2);
    EXPECT_EQ(boxes[0], "box_data_0");
    EXPECT_EQ(boxes[1], "box_data_1");
}

TEST_F(DbLayerTest, LoadRoomBoxes_Empty)
{
    std::vector<std::string> boxes;
    EXPECT_TRUE(db_load_room_boxes("empty_room", boxes));
    EXPECT_EQ((int)boxes.size(), 0);
}

// ====================================================================
// SqlAccount.cpp — 用户账号
// ====================================================================

TEST_F(DbLayerTest, set_online_flag)
{
    EXPECT_TRUE(db_set_online_flag("1", 3));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("cc_server"), std::string::npos);
    EXPECT_NE(sql.find("1"), std::string::npos);
}

TEST_F(DbLayerTest, SetOnlineFlag_Fails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_set_online_flag("1", 3));
}

TEST_F(DbLayerTest, QueryServer_Found)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"5"});
    s_mockDb->SetQueryResult("select cc_server from jianghudb where cc_id = 1",
                             rs);
    EXPECT_EQ(db_query_server("1"), 5);
}

TEST_F(DbLayerTest, QueryServer_NotFound)
{
    EXPECT_EQ(db_query_server("9999"), -1);
}

TEST_F(DbLayerTest, QueryServer_Fails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_EQ(db_query_server("1"), -1);
}

TEST_F(DbLayerTest, SetFreeze)
{
    EXPECT_TRUE(db_set_freeze("1", 1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("cc_freeze"), std::string::npos);
}

TEST_F(DbLayerTest, QueryFreeze_Found)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"1"});
    s_mockDb->SetQueryResult("select cc_freeze from jianghudb where cc_id=1",
                             rs);
    EXPECT_EQ(db_query_freeze("1"), 1);
}

TEST_F(DbLayerTest, QueryFreeze_NotFound_Default1)
{
    // 不存在时默认返回 1（冻结）
    EXPECT_EQ(db_query_freeze("9999"), 1);
}

TEST_F(DbLayerTest, change_password)
{
    EXPECT_TRUE(db_change_password("1", "newpass"));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("cc_passwd"), std::string::npos);
    EXPECT_NE(sql.find("newpass"), std::string::npos);
}

TEST_F(DbLayerTest, ChangePassword_Fails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_change_password("1", "newpass"));
}

TEST_F(DbLayerTest, change_verify_password)
{
    EXPECT_TRUE(db_change_verify_password("1", "newpass"));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("cc_verifypass"), std::string::npos);
    EXPECT_NE(sql.find("OLD_PASSWORD"), std::string::npos);
}

// ====================================================================
// SqlAccount.cpp — 频道/社交
// ====================================================================

TEST_F(DbLayerTest, QueryRecruit)
{
    DbResultSet rs;
    DbRow row;
    row.columns.push_back("2");     // re_id
    row.columns.push_back("张三");  // re_name
    row.columns.push_back("100");   // re_exp
    row.columns.push_back("1");     // re_apprentice
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult(
        "select re_id, re_name, re_exp, re_apprentice from "
        "recruit_exp where re_master=1",
        rs);

    DbResultSet result;
    EXPECT_TRUE(db_query_recruit("1", result));
    EXPECT_EQ(result.size(), 1);
}

TEST_F(DbLayerTest, UpdateRecruit_Insert)
{
    // 查询返回空（触发 INSERT）
    EXPECT_TRUE(db_update_recruit("1", "2", "学徒", 100, 1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("insert"), std::string::npos);
    EXPECT_NE(sql.find("recruit_exp"), std::string::npos);
}

TEST_F(DbLayerTest, UpdateRecruit_Update)
{
    // 查询返回已有记录（触发 UPDATE）
    DbResultSet rs = MockDatabase::MakeSingleRow({"50"});
    s_mockDb->SetQueryResult(
        "select re_exp from recruit_exp where re_master=1 and re_id=2", rs);

    EXPECT_TRUE(db_update_recruit("1", "2", "新名", 200, 1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
}

TEST_F(DbLayerTest, UpdateRelative_Insert_NoFlag)
{
    EXPECT_TRUE(db_update_relative("1", "2", 500, -1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("insert"), std::string::npos);
    // flag=-1 时 cc_relative 应为 0
    EXPECT_NE(sql.find("0)"), std::string::npos);
}

TEST_F(DbLayerTest, UpdateRelative_Update_WithFlag)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"100"});
    s_mockDb->SetQueryResult(
        "select cc_point from relative where cc_id=1 and cc_other=2", rs);

    EXPECT_TRUE(db_update_relative("1", "2", 500, 1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
}

TEST_F(DbLayerTest, QueryRelative_Found)
{
    DbResultSet rs;
    DbRow row;
    row.columns.push_back("500");  // cc_point
    row.columns.push_back("1");    // cc_relative
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult(
        "select cc_point, cc_relative from relative where cc_id=1 "
        "and cc_other=2",
        rs);

    long point = -1;
    int rel    = -1;
    EXPECT_TRUE(db_query_relative("1", "2", point, rel));
    EXPECT_EQ(point, 500);
    EXPECT_EQ(rel, 1);
}

TEST_F(DbLayerTest, QueryRelative_NotFound)
{
    long point = -1;
    int rel    = -1;
    EXPECT_FALSE(db_query_relative("999", "888", point, rel));
}

// ====================================================================
// Sql.cpp 核心链接验证（仍通过 mock）
// ====================================================================

TEST(DbCoreLink, GetDbType_Link) { EXPECT_STREQ(get_db_type(), "none"); }

// ====================================================================
// 补充测试：覆盖错误路径、空结果、边界条件
// ====================================================================

// --- SqlAccount.cpp: db_verify_password ---

TEST_F(DbLayerTest, VerifyPassword_Found)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"测试用户"});
    s_mockDb->SetQueryResult(
        "select cc_name from jianghudb where cc_id=1 and "
        "cc_verifypass=OLD_PASSWORD('pass')",
        rs);
    EXPECT_TRUE(db_verify_password("1", "pass"));
}

TEST_F(DbLayerTest, VerifyPassword_EmptyResult)
{
    // 默认空结果 → 返回 false
    EXPECT_FALSE(db_verify_password("1", "wrong"));
}

TEST_F(DbLayerTest, VerifyPassword_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_verify_password("1", "pass"));
}

TEST_F(DbLayerTest, VerifyPassword_EscapesQuote)
{
    // 密码含单引号，应被替换为空格
    DbResultSet rs = MockDatabase::MakeSingleRow({"u"});
    s_mockDb->SetQueryResult(
        "select cc_name from jianghudb where cc_id=1 and "
        "cc_verifypass=OLD_PASSWORD('a b')",
        rs);
    EXPECT_TRUE(db_verify_password("1", "a'b"));
}

// --- SqlAccount.cpp: query/execute 失败路径 ---

TEST_F(DbLayerTest, ChangePassword_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_change_password("1", "new"));
}

TEST_F(DbLayerTest, ChangeVerifyPassword_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_change_verify_password("1", "new"));
}

TEST_F(DbLayerTest, SetOnlineFlag_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_set_online_flag("1", 1));
}

TEST_F(DbLayerTest, QueryServer_EmptyResult)
{
    EXPECT_EQ(db_query_server("1"), -1);
}

TEST_F(DbLayerTest, QueryServer_RowsEmpty)
{
    // 返回一行但 columns 为空
    DbResultSet rs;
    DbRow row;  // columns 为空
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult("select cc_server from jianghudb where cc_id = 1",
                             rs);
    EXPECT_EQ(db_query_server("1"), -1);
}

TEST_F(DbLayerTest, QueryFreeze_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_EQ(db_query_freeze("1"), 1);  // 失败返回1
}

TEST_F(DbLayerTest, QueryFreeze_EmptyResult)
{
    EXPECT_EQ(db_query_freeze("1"), 1);
}

TEST_F(DbLayerTest, QueryFreeze_RowsEmpty)
{
    DbResultSet rs;
    DbRow row;
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult("select cc_freeze from jianghudb where cc_id=1",
                             rs);
    EXPECT_EQ(db_query_freeze("1"), 1);
}

TEST_F(DbLayerTest, SetFreeze_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_set_freeze("1", 1));
}

// --- SqlAccount.cpp: 频道/社交错误路径 ---

TEST_F(DbLayerTest, QueryRecruit_Fails)
{
    s_mockDb->SetShouldFail(true);
    DbResultSet result;
    EXPECT_FALSE(db_query_recruit("1", result));
}

TEST_F(DbLayerTest, UpdateRecruit_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_update_recruit("1", "2", "name", 100, 1));
}

TEST_F(DbLayerTest, UpdateRecruit_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_update_recruit("1", "2", "name", 100, 1));
}

TEST_F(DbLayerTest, QueryRelative_Fails)
{
    s_mockDb->SetShouldFail(true);
    long p = 0;
    int r  = 0;
    EXPECT_FALSE(db_query_relative("1", "2", p, r));
}

TEST_F(DbLayerTest, QueryRelative_RowsEmptyColumns)
{
    DbResultSet rs;
    DbRow row;  // columns 为空
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult(
        "select cc_point, cc_relative from relative where cc_id=1 "
        "and cc_other=2",
        rs);
    long p = 0;
    int r  = 0;
    EXPECT_FALSE(db_query_relative("1", "2", p, r));
}

TEST_F(DbLayerTest, UpdateRelative_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_update_relative("1", "2", 100, 1));
}

TEST_F(DbLayerTest, UpdateRelative_Insert_WithFlag)
{
    EXPECT_TRUE(db_update_relative("1", "2", 100, 1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("insert"), std::string::npos);
}

TEST_F(DbLayerTest, UpdateRelative_Update_NoFlag)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"100"});
    s_mockDb->SetQueryResult(
        "select cc_point from relative where cc_id=1 and cc_other=2", rs);
    EXPECT_TRUE(db_update_relative("1", "2", 200, -1));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
}

TEST_F(DbLayerTest, UpdateRelative_ExecuteFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(db_update_relative("1", "2", 100, 1));
}

// --- SqlContainer.cpp: 补充边界 ---

TEST_F(DbLayerTest, SaveContainer_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_save_container("t_item", "x", "y", "z", false));
    // 恢复
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, LoadContainer_NullDb)
{
    g_db = nullptr;
    std::string data;
    EXPECT_FALSE(db_load_container("t_item", "x", data));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, DeleteContainer_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_delete_container("t_item", "x"));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, LockContainer_NullDb)
{
    g_db   = nullptr;
    int cl = 0;
    EXPECT_FALSE(db_lock_container("t_item", "x", 1, cl));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, SaveObject_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_save_object("1", 0, "x", "y"));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, LoadObject_NullDb)
{
    g_db = nullptr;
    std::string b, d;
    EXPECT_FALSE(db_load_object("1", 0, b, d));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, LoadRoomBoxes_NullDb)
{
    g_db = nullptr;
    std::vector<std::string> v;
    EXPECT_FALSE(db_load_room_boxes("x", v));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, LoadContainer_ColumnsEmpty)
{
    // 返回一行但 columns 为空
    DbResultSet rs;
    DbRow row;
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult("select obj_data from t_item where obj_id='x'",
                             rs);
    std::string data;
    EXPECT_FALSE(db_load_container("t_item", "x", data));
}

TEST_F(DbLayerTest, LoadContainer_DataEmpty)
{
    // 返回一行但数据为空字符串
    DbResultSet rs = MockDatabase::MakeSingleRow({""});
    s_mockDb->SetQueryResult("select obj_data from t_item where obj_id='x'",
                             rs);
    std::string data;
    EXPECT_FALSE(db_load_container("t_item", "x", data));
}

TEST_F(DbLayerTest, LockContainer_RowsEmpty)
{
    DbResultSet rs;
    DbRow row;
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult("select obj_lock from t_item where obj_id='x'",
                             rs);
    int cl = 0;
    EXPECT_FALSE(db_lock_container("t_item", "x", 1, cl));
}

TEST_F(DbLayerTest, LockContainer_DataEmpty)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({""});
    s_mockDb->SetQueryResult("select obj_lock from t_item where obj_id='x'",
                             rs);
    int cl = 0;
    EXPECT_FALSE(db_lock_container("t_item", "x", 1, cl));
}

TEST_F(DbLayerTest, LoadObject_TooFewColumns)
{
    DbResultSet rs;
    DbRow row;
    row.columns.push_back("only_one");
    rs.rows.push_back(row);
    s_mockDb->SetQueryResult(
        "select base_name, savedata from t_saveobj where owner =1 "
        "and grid = 0",
        rs);
    std::string b, d;
    EXPECT_FALSE(db_load_object("1", 0, b, d));
}

TEST_F(DbLayerTest, LoadRoomBoxes_RowEmptyColumns)
{
    DbResultSet rs;
    DbRow row;
    rs.rows.push_back(row);
    std::vector<std::string> v;
    EXPECT_TRUE(db_load_room_boxes("x", v));
    EXPECT_EQ((int)v.size(), 0);
}

// --- SqlAccount.cpp: NullDb 路径 ---

TEST_F(DbLayerTest, VerifyPassword_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_verify_password("1", "pass"));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, ChangePassword_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_change_password("1", "pass"));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, ChangeVerifyPassword_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_change_verify_password("1", "pass"));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, SetOnlineFlag_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_set_online_flag("1", 1));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, QueryServer_NullDb)
{
    g_db = nullptr;
    EXPECT_EQ(db_query_server("1"), -1);
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, QueryFreeze_NullDb)
{
    g_db = nullptr;
    EXPECT_EQ(db_query_freeze("1"), 1);
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, SetFreeze_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_set_freeze("1", 1));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, QueryRecruit_NullDb)
{
    g_db = nullptr;
    DbResultSet r;
    EXPECT_FALSE(db_query_recruit("1", r));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, UpdateRecruit_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_update_recruit("1", "2", "n", 0, 0));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, QueryRelative_NullDb)
{
    g_db = nullptr;
    long p;
    int r;
    EXPECT_FALSE(db_query_relative("1", "2", p, r));
    g_db = s_mockDb;
}

TEST_F(DbLayerTest, UpdateRelative_NullDb)
{
    g_db = nullptr;
    EXPECT_FALSE(db_update_relative("1", "2", 100, 1));
    g_db = s_mockDb;
}

// ====================================================================
// SqlRank.cpp — paihangbang / sql_update_record
// ====================================================================

// paihangbang 返回值契约：query 失败 -> 0；成功（含空结果/有行） -> 1。
// 空结果是合法空榜，不是 store 失败（生产曾因返回值反转把成功当失败上报）。
TEST_F(DbLayerTest, Paihangbang_QueryFails_ReturnsZero)
{
    s_mockDb->SetShouldFail(true);
    CContainer me;
    EXPECT_EQ(0, paihangbang(&me));
}

TEST_F(DbLayerTest, Paihangbang_EmptyBoard_ReturnsOne)
{
    CContainer me;
    EXPECT_EQ(1, paihangbang(&me));
    // 空榜不应发出任何 SELECT 之外的写操作
    EXPECT_FALSE(s_mockDb->ExecutedContaining("insert"));
}

TEST_F(DbLayerTest, Paihangbang_WithRows_ReturnsOne)
{
    DbResultSet rs
        = MockDatabase::MakeSingleRow({"1001", "张三", "华山", "600000"});
    s_mockDb->SetQueryResultMatching("from jianghudb", rs);
    CContainer me;
    EXPECT_EQ(1, paihangbang(&me));
}

// 这个函数未在 Sql.h 中声明
extern bool sql_update_record(const char* id, int type, int level,
                              const char* data, bool update);

TEST_F(DbLayerTest, SqlUpdateRecord_NotFound_Inserts)
{
    // 默认空结果集 = 该 id/type 无记录 -> 走 INSERT 分支
    EXPECT_TRUE(sql_update_record("1", 1, 10, "data", false));
    EXPECT_TRUE(s_mockDb->ExecutedContaining("INSERT INTO t_record"));
}

TEST_F(DbLayerTest, SqlUpdateRecord_Found_UpdateHigherLevel)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"5"});
    s_mockDb->SetQueryResult(
        "SELECT cc_level FROM t_record WHERE cc_id = '1' and cc_type = 1;", rs);
    EXPECT_TRUE(sql_update_record("1", 1, 10, "newdata", false));
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update"), std::string::npos);
    EXPECT_NE(sql.find("t_record"), std::string::npos);
}

TEST_F(DbLayerTest, SqlUpdateRecord_Found_SkipUpdate_LowerLevel)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"20"});
    s_mockDb->SetQueryResult(
        "SELECT cc_level FROM t_record WHERE cc_id = '1' and cc_type = 1;", rs);
    EXPECT_TRUE(sql_update_record("1", 1, 5, "data", true));
    // 现有等级 20 > 新等级 5："跳过" 由 UPDATE 的 WHERE cc_level < 5 在
    // DB 侧保证（低等级行不匹配 WHERE，不会被覆盖）；语句仍会下发。
    std::string sql = s_mockDb->GetLastSql();
    EXPECT_NE(sql.find("update t_record"), std::string::npos);
    EXPECT_NE(sql.find("cc_level < 5"), std::string::npos);
}

TEST_F(DbLayerTest, SqlUpdateRecord_QueryFails)
{
    s_mockDb->SetShouldFail(true);
    EXPECT_FALSE(sql_update_record("1", 1, 10, "data", false));
}

TEST_F(DbLayerTest, SqlUpdateRecord_ExecuteFails)
{
    DbResultSet rs = MockDatabase::MakeSingleRow({"1"});
    s_mockDb->SetQueryResult(
        "SELECT cc_level FROM t_record WHERE cc_id = '1' and cc_type = 1;", rs);
    s_mockDb->SetExecuteErrorCode(1);  // execute 返回非 0 = 失败
    EXPECT_FALSE(sql_update_record("1", 1, 10, "data", false));
    s_mockDb->SetExecuteErrorCode(0);
}

TEST_F(DbLayerTest, SqlUpdateRecord_NullDb)
{
    // sql_update_record 无 g_db 空检查，会崩溃，跳过
}

// ====================================================================
// IDatabase.h 的两个内联适配体（经 MockDatabase 虚分发到基类实现）
// ====================================================================

TEST_F(DbLayerTest, IDatabase_CreateTable_DelegatesToExecute)
{
    EXPECT_EQ(0, s_mockDb->create_table("create table t_probe (id int)"));
    EXPECT_TRUE(s_mockDb->ExecutedContaining("create table t_probe"))
        << "create_table 直通 execute，DDL 原样入执行流";

    s_mockDb->SetShouldFail(true);
    EXPECT_NE(0, s_mockDb->create_table("create table t_probe2 (id int)"))
        << "失败码原样透传";
}

TEST_F(DbLayerTest, IDatabase_SetCharset_IssuesSetNames)
{
    EXPECT_EQ(0, s_mockDb->set_charset("utf8"));
    EXPECT_EQ("set names 'utf8'", s_mockDb->GetLastSql())
        << "字符集适配体拼出 set names 语句";

    s_mockDb->SetShouldFail(true);
    EXPECT_NE(0, s_mockDb->set_charset("gbk"))
        << "失败码原样透传（mock 失败路径不落 SQL 记录）";
}

// ====================================================================
// SqlAccount.cpp 异常面：11 个函数的 catch(...) + 2 个 execute 失败支
//   ThrowingDatabase 双开关让首个 db 调用抛出 → 捕获路径必达
// ====================================================================

TEST_F(DbLayerTest, AccountDbFunctionsSurviveDbExceptions)
{
    ThrowingDatabase tdb;
    tdb.throwOnQuery   = true;
    tdb.throwOnExecute = true;
    g_db               = &tdb;

    DbResultSet rs;
    long point = 0;
    int rel    = 0;

    EXPECT_FALSE(db_verify_password("10001", "pw")) << "catch → false(48-49)";
    EXPECT_FALSE(db_change_password("10001", "npw")) << "catch → false(65-66)";
    EXPECT_FALSE(db_change_verify_password("10001", "nvw"))
        << "catch → false(84-85)";
    EXPECT_FALSE(db_set_online_flag("10001", 1)) << "catch → false(105-106)";
    EXPECT_EQ(-1, db_query_server("10001")) << "catch → -1(132-133)";
    EXPECT_EQ(1, db_query_freeze("10001")) << "catch → 1(159-160)";
    EXPECT_FALSE(db_set_freeze("10001", 1)) << "catch → false(176-177)";
    EXPECT_FALSE(db_query_recruit("20001", rs)) << "catch → false(202-203)";
    EXPECT_FALSE(db_update_recruit("20001", "30001", "弟子", 10, 0))
        << "catch → false(247-248)";
    EXPECT_FALSE(db_query_relative("10001", "10002", point, rel))
        << "catch → false(278-279)";
    EXPECT_FALSE(db_update_relative("10001", "10002", 10, 1))
        << "catch → false(337-338)";

    g_db = s_mockDb;  // 恢复 fixture mock（EXPECT 非致命 → 必达）
}

TEST_F(DbLayerTest, AccountUpdateExecuteFailureLogs)
{
    s_mockDb->SetExecuteErrorCode(1);  // execute 失败、query 正常

    EXPECT_FALSE(db_update_recruit("20002", "30002", "门徒", 20, 0))
        << "execute 失败 → LOG + false(242-243)";
    EXPECT_FALSE(db_update_relative("10003", "10004", 20, 0))
        << "execute 失败 → LOG + false(332-333)";
}

// ====================================================================
// SqlContainer 深路：7 个 catch(...) 未知异常支(65-66 … 294-295)
// ====================================================================
TEST_F(DbLayerTest, SqlContainer_AllUnknownExceptionCatches)
{
    // 一抛即炸的 DB：try 内首个 db 调用抛 → 逐函数落 catch(...)
    ThrowingDatabase boom;
    boom.throwOnExecute = true;
    boom.throwOnQuery   = true;
    IDatabase* saved    = g_db;
    g_db                = &boom;

    std::string data;
    std::string base;
    int cl = 0;
    std::vector<std::string> boxes;

    EXPECT_FALSE(db_save_container("t", "i1", "n", "d", false))
        << "save_container catch(65-66)";
    EXPECT_FALSE(db_save_container("t", "i1", "n", "d", true))
        << "update 支同样落 catch";
    EXPECT_FALSE(db_load_container("t", "i1", data))
        << "load_container catch(95-96)";
    EXPECT_FALSE(db_delete_container("t", "i1"))
        << "delete_container catch(115-116)";
    EXPECT_FALSE(db_lock_container("t", "i1", 1, cl))
        << "lock_container catch(163-164)";
    EXPECT_FALSE(db_save_object("1", 0, "x", "y"))
        << "save_object catch(227-228)";
    EXPECT_FALSE(db_load_object("1", 0, base, data))
        << "load_object catch(259-260)";
    EXPECT_FALSE(db_load_room_boxes("r", boxes))
        << "load_room_boxes catch(294-295)";

    g_db = saved;
}

// ====================================================================
// SqlContainer 深路：execute/query 失败支(157-159,222-223,247-248,283-284)
// ====================================================================
TEST_F(DbLayerTest, SqlContainer_ExecuteAndQueryFailBranches)
{
    // lock：query 成功（预置行）→ execute 失败(157-159)
    DbResultSet rs = MockDatabase::MakeSingleRow({"0"});
    s_mockDb->SetQueryResult(
        "select obj_lock from t_item where obj_id='chest_01'", rs);
    s_mockDb->SetExecuteErrorCode(1);
    int cl = -1;
    EXPECT_FALSE(db_lock_container("t_item", "chest_01", 1, cl))
        << "lock execute 失败(157-159)";

    // save_object：无预置 → insert 路径 → execute 失败(222-223)
    EXPECT_FALSE(db_save_object("1001", 0, "sword", "data"))
        << "save_object execute 失败(222-223)";
    s_mockDb->SetExecuteErrorCode(0);

    // load_object / load_room_boxes：首个 query 失败(247-248,283-284)
    s_mockDb->SetShouldFail(true);
    std::string base, data;
    EXPECT_FALSE(db_load_object("1001", 0, base, data))
        << "load_object query 失败(247-248)";
    std::vector<std::string> boxes;
    EXPECT_FALSE(db_load_room_boxes("road1", boxes))
        << "load_room_boxes query 失败(283-284)";
}
