// test_db_tables.cpp — DbTables.cpp 建表脚本单元测试
//
// 断言服务器启动时按顺序创建全部游戏表，任一表建失败即中止，
// 且 DB 异常不穿透到启动流程。
#include "pch.h"

#include <string>

#include "IDatabase.h"
#include "Sql.h"
#include "mock/mock_database.h"

bool prepare_tables(void);

namespace {

// 服务器依赖的核心表：账号、门派招募、容器/物品/房间、存档、战斗记录等。
// 数据集专用表（如 t_treasure、t_corp）不在此列——它们由脚本经 db_register_ddl
// 登记、prepare_tables 末尾统一执行，见 DatasetDdl 用例。
const char* const kRequiredTables[] = {
    "jianghudb", "recruit_exp", "t_item", "t_npc", "t_room",
    "t_saveobj", "t_box",       "user",   "t_mf",  "t_record",
};

constexpr int kTableCount
    = static_cast<int>(sizeof(kRequiredTables) / sizeof(kRequiredTables[0]));

std::string CreateDdl(const char* table)
{
    return std::string("CREATE TABLE IF NOT EXISTS `") + table + "`";
}

class DbTablesTest : public ::testing::Test {
protected:
    MockDatabase db;
    std::vector<std::string> saved_ddl;

    // 其他用例 boot dataset hooks 会往全局注册清单里留 DDL，
    // 本用例断言核心表的确切执行数，必须先隔离该全局状态。
    void SetUp() override
    {
        g_db      = &db;
        saved_ddl = g_dataset_table_ddl;
        g_dataset_table_ddl.clear();
    }
    void TearDown() override
    {
        g_dataset_table_ddl = saved_ddl;
        g_db                = nullptr;
    }
};

class DbTablesThrowTest : public ::testing::Test {
protected:
    ThrowingDatabase db;

    void SetUp() override { g_db = &db; }
    void TearDown() override { g_db = nullptr; }
};

}  // namespace

TEST_F(DbTablesTest, PrepareTables_CreatesEveryGameTable)
{
    EXPECT_TRUE(prepare_tables());

    for (const char* table : kRequiredTables) {
        EXPECT_TRUE(db.ExecutedContaining(CreateDdl(table)))
            << "missing DDL for table " << table;
    }
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount);
}

TEST_F(DbTablesTest, PrepareTables_StopsAtFirstFailingTable)
{
    for (int failing = 0; failing < kTableCount; failing++) {
        db.Clear();
        db.SetExecuteFailAt(failing);

        EXPECT_FALSE(prepare_tables())
            << "failure at table " << kRequiredTables[failing];
        EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), failing + 1)
            << "must stop at table " << kRequiredTables[failing];
    }
}

TEST_F(DbTablesThrowTest, PrepareTables_SwallowsDbException)
{
    db.throwOnExecute = true;

    EXPECT_FALSE(prepare_tables());
}

// 数据集专用表：脚本登记的 DDL 在核心表之后逐条执行；任一失败即中止。
class DbTablesDatasetTest : public ::testing::Test {
protected:
    MockDatabase db;
    std::vector<std::string> saved;

    void SetUp() override
    {
        g_db  = &db;
        saved = g_dataset_table_ddl;
        g_dataset_table_ddl.clear();
    }
    void TearDown() override
    {
        g_dataset_table_ddl = saved;
        g_db                = nullptr;
    }
};

TEST_F(DbTablesDatasetTest, PrepareTables_ExecutesRegisteredDdl)
{
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_a` (`x` "
        "int)engine=InnoDB default charset=utf8;");
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_b` (`y` "
        "int)engine=InnoDB default charset=utf8;");

    EXPECT_TRUE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("CREATE TABLE IF NOT EXISTS `t_ds_a`"));
    EXPECT_TRUE(db.ExecutedContaining("CREATE TABLE IF NOT EXISTS `t_ds_b`"));
    // 核心表 + 2 条注册表
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 2);
}

TEST_F(DbTablesDatasetTest, PrepareTables_StopsAtFailingDatasetDdl)
{
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_fail` (`x` "
        "int)engine=InnoDB default charset=utf8;");
    // 末条失败：核心表全部成功后，执行到注册表时中止
    db.SetExecuteFailAt(kTableCount);

    EXPECT_FALSE(prepare_tables());
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 1);
}
