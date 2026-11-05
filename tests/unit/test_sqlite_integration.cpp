// test_sqlite_integration.cpp — 真实 SQLite 后端与生产 db 层/SQL 的集成测试
//
// 目的：把真实内存 SQLite 接入 g_db，用生产函数（prepare_tables、db_* 等）
// 真正跑一遍建表与 CRUD，验证 SQLite 后端可用。
// 与 test_db_layer.cpp 互补：那里用 MockDatabase 只断言 SQL 字符串，
// 这里在真实后端上断言可观察结果（建表成功、写入能读回、字节保真）。
//
// Build+Run: .\run-coverage.ps1
//   Release\tests.exe --gtest_filter=SqliteIntegrationTest.*
#include "pch.h"

#include <cstdio>
#include <string>
#include <vector>

#include "Container.h"
#include "IDatabase.h"
#include "Sql.h"

// Sql.h 未声明的生产入口
bool prepare_tables(void);

namespace {

class SqliteIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<IDatabase> db;
    IDatabase* saved_db = nullptr;
    std::vector<std::string> saved_ddl;

    void SetUp() override
    {
        db = create_sqlite_db();
        ASSERT_NE(db, nullptr);
        // db 路径为空 → 内存库（":memory:"）
        ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0)
            << db->error();

        saved_db = g_db;
        g_db     = db.get();

        // prepare_tables 末尾会消费全局注册表；隔离以免受其他用例影响
        saved_ddl = g_dataset_table_ddl;
        g_dataset_table_ddl.clear();
    }

    void TearDown() override
    {
        g_dataset_table_ddl = saved_ddl;
        g_db                = saved_db;
    }

    bool TableExists(const char* name)
    {
        DbResultSet rs;
        const std::string sql
            = std::string(
                  "select name from sqlite_master where type='table' "
                  "and name='")
              + name + "'";
        if (db->query(sql.c_str(), rs) != 0) return false;
        return rs.size() == 1;
    }
};

}  // namespace

// ====================================================================
// 建表
// ====================================================================

TEST_F(SqliteIntegrationTest, PrepareTables_CreatesEveryTable)
{
    ASSERT_TRUE(prepare_tables()) << db->error();

    for (const char* t :
         {"jianghudb", "recruit_exp", "t_item", "t_npc", "t_room", "t_box",
          "t_saveobj", "user", "t_record", "relative"}) {
        EXPECT_TRUE(TableExists(t)) << "missing table " << t;
    }
}

TEST_F(SqliteIntegrationTest, PrepareTables_IsIdempotent)
{
    ASSERT_TRUE(prepare_tables()) << db->error();
    EXPECT_TRUE(prepare_tables()) << db->error();  // CREATE TABLE IF NOT EXISTS
}

// ====================================================================
// 容器 / 存档 / 箱子 / 锁
// ====================================================================

TEST_F(SqliteIntegrationTest, ContainerCrudRoundTrip)
{
    ASSERT_TRUE(prepare_tables());

    ASSERT_TRUE(
        db_save_container("t_item", "sword_01", "铁剑", "data1", false));
    std::string data;
    ASSERT_TRUE(db_load_container("t_item", "sword_01", data));
    EXPECT_EQ(data, "data1");

    ASSERT_TRUE(db_save_container("t_item", "sword_01", "铁剑", "data2", true));
    ASSERT_TRUE(db_load_container("t_item", "sword_01", data));
    EXPECT_EQ(data, "data2");

    ASSERT_TRUE(db_delete_container("t_item", "sword_01"));
    EXPECT_FALSE(db_load_container("t_item", "sword_01", data));
}

// JSON blob 里的反斜杠/单引号/双引号/制表符必须原样存取：MySQL 侧靠
// NO_BACKSLASH_ESCAPES，SQLite 原生不处理转义；两后端契约一致。
TEST_F(SqliteIntegrationTest, ContainerBlobPreservesSpecialBytes)
{
    ASSERT_TRUE(prepare_tables());

    const std::string payload
        = "{\"a\":\"x\\\\y\",\"b\":\"it's\",\"c\":\"\\\"q\\\"\","
          "\"t\":\"a\\tb\"}";
    ASSERT_TRUE(
        db_save_container("t_box", "blob_1", "箱", payload.c_str(), false));

    std::string data;
    ASSERT_TRUE(db_load_container("t_box", "blob_1", data));
    EXPECT_EQ(data, payload);
}

