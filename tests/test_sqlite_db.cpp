// test_sqlite_db.cpp — CSqliteDb（真实 SQLite 后端）单元测试
//
// 守护契约：
//   - IDatabase::create("sqlite") / create_sqlite_db() 返回可用实现；
//   - connect/execute/query 的成功与错误路径（错误码 + error() 文案）；
//   - NULL 列读出为空串、last_rows() 行数、select_database 边界；
//   - close() 幂等且清空错误状态。
//
// 说明：SqliteDb.cpp 里 CSqliteDb 是私有类，只能经工厂拿到，
// 因此所有断言都经 IDatabase 接口进行（这也是生产的真实使用面）。

#include "pch.h"
#include <cstdio>
#include <string>
#include "IDatabase.h"

// 工厂：create("sqlite") 与 create_sqlite_db() 都应产出非空对象
TEST(SqliteDbTest, FactoryCreatesBackend)
{
    std::unique_ptr<IDatabase> db = IDatabase::create("sqlite");
    ASSERT_NE(db, nullptr);

    std::unique_ptr<IDatabase> db2 = create_sqlite_db();
    ASSERT_NE(db2, nullptr);
}

// create() 对非 sqlite 类型不返回 MySQL 实现（MySqlDb.cpp 未编入测试）
TEST(SqliteDbTest, FactoryUnknownTypeReturnsNull)
{
    std::unique_ptr<IDatabase> db = IDatabase::create("oracle");
    EXPECT_EQ(db, nullptr);
}

// db 传 nullptr → 使用内存库；连接/建表/插入/查询全链路成功
TEST(SqliteDbTest, ConnectMemoryExecuteQuery)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect("localhost", "user", "pass", nullptr), 0);

    EXPECT_EQ(db->execute("create table t(id integer, name text)"), 0);
    EXPECT_EQ(db->execute("insert into t values(1, 'alpha')"), 0);
    EXPECT_EQ(db->execute("insert into t values(2, 'beta')"), 0);

    DbResultSet rs;
    ASSERT_EQ(db->query("select id, name from t order by id", rs), 0);
    ASSERT_EQ(rs.size(), 2);
    EXPECT_EQ(rs.rows[0][0], "1");
    EXPECT_EQ(rs.rows[0][1], "alpha");
    EXPECT_EQ(rs.rows[1][1], "beta");
    EXPECT_EQ(db->last_rows(), 2);
    EXPECT_STREQ(db->error(), "");
}

// NULL 列读出为空串而不是崩溃/遗漏
TEST(SqliteDbTest, QueryNullColumnBecomesEmpty)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    ASSERT_EQ(db->execute("create table t(a, b)"), 0);
    ASSERT_EQ(db->execute("insert into t values(NULL, NULL)"), 0);

    DbResultSet rs;
    ASSERT_EQ(db->query("select a, b from t", rs), 0);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs.rows[0][0], "");
    EXPECT_EQ(rs.rows[0][1], "");
}

// 空结果集查询：size==0、last_rows()==0
TEST(SqliteDbTest, QueryEmptyResultSet)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    ASSERT_EQ(db->execute("create table t(a)"), 0);

    DbResultSet rs;
    ASSERT_EQ(db->query("select a from t", rs), 0);
    EXPECT_TRUE(rs.empty());
    EXPECT_EQ(db->last_rows(), 0);
}

// SQL 语法错误：execute 返回非 0 且 error() 非空
TEST(SqliteDbTest, ExecuteBadSqlReportsError)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);

    EXPECT_NE(db->execute("this is not sql"), 0);
    EXPECT_STRNE(db->error(), "");
}

// 未连接就执行：sqlite3_exec(nullptr) 返回错误而不是崩溃
TEST(SqliteDbTest, ExecuteWithoutConnectFails)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    EXPECT_NE(db->execute("select 1"), 0);
    EXPECT_STRNE(db->error(), "");
}

// 语法错误的 SELECT：prepare 失败返回非 0 并记录错误
TEST(SqliteDbTest, QueryBadSqlPrepareFails)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);

    DbResultSet rs;
    EXPECT_NE(db->query("select nope from missing", rs), 0);
    EXPECT_STRNE(db->error(), "");
}

// 重复 connect：第二次先关旧连接再开新连接，仍可用
TEST(SqliteDbTest, ReconnectClosesPrevious)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    ASSERT_EQ(db->execute("create table t(a)"), 0);

    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    // 新连接是全新的内存库，旧表不存在
    EXPECT_NE(db->execute("insert into t values(1)"), 0);
    // 重新建表后可用
    EXPECT_EQ(db->execute("create table t(a)"), 0);
    EXPECT_EQ(db->execute("insert into t values(1)"), 0);
}

// 打不开的文件路径（目录不存在）：connect 返回非 0 且 error() 非空
TEST(SqliteDbTest, ConnectBadPathFails)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    EXPECT_NE(
        db->connect(nullptr, nullptr, nullptr, "no_such_dir_xyz/db.sqlite"), 0);
    EXPECT_STRNE(db->error(), "");
}

// create_database 是 no-op，恒成功
TEST(SqliteDbTest, CreateDatabaseIsNoop)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    EXPECT_EQ(db->create_database("whatever"), 0);
}

// select_database 边界：nullptr/空串拒绝
TEST(SqliteDbTest, SelectDatabaseEmptyRejected)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(db->select_database(nullptr), 1);
    EXPECT_EQ(db->select_database(""), 1);
}

// select_database 正常路径：ATTACH 成功
TEST(SqliteDbTest, SelectDatabaseAttaches)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(db->select_database(":memory:"), 0);
}

// close() 幂等：重复 close 不崩，且清空错误/行数；close 后 execute 失败
TEST(SqliteDbTest, CloseIsIdempotentAndClearsState)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_EQ(db->connect(nullptr, nullptr, nullptr, nullptr), 0);
    ASSERT_NE(db->execute("bad sql"), 0);  // 置错误状态

    db->close();
    db->close();
    EXPECT_STREQ(db->error(), "");          // close 会清空错误状态
    EXPECT_NE(db->execute("select 1"), 0);  // 已关闭
}
