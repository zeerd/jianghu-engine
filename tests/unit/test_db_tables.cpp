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

// 服务器依赖的核心表：账号、门派招募、容器/物品/房间/箱子、存档、战斗记录等。
// 数据集专用表（如 t_treasure、t_corp）不在此列——它们由脚本经 db_register_ddl
// 登记、prepare_tables 末尾统一执行，见 DatasetDdl 用例。
// 顺序需与 prepare_tables 的执行顺序一致，失败点用例据此标注表名。
const char* const kRequiredTables[] = {
    "jianghudb", "recruit_exp", "t_item", "t_npc",    "t_room",
    "t_box",     "t_saveobj",   "user",   "t_record", "relative",
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
    // 1 次 jianghudb 存在性查询 + kTableCount 条建表 DDL + 1 次起点播种
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 2);
}

// 玩家 ID 起点：jianghudb 首次建表时把自增起点抬到 1000（1~999 预留给
// 巫师/系统账号）。起点只在"建表前表不存在"时播种——已有库重跑
// prepare_tables 绝不能重置自增计数器（否则新号会撞已发的 cc_id）。
TEST_F(DbTablesTest, PrepareTables_SeedsIdFloorOnFirstCreation)
{
    // Mock 的 existence 查询默认返回空行集 → 视为首次建表 → 播种起点
    EXPECT_TRUE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("AUTO_INCREMENT = 1000"))
        << "fresh table must be seeded with the id floor";
}

TEST_F(DbTablesTest, PrepareTables_SkipsIdFloorWhenTableExists)
{
    // existence 查询返回一行 → jianghudb 已存在 → 禁止播种
    db.SetQueryResultMatching("jianghudb",
                              MockDatabase::MakeSingleRow({"jianghudb"}));

    EXPECT_TRUE(prepare_tables());
    EXPECT_FALSE(db.ExecutedContaining("AUTO_INCREMENT = 1000"))
        << "existing table must not have its id floor reset";
}

TEST_F(DbTablesTest, PrepareTables_StopsAtFirstFailingTable)
{
    for (int failing = 0; failing < kTableCount; failing++) {
        db.Clear();
        db.SetExecuteFailAt(failing);

        EXPECT_FALSE(prepare_tables())
            << "failure at table " << kRequiredTables[failing];
        // 1 次存在性查询 + 失败前(含)的 failing+1 条 DDL
        EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), failing + 2)
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
    // 1 次存在性查询 + 核心表 + 起点播种 + 2 条注册表
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 4);
}

TEST_F(DbTablesDatasetTest, PrepareTables_StopsAtFailingDatasetDdl)
{
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_fail` (`x` "
        "int)engine=InnoDB default charset=utf8;");
    // 核心表(索引 0~kTableCount-1)之后是起点播种(索引 kTableCount)，
    // 末条注册表失败：核心表与播种全部成功后，执行到注册表时中止
    db.SetExecuteFailAt(kTableCount + 1);

    EXPECT_FALSE(prepare_tables());
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 3);
}