TEST_F(SqliteIntegrationTest, SaveObjectRoundTrip)
{
    ASSERT_TRUE(prepare_tables());

    // 使用通用容器接口操作 t_saveobj 表
    // obj_id 格式: owner_grid
    ASSERT_TRUE(db_save_container("t_saveobj", "1001_0", "sword", "json1",
                                  false));  // insert
    std::string data;
    ASSERT_TRUE(db_load_container("t_saveobj", "1001_0", data));
    EXPECT_EQ(data, "json1");

    ASSERT_TRUE(db_save_container("t_saveobj", "1001_0", "sword2", "json2",
                                  true));  // update
    ASSERT_TRUE(db_load_container("t_saveobj", "1001_0", data));
    EXPECT_EQ(data, "json2");
}

TEST_F(SqliteIntegrationTest, RoomBoxesLoadByPrefix)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_TRUE(db_save_container("t_box", "road1_0", "箱", "b0", false));
    ASSERT_TRUE(db_save_container("t_box", "road1_1", "箱", "b1", false));
    ASSERT_TRUE(db_save_container("t_box", "road2_0", "箱", "c0", false));

    std::vector<std::string> boxes;
    // 使用通用容器接口逐个加载
    std::string data;
    ASSERT_TRUE(db_load_container("t_box", "road1_0", data));
    boxes.push_back(data);
    ASSERT_TRUE(db_load_container("t_box", "road1_1", data));
    boxes.push_back(data);
    EXPECT_EQ((int)boxes.size(), 2);
    EXPECT_EQ(boxes[0], "b0");
    EXPECT_EQ(boxes[1], "b1");
}

TEST_F(SqliteIntegrationTest, LockContainerTogglesState)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_TRUE(db_save_container("t_item", "chest1", "箱子", "d", false));

    int cur = -1;
    ASSERT_TRUE(db_lock_container("t_item", "chest1", 1, cur));
    EXPECT_EQ(cur, 0);

    EXPECT_FALSE(db_lock_container("t_item", "chest1", 1, cur));  // 已锁冲突
    EXPECT_EQ(cur, 1);

    ASSERT_TRUE(db_lock_container("t_item", "chest1", 0, cur));
    EXPECT_EQ(cur, 1);
}

// ====================================================================
// 账号 ID 起点（1000）与巫师账号
// ====================================================================

// 首次建表时播种账号自增起点 1000（1~999 预留给巫师/系统账号）
TEST_F(SqliteIntegrationTest, PrepareTables_SeedsIdFloorOnFreshDb)
{
    ASSERT_TRUE(prepare_tables()) << db->error();

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    ASSERT_EQ(id, 1000) << err;  // 起点播种后首个注册账号从 1000 开始
}

