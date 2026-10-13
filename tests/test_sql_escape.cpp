// test_sql_escape.cpp — sql_escape_literal 与存盘 SQL 字面量转义。
//
// 背景：存盘把 get_buffer() 的 JSON 文本写进单引号 SQL 字面量。连接已设
// NO_BACKSLASH_ESCAPES（见 init_sql_server），反斜杠退化为普通字符，字面量内
// 唯一需要处理的是单引号。本文件锁定该契约，并覆盖此前从未被测试的
// db_save_container / db_save_object 转义路径。
//
// Build+Run: .\run-coverage.ps1

#include "pch.h"
#include <string>
#include "IDatabase.h"
#include "Mapping.h"
#include "Sql.h"
#include "SqlEscape.h"
#include "mock/mock_database.h"

extern IDatabase* g_db;

// ====================================================================
// sql_escape_literal — 纯函数
// ====================================================================

TEST(SqlEscape, EmptyAndNull)
{
    EXPECT_EQ(sql_escape_literal(""), "");
    EXPECT_EQ(sql_escape_literal(nullptr), "");
}

TEST(SqlEscape, PlainTextUnchanged)
{
    EXPECT_EQ(sql_escape_literal("abc 123 {}[]:/"), "abc 123 {}[]:/");
}

TEST(SqlEscape, DoublesSingleQuoteOnly)
{
    EXPECT_EQ(sql_escape_literal("it's"), "it''s");
    EXPECT_EQ(sql_escape_literal("'"), "''");
    EXPECT_EQ(sql_escape_literal("a'b'c"), "a''b''c");
}

TEST(SqlEscape, BackslashAndQuoteAreLiteral)
{
    // 反斜杠与双引号是 JSON 的正常内容，NO_BACKSLASH_ESCAPES 下原样透传。
    EXPECT_EQ(sql_escape_literal("a\\b"), "a\\b");
    EXPECT_EQ(sql_escape_literal("say \"hi\""), "say \"hi\"");
    EXPECT_EQ(sql_escape_literal("\\\"/"), "\\\"/");
}

TEST(SqlEscape, HighBytesPassthrough)
{
    // GBK 文本含尾字节 0x5C（'\'），必须原样保留。
    char gbk[4];
    gbk[0]          = static_cast<char>(0x81);
    gbk[1]          = static_cast<char>(0x5C);
    gbk[2]          = static_cast<char>(0xA0);
    gbk[3]          = 0;
    std::string out = sql_escape_literal(gbk);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(static_cast<unsigned char>(out[0]), 0x81);
    EXPECT_EQ(static_cast<unsigned char>(out[1]), 0x5C);
    EXPECT_EQ(static_cast<unsigned char>(out[2]), 0xA0);
}

// ====================================================================
// db_save_container / db_save_object — 转义接入
// ====================================================================

class DbEscapeTest : public ::testing::Test {
protected:
    MockDatabase* db = nullptr;
    void SetUp() override
    {
        db   = new MockDatabase();
        g_db = db;
    }
    void TearDown() override
    {
        g_db = nullptr;
        delete db;
        db = nullptr;
    }
};

TEST_F(DbEscapeTest, SaveContainer_DoublesQuoteInData)
{
    // obj_data 含单引号，必须被翻倍；双引号与反斜杠原样保留。
    ASSERT_TRUE(db_save_container("t_item", "id'1", "n'a", "a'b\"c\\d", true));
    std::string sql = db->GetLastSql();
    // obj_id 的单引号翻倍
    EXPECT_NE(sql.find("'id''1'"), std::string::npos) << sql;
    // obj_name 的单引号翻倍
    EXPECT_NE(sql.find("'n''a'"), std::string::npos) << sql;
    // obj_data：单引号翻倍，双引号与反斜杠原样
    EXPECT_NE(sql.find("'a''b\"c\\d'"), std::string::npos) << sql;
}

TEST_F(DbEscapeTest, SaveObject_DoublesQuoteInData)
{
    ASSERT_TRUE(db_save_object("1001", 0, "sw'ord", "s'a\"b"));
    std::string sql = db->GetLastSql();
    EXPECT_NE(sql.find("'sw''ord'"), std::string::npos) << sql;
    EXPECT_NE(sql.find("'s''a\"b'"), std::string::npos) << sql;
}

// ====================================================================
// 端到端：JSON 文本经转义写库、还原字面量后原样读回、再 load_buffer 还原
// ====================================================================

// 从 SQL 中截取 obj_data='<...>' 的 <...>，并把翻倍单引号折叠回单个，模拟
// MySQL 在 NO_BACKSLASH_ESCAPES 下把字面量存进 blob 的字节。
static std::string extract_obj_data(const std::string& sql)
{
    const std::string marker = "obj_data='";
    size_t b                 = sql.find(marker);
    if (b == std::string::npos) return std::string();
    b += marker.size();

    std::string out;
    for (size_t i = b; i < sql.size(); ++i) {
        if (sql[i] == '\'') {
            if (i + 1 < sql.size() && sql[i + 1] == '\'') {
                out += '\'';  // '' -> '
                ++i;
                continue;
            }
            break;  // 结束引号
        }
        out += sql[i];
    }
    return out;
}

TEST_F(DbEscapeTest, EndToEnd_JsonWithSpecialCharsSurvivesDb)
{
    // 值里有双引号、反斜杠、单引号；键里有斜杠。
    CMapping src;
    src.set("name", "zhang\"san");
    src.set("path/a", 42);
    src.set("note", "it's a \\ test");

    const std::string json = src.get_buffer();

    // UPDATE 形式含 obj_data='<...>' 标记，便于从 SQL 中截取值。
    ASSERT_TRUE(db_save_container("t_item", "obj1", "n1", json.c_str(), true));
    std::string sql = db->GetLastSql();

    // 还原出的 obj_data 必须与原始 JSON 逐字节相等（转义无损往返）。
    EXPECT_EQ(extract_obj_data(sql), json) << "sql=" << sql;

    // 读回的字节再 load_buffer，特殊字符无损。
    CMapping back;
    ASSERT_GT(back.load_buffer(extract_obj_data(sql).c_str()), 0L);
    EXPECT_STREQ(back.querystr("name"), "zhang\"san");
    EXPECT_EQ(back.query("path/a"), 42);
    EXPECT_STREQ(back.querystr("note"), "it's a \\ test");
}
