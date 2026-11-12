// test_mysql_integration.cpp — 真实 MySQL 后端与生产建表/账号链的集成测试
//
// 与 test_sqlite_integration.cpp 成对：账号 ID 起点（1000，1~999 留给巫师/
// 系统账号）这条契约在两个真实后端上各测一遍。MySQL 方言（engine 表选项、
// information_schema 存在性探测、ALTER TABLE ... AUTO_INCREMENT 播种）在
// Mock 上只能断言 SQL 字符串，这里跑的是生产 prepare_tables 的真实执行结果。
//
// 依赖本机 MySQL 服务与 tests/unit/test_config.ini 的 [database] 连接配置；
// 服务不可达时用例记为通过并说明原因——MySQL 不是单测的硬依赖。
//
// Build+Run: .\run-coverage.ps1
//   x64\Release\tests.exe --gtest_filter=MysqlIntegrationTest.*
#include "pch.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "IDatabase.h"
#include "IniParser.h"
#include "Sql.h"

// Sql.h 未声明的生产入口
bool prepare_tables(void);

namespace {

// 每进程一个库名：run-coverage 的分片是并行的独立进程，互不踩库
std::string TestDbName()
{
    return std::string("game_unit_idfloor_")
           + std::to_string(GetCurrentProcessId());
}

class MysqlIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<IDatabase> db;
    IDatabase* saved_db = nullptr;
    std::vector<std::string> saved_ddl;

    void SetUp() override
    {
        saved_db  = g_db;
        saved_ddl = g_dataset_table_ddl;
        g_dataset_table_ddl.clear();

        db = create_my_sql_db();
        ini_load("tests/unit/test_config.ini");
        if (db->connect(ini_get("database", "host", "127.0.0.1"),
                        ini_get("database", "user", "root"),
                        ini_get("database", "password", "root"), nullptr)
            != 0) {
            db.reset();
            return;  // MySQL 不可达：用例自行记原因放行
        }

        const std::string name = TestDbName();
        db->execute(("DROP DATABASE IF EXISTS `" + name + "`").c_str());
        if (db->create_database(name.c_str()) != 0
            || db->select_database(name.c_str()) != 0) {
            db.reset();
            return;
        }
        // 连接字符集取 utf8：测试源码字符串就是 UTF-8 字节，按其他字符集
        // 转码会把建档中文字段写坏（严格模式下直接写失败）
        db->set_charset("utf8");
        g_db = db.get();
    }

    void TearDown() override
    {
        g_dataset_table_ddl = saved_ddl;
        g_db                = saved_db;
        if (db) {
            // 连接选中的正是本用例的库，可直接丢弃，不留残库
            const std::string name = TestDbName();
            db->execute(("DROP DATABASE IF EXISTS `" + name + "`").c_str());
            db->close();
            db.reset();
        }
    }

    // MySQL 不可达时放行并写明原因，不制造环境性红灯
    bool MysqlUnavailable()
    {
        if (db) return false;
        SUCCEED() << "MySQL 不可达，本用例未执行";
        return true;
    }
};

}  // namespace

// ====================================================================
// 账号 ID 起点（1000）
// ====================================================================

// 首次建表时把自增起点抬到 1000：首个注册账号不得落在巫师/系统预留段
TEST_F(MysqlIntegrationTest, PrepareTables_SeedsIdFloorOnFreshDb)
{
    if (MysqlUnavailable()) return;

    ASSERT_TRUE(prepare_tables()) << db->error();

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(id, 1000) << err;
}

// 模拟重启：jianghudb 已存在时重跑 prepare_tables 不得重置起点，
// 否则新账号会撞已发的 cc_id（主键冲突 → 注册失败）
TEST_F(MysqlIntegrationTest, PrepareTables_ReRunKeepsIdFloor)
{
    if (MysqlUnavailable()) return;

    ASSERT_TRUE(prepare_tables()) << db->error();

    char err[256] = "";
    ASSERT_EQ(db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err)),
              1000)
        << err;

    ASSERT_TRUE(prepare_tables()) << db->error();  // 模拟第二次开机

    long id = db_register_account("李四", "pass1234", "lisi@example.com",
                                  "女性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(id, 1001) << err;  // 起点未被重置，ID 自然递增
}

// 与 ensure_wizard_accounts 的契约：巫师用显式 cc_id=1/2 插入，
// 不得把自增起点压回 3——巫师占 1/2，玩家仍从 1000 开始
TEST_F(MysqlIntegrationTest, PrepareTables_FloorCoexistsWithWizardAccounts)
{
    if (MysqlUnavailable()) return;

    ASSERT_TRUE(prepare_tables()) << db->error();

    // 复刻 ensure_wizard_accounts 的显式插入
    ASSERT_EQ(
        db->execute("insert into jianghudb (cc_id, cc_name, cc_passwd, "
                    "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                    "values (1, 'wizard1', 'pw', 21, 21, 21, 21, 'male', "
                    "100)"),
        0)
        << db->error();
    ASSERT_EQ(
        db->execute("insert into jianghudb (cc_id, cc_name, cc_passwd, "
                    "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                    "values (2, 'wizard2', 'pw', 21, 21, 21, 21, 'female', "
                    "100)"),
        0)
        << db->error();

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(id, 1000) << err;  // 显式插入 1/2 不下压起点，玩家仍是 1000
}

// 历史无起点库（建表时没播种，已发号落在 1~999 预留段内）在下一次开机
// 被修复：起点抬到 1000，注册不再落进巫师/系统预留段，且不撞已发号
TEST_F(MysqlIntegrationTest, PrepareTables_RepairsLegacyDbWithoutFloor)
{
    if (MysqlUnavailable()) return;

    // 复刻「没有起点的历史库」：只带注册要用的列，自增从 1 起，
    // 凭据列也缺——由 prepare_tables 走历史库补列路径补齐
    ASSERT_EQ(db->execute("CREATE TABLE `jianghudb` ("
                          "`cc_id` int(32) NOT NULL AUTO_INCREMENT,"
                          "`cc_name` varchar(40) NOT NULL DEFAULT '',"
                          "`cc_passwd` varchar(22) NOT NULL DEFAULT '',"
                          "`cc_email` varchar(64) NOT NULL DEFAULT '',"
                          "`cc_str` int(16) NOT NULL DEFAULT '0',"
                          "`cc_int` int(16) NOT NULL DEFAULT '0',"
                          "`cc_con` int(16) NOT NULL DEFAULT '0',"
                          "`cc_dex` int(16) NOT NULL DEFAULT '0',"
                          "`cc_gender` varchar(4) NOT NULL DEFAULT '',"
                          "`cc_age` int(8) NOT NULL DEFAULT '0',"
                          "`cc_level` tinyint(3) NOT NULL DEFAULT '0',"
                          "PRIMARY KEY (`cc_id`)"
                          ")engine=InnoDB default charset=utf8;"),
              0)
        << db->error();
    ASSERT_EQ(db->execute("INSERT INTO jianghudb (cc_id, cc_name) VALUES "
                          "(1, 'wizard1'), (2, 'wizard2'), (3, 'legacy')"),
              0)
        << db->error();

    // 建表 IF NOT EXISTS 跳过已有表 → 补凭据列 → 补起点
    ASSERT_TRUE(prepare_tables()) << db->error();

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(id, 1000) << err;  // 预留段内的已发号之后，起点补到 1000
}