// 模拟重启：jianghudb 已存在时重跑 prepare_tables 不得重置起点，
// 否则新账号会撞上已发的 cc_id（主键冲突 → 注册失败）
TEST_F(SqliteIntegrationTest, PrepareTables_ReRunKeepsIdFloor)
{
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
// 不得干扰自增起点——巫师占 1/2，玩家仍从 1000 开始
TEST_F(SqliteIntegrationTest, PrepareTables_FloorCoexistsWithWizardAccounts)
{
    ASSERT_TRUE(prepare_tables()) << db->error();

    // 复刻 ensure_wizard_accounts 的显式插入
    ASSERT_EQ(
        db->execute("insert into jianghudb (cc_id, cc_name, cc_passwd, "
                    "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                    "values (1, '巫师一号', 'pw', 21, 21, 21, 21, '男性', "
                    "100)"),
        0)
        << db->error();
    ASSERT_EQ(
        db->execute("insert into jianghudb (cc_id, cc_name, cc_passwd, "
                    "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                    "values (2, '巫师二号', 'pw', 21, 21, 21, 21, '女性', "
                    "100)"),
        0)
        << db->error();

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    EXPECT_EQ(id, 1000) << err;  // 显式插入 1/2 不下压 seq，玩家仍是 1000
}

// ====================================================================
// 账号 / 社交
// ====================================================================

TEST_F(SqliteIntegrationTest, AccountRegistrationAndPasswordRoundTrip)
{
    ASSERT_TRUE(prepare_tables());

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    ASSERT_EQ(id, 1000) << err;  // prepare_tables 首建已播种起点 1000

    const std::string sid = std::to_string(id);

    EXPECT_EQ(db_query_server(sid.c_str()), 0);
    ASSERT_TRUE(db_set_online_flag(sid.c_str(), 3));
    EXPECT_EQ(db_query_server(sid.c_str()), 3);

    EXPECT_EQ(db_query_freeze(sid.c_str()), 0);
    ASSERT_TRUE(db_set_freeze(sid.c_str(), 1));
    EXPECT_EQ(db_query_freeze(sid.c_str()), 1);

    ASSERT_TRUE(db_change_password(sid.c_str(), "newpass1"));

    // 暗码同样以盐+哈希落库；历史明文行在校验通过时迁移（无 OLD_PASSWORD）
    ASSERT_TRUE(db_change_verify_password(sid.c_str(), "secret"));
    EXPECT_TRUE(db_verify_password(sid.c_str(), "secret"));
    EXPECT_FALSE(db_verify_password(sid.c_str(), "wrong"));
}

// 主密码：注册即哈希落库，登录校验走哈希分支，改密后旧口令立即失效
TEST_F(SqliteIntegrationTest, AuthenticateAccountHashRoundTrip)
{
    ASSERT_TRUE(prepare_tables());

    char err[256] = "";
    long id = db_register_account("张三", "pass1234", "zhangsan@example.com",
                                  "男性", 21, 21, 21, 21, err, sizeof(err));
    ASSERT_EQ(id, 1000) << err;
    const std::string sid = std::to_string(id);

    DbResultSet rs;
    ASSERT_EQ(0, db->query(("select cc_passwd, cc_passwd_hash from jianghudb "
                            "where cc_id="
                            + sid)
                               .c_str(),
                           rs))
        << db->error();
    ASSERT_EQ(rs.size(), 1);
    EXPECT_TRUE(rs.rows[0][0].empty()) << "明文列不写值";
    EXPECT_FALSE(rs.rows[0][1].empty()) << "注册即写入哈希";

    EXPECT_TRUE(db_authenticate_account(sid.c_str(), "pass1234"));
    EXPECT_FALSE(db_authenticate_account(sid.c_str(), "wrongpass"));

    ASSERT_TRUE(db_change_password(sid.c_str(), "newpass1"));
    EXPECT_TRUE(db_authenticate_account(sid.c_str(), "newpass1"));
    EXPECT_FALSE(db_authenticate_account(sid.c_str(), "pass1234"))
        << "改密后旧口令必须失效";
}

// 历史明文行：首次校验通过后原地迁移（明文列清空、盐与哈希写入），
// 迁移后空口令不得再放行
TEST_F(SqliteIntegrationTest, LegacyPlaintextMigratesOnFirstLogin)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_EQ(0, db->execute("insert into jianghudb (cc_id, cc_name, "
                             "cc_passwd) values (42, '老号', 'legacy1')"));

    EXPECT_TRUE(db_authenticate_account("42", "legacy1"));
    EXPECT_FALSE(db_authenticate_account("42", "wrong"));

    DbResultSet rs;
    ASSERT_EQ(0, db->query("select cc_passwd, cc_passwd_salt, "
                           "cc_passwd_hash from jianghudb where cc_id=42",
                           rs))
        << db->error();
    ASSERT_EQ(rs.size(), 1);
    EXPECT_TRUE(rs.rows[0][0].empty()) << "迁移后明文列清空";
    EXPECT_FALSE(rs.rows[0][1].empty());
    EXPECT_FALSE(rs.rows[0][2].empty());

    EXPECT_TRUE(db_authenticate_account("42", "legacy1")) << "迁移后照常登录";
    EXPECT_FALSE(db_authenticate_account("42", ""))
        << "空口令不得顶替真口令（明文列已为空）";
}

// 暗码：历史行迁移 + 「空暗码校验」语义（未设暗码的账号空入参通过）
TEST_F(SqliteIntegrationTest, LegacyVerifyPasswordMigratesAndKeepsEmptyCheck)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_EQ(0, db->execute("insert into jianghudb (cc_id, cc_name, "
                             "cc_verifypass) values (43, '暗码号', "
                             "'legacysecret')"));

    EXPECT_TRUE(db_verify_password("43", "legacysecret"));
    EXPECT_FALSE(db_verify_password("43", "wrong"));

    DbResultSet rs;
    ASSERT_EQ(0, db->query("select cc_verifypass, cc_verifypass_salt, "
                           "cc_verifypass_hash from jianghudb where cc_id=43",
                           rs))
        << db->error();
    ASSERT_EQ(rs.size(), 1);
    EXPECT_TRUE(rs.rows[0][0].empty()) << "迁移后旧列清空";
    EXPECT_FALSE(rs.rows[0][1].empty());
    EXPECT_FALSE(rs.rows[0][2].empty());

    // 未设置暗码的账号：空入参通过（新号首次设暗码前的口令确认依赖它）
    ASSERT_EQ(0, db->execute("insert into jianghudb (cc_id, cc_name) "
                             "values (44, '无暗码')"));
    EXPECT_TRUE(db_verify_password("44", ""));
    EXPECT_FALSE(db_verify_password("44", "anything"));
}

TEST_F(SqliteIntegrationTest, RecruitRoundTrip)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_TRUE(db_update_recruit("1", "2", "弟子甲", 100, 1));  // insert

    DbResultSet rs;
    ASSERT_TRUE(db_query_recruit("1", rs));
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs.rows[0][1], "弟子甲");

    ASSERT_TRUE(db_update_recruit("1", "2", "弟子乙", 200, 0));  // update
    ASSERT_TRUE(db_query_recruit("1", rs));
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs.rows[0][1], "弟子乙");
}

