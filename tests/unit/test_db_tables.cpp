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

// 服务器依赖的核心表：账号、门派招募、容器/物品/房间/箱子、社交关系等。
// 数据集专用表（如 t_treasure、t_corp）不在此列——它们由脚本经 db_register_ddl
// 登记、prepare_tables 末尾统一执行，见 DatasetDdl 用例。
// 顺序需与 prepare_tables 的执行顺序一致，失败点用例据此标注表名。
const char* const kRequiredTables[] = {
    "jianghudb", "recruit_exp", "t_item",   "t_npc",
    "t_room",    "t_box",       "relative",
};

constexpr int kTableCount
    = static_cast<int>(sizeof(kRequiredTables) / sizeof(kRequiredTables[0]));

std::string CreateDdl(const char* table)
{
    return std::string("CREATE TABLE IF NOT EXISTS `") + table + "`";
}

// 预置 jianghudb 列探测结果：真实库的 information_schema 必然返回列清单，
// mock 默认空行集会把凭据列全判为缺失。
void PresetColumnProbe(MockDatabase& db, std::vector<std::string> columns)
{
    DbResultSet rs;
    for (const std::string& name : columns) {
        DbRow row;
        row.columns.push_back(name);
        rs.rows.push_back(row);
    }
    db.SetQueryResultMatching("information_schema.columns", rs);
}

// 新库形态：建表语句里已含全部凭据列
void PresetAllCredentialColumnsPresent(MockDatabase& db)
{
    PresetColumnProbe(
        db, {"cc_id", "cc_name", "cc_passwd", "cc_verifypass", "cc_passwd_salt",
             "cc_passwd_hash", "cc_verifypass_salt", "cc_verifypass_hash"});
}

int CountAlterTables(const MockDatabase& db)
{
    int n = 0;
    for (const std::string& sql : db.GetExecutedSqls()) {
        if (sql.find("ALTER TABLE jianghudb ADD COLUMN") != std::string::npos)
            n++;
    }
    return n;
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
        PresetAllCredentialColumnsPresent(db);
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
    // kTableCount 条建表 DDL + 1 次已发号查询 + 1 次起点播种
    // + 1 次凭据列探测
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 3);
}

// 玩家 ID 起点：已发号不足 1000（1~999 预留给巫师/系统账号）时播种自增
// 起点。空库（MAX 为 NULL）与历史无起点库（已发号落在预留段内）同属此列。
TEST_F(DbTablesTest, PrepareTables_SeedsIdFloorWhenPlayersBelowReserve)
{
    // Mock 的查询默认返回空行集 → 读不到已发号 → 视为需要播种
    EXPECT_TRUE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("AUTO_INCREMENT = 1000"))
        << "已发号不足起点必须把自增起点抬到 1000";
}

// 历史无起点库（已发号为 3）在下一次开机时被修复：起点抬到 1000，
// 新号不再落进巫师/系统预留段
TEST_F(DbTablesTest, PrepareTables_SeedsIdFloorWhenCounterFellBelow)
{
    db.SetQueryResultMatching("MAX(cc_id)", MockDatabase::MakeSingleRow({"3"}));

    EXPECT_TRUE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("AUTO_INCREMENT = 1000"))
        << "已发号仍在预留段内必须补播起点";
}

// 已发号进入 1000+ → 起点已在预留段之外，绝不能再动自增计数器
TEST_F(DbTablesTest, PrepareTables_SkipsIdFloorBeyondReserve)
{
    db.SetQueryResultMatching("MAX(cc_id)",
                              MockDatabase::MakeSingleRow({"1578"}));

    EXPECT_TRUE(prepare_tables());
    EXPECT_FALSE(db.ExecutedContaining("AUTO_INCREMENT = 1000"))
        << "已发号越过预留段后不得再干预自增计数器";
}

TEST_F(DbTablesTest, PrepareTables_StopsAtFirstFailingTable)
{
    for (int failing = 0; failing < kTableCount; failing++) {
        db.Clear();
        db.SetExecuteFailAt(failing);

        EXPECT_FALSE(prepare_tables())
            << "failure at table " << kRequiredTables[failing];
        // 已发号查询在建表之后才发生，此时只下发了失败前(含)的 failing+1 条 DDL
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
    PresetAllCredentialColumnsPresent(db);
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_a` (`x` "
        "int)engine=InnoDB default charset=utf8;");
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_b` (`y` "
        "int)engine=InnoDB default charset=utf8;");

    EXPECT_TRUE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("CREATE TABLE IF NOT EXISTS `t_ds_a`"));
    EXPECT_TRUE(db.ExecutedContaining("CREATE TABLE IF NOT EXISTS `t_ds_b`"));
    // 1 次存在性查询 + 核心表 + 起点播种 + 凭据列探测 + 2 条注册表
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 5);
}