TEST_F(SqliteIntegrationTest, RelativeRoundTrip)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_TRUE(db_update_relative("1", "2", 500, 1));  // insert

    long pt = 0;
    int rel = -1;
    ASSERT_TRUE(db_query_relative("1", "2", pt, rel));
    EXPECT_EQ(pt, 500);
    EXPECT_EQ(rel, 1);

    ASSERT_TRUE(db_update_relative("1", "2", 700, -1));  // update，保留关系
    ASSERT_TRUE(db_query_relative("1", "2", pt, rel));
    EXPECT_EQ(pt, 700);
    EXPECT_EQ(rel, 1);
}

// ====================================================================
// 排行榜 / 记录
// ====================================================================

TEST_F(SqliteIntegrationTest, RecordRoundTrip)
{
    ASSERT_TRUE(prepare_tables());

    ASSERT_TRUE(sql_update_record("1", 1, 10, "dataA", false));  // insert
    ASSERT_TRUE(sql_update_record("1", 1, 20, "dataB", true));   // update

    DbResultSet rs;
    ASSERT_EQ(db->query("select cc_level from t_record where cc_id=1", rs), 0);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs.rows[0][0], "20");
}

TEST_F(SqliteIntegrationTest, PaihangbangReadsSeededRows)
{
    ASSERT_TRUE(prepare_tables());
    ASSERT_EQ(db->execute("insert into jianghudb "
                          "(cc_id, cc_name, cc_family, cc_experice, "
                          "cc_freeze, cc_grant) "
                          "values (5, '高手', '华山', 600000, 0, 30)"),
              0)
        << db->error();

    CContainer me;
    EXPECT_EQ(paihangbang(&me), 1);
}