TEST_F(DbTablesDatasetTest, PrepareTables_StopsAtFailingDatasetDdl)
{
    PresetAllCredentialColumnsPresent(db);
    g_dataset_table_ddl.push_back(
        "CREATE TABLE IF NOT EXISTS `t_ds_fail` (`x` "
        "int)engine=InnoDB default charset=utf8;");
    // 核心表(索引 0~kTableCount-1)之后是起点播种(索引 kTableCount)，
    // 末条注册表失败：核心表与播种全部成功后，执行到注册表时中止
    db.SetExecuteFailAt(kTableCount + 1);

    EXPECT_FALSE(prepare_tables());
    EXPECT_EQ(static_cast<int>(db.GetExecutedSqls().size()), kTableCount + 4);
}

// ====================================================================
// 历史库补列：CREATE TABLE IF NOT EXISTS 不给已有表加列，靠探测 + ALTER
// ====================================================================

// 老库没有凭据列 → 逐列追加（类型/默认值与建表语句同源）
TEST_F(DbTablesTest, PrepareTables_AddsMissingCredentialColumns)
{
    db.Clear();  // 清掉夹具的「列齐全」预置，改用历史库形态
    PresetColumnProbe(db, {"cc_id", "cc_name", "cc_passwd", "cc_verifypass"});

    EXPECT_TRUE(prepare_tables());
    EXPECT_EQ(CountAlterTables(db), 4);
    EXPECT_TRUE(db.ExecutedContaining(
        "ALTER TABLE jianghudb ADD COLUMN `cc_passwd_salt`"));
    EXPECT_TRUE(db.ExecutedContaining(
        "ALTER TABLE jianghudb ADD COLUMN `cc_passwd_hash`"));
    EXPECT_TRUE(db.ExecutedContaining(
        "ALTER TABLE jianghudb ADD COLUMN `cc_verifypass_salt`"));
    EXPECT_TRUE(db.ExecutedContaining(
        "ALTER TABLE jianghudb ADD COLUMN `cc_verifypass_hash`"));
}

// 新库建表已含凭据列 → 探测后不再动表
TEST_F(DbTablesTest, PrepareTables_SkipsCredentialColumnsWhenPresent)
{
    EXPECT_TRUE(prepare_tables());
    EXPECT_EQ(CountAlterTables(db), 0);
}

// 探测失败即启动失败：缺列时注册与登录会在运行期报错，不如开机阶段暴露
TEST_F(DbTablesTest, PrepareTables_FailsWhenColumnProbeFails)
{
    db.SetQueryErrorCode(1);
    EXPECT_FALSE(prepare_tables());
    EXPECT_EQ(CountAlterTables(db), 0) << "探测没结论就不猜列的存在性";
}

// 账号 ID 起点播种失败必须中止启动：起点没播下去就建号，新号会落进
// 1~999 的巫师/系统预留段
TEST_F(DbTablesTest, PrepareTables_FailsWhenIdFloorSeedFails)
{
    db.SetExecuteFailAt(kTableCount);  // 核心表之后的第一条 execute 即播种
    EXPECT_FALSE(prepare_tables());
    EXPECT_TRUE(db.ExecutedContaining("AUTO_INCREMENT = 1000"));
}

// 补列失败必须中止启动：缺列的库跑起来，注册与登录会逐条报错
TEST_F(DbTablesTest, PrepareTables_FailsWhenAddingColumnFails)
{
    db.Clear();  // 换成历史库形态（缺凭据列）
    PresetColumnProbe(db, {"cc_id", "cc_name", "cc_passwd", "cc_verifypass"});
    db.SetExecuteFailAt(kTableCount + 1);  // 播种之后的第一条 execute 即 ALTER

    EXPECT_FALSE(prepare_tables());
    EXPECT_EQ(CountAlterTables(db), 1) << "首条补列语句已下发并失败";
}
