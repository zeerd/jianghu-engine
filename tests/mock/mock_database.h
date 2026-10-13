// mock_database.h — 实现 IDatabase 的内存版 Mock，供 db 层测试共享
//
// 通过记录执行过的 SQL、预设 query() 结果集与错误码，
// 让真实 db 业务代码（sql_*.cpp、DbTables.cpp）在无 MySQL 环境下可测。
#ifndef SERVERTEST_MOCK_DATABASE_H
#define SERVERTEST_MOCK_DATABASE_H

#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "IDatabase.h"

class MockDatabase : public IDatabase {
public:
    MockDatabase() {}

    int connect(const char*, const char*, const char*, const char*, int,
                const char*) override
    {
        return m_shouldFail ? 1 : 0;
    }

    int execute(const char* sql) override
    {
        if (m_shouldFail) {
            m_last_error = "mock execute failure";
            return 1;
        }
        m_lastSql = sql;
        m_executedSqls.push_back(sql);

        // 指定序号的 execute() 失败（0 起），其余成功
        if (m_executes++ == m_executeFailAt) {
            m_last_error = "mock execute failure";
            return 1;
        }

        return m_executeErrorCode;
    }

    int query(const char* sql, DbResultSet& result) override
    {
        if (m_shouldFail) {
            m_last_error = "mock query failure";
            return 1;
        }
        m_lastSql = sql;
        m_executedSqls.push_back(sql);
        result.clear();

        if (m_queryResults.count(m_lastSql)) {
            result = m_queryResults[m_lastSql];
            return m_queryErrorCode;
        }

        // 按子串匹配：调用方无需逐字节复制生产侧的 SQL 格式
        for (const auto& preset : m_queryPatternResults) {
            if (m_lastSql.find(preset.first) != std::string::npos) {
                result = preset.second;
                return m_queryErrorCode;
            }
        }

        // 默认：空结果集
        return m_queryErrorCode;
    }

    int create_database(const char*, const char*) override { return 0; }
    int select_database(const char*) override { return 0; }
    int last_rows() const override { return 0; }
    const char* error() const override { return m_last_error.c_str(); }
    void close() override {}

    // --- 测试辅助方法 ---

    // 预设 query() 结果：SQL 与 key 完全一致时命中
    void SetQueryResult(const std::string& sql, DbResultSet result)
    {
        m_queryResults[sql] = result;
    }

    // 预设 query() 结果：SQL 含 pattern 子串时命中，按添加顺序匹配
    void SetQueryResultMatching(const std::string& pattern, DbResultSet result)
    {
        m_queryPatternResults.emplace_back(pattern, std::move(result));
    }

    // 构造一个单行结果
    static DbResultSet MakeSingleRow(std::initializer_list<std::string> cols)
    {
        DbResultSet rs;
        DbRow row;
        for (auto& c : cols) row.columns.push_back(c);
        rs.rows.push_back(row);
        return rs;
    }

    // 最后执行的 SQL
    std::string GetLastSql() const { return m_lastSql; }

    // 按执行顺序记录的全部 SQL
    const std::vector<std::string>& GetExecutedSqls() const
    {
        return m_executedSqls;
    }

    // 是否执行过包含指定子串的语句
    bool ExecutedContaining(const std::string& needle) const
    {
        for (const auto& sql : m_executedSqls) {
            if (sql.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    // 所有 DB 操作一律失败
    void SetShouldFail(bool fail) { m_shouldFail = fail; }

    // query() 返回的错误码（但仍填充 result），用于 query() ? 0 : 1 的反转逻辑
    void SetQueryErrorCode(int code) { m_queryErrorCode = code; }

    // execute() 返回的错误码
    void SetExecuteErrorCode(int code) { m_executeErrorCode = code; }

    // 让第 index 次 execute() 失败（0 起），其余成功
    void SetExecuteFailAt(int index) { m_executeFailAt = index; }

    // 清除全部预设与记录
    void Clear()
    {
        m_queryResults.clear();
        m_queryPatternResults.clear();
        m_executedSqls.clear();
        m_lastSql.clear();
        m_last_error.clear();
        m_shouldFail       = false;
        m_queryErrorCode   = 0;
        m_executeErrorCode = 0;
        m_executeFailAt    = -1;
        m_executes         = 0;
    }

private:
    std::string m_lastSql;
    std::string m_last_error;
    bool m_shouldFail      = false;
    int m_queryErrorCode   = 0;
    int m_executeErrorCode = 0;
    int m_executeFailAt    = -1;
    int m_executes         = 0;
    std::map<std::string, DbResultSet> m_queryResults;
    std::vector<std::pair<std::string, DbResultSet>> m_queryPatternResults;
    std::vector<std::string> m_executedSqls;
};

// 按需抛异常的 DB：验证 db 层异常不得穿透到游戏逻辑
class ThrowingDatabase : public IDatabase {
public:
    bool throwOnExecute    = false;
    bool throwOnQuery      = false;
    int executeBeforeThrow = 0;  // 前 N 次 execute 正常返回

    void Reset()
    {
        throwOnExecute     = false;
        throwOnQuery       = false;
        executeBeforeThrow = 0;
        m_executes         = 0;
    }

    int connect(const char*, const char*, const char*, const char*, int,
                const char*) override
    {
        return 0;
    }

    int execute(const char*) override
    {
        const bool mustThrow
            = throwOnExecute && m_executes >= executeBeforeThrow;
        m_executes++;
        if (mustThrow) throw std::runtime_error("db execute");
        return 0;
    }

    int query(const char*, DbResultSet&) override
    {
        if (throwOnQuery) throw std::runtime_error("db query");
        return 0;
    }

    int create_database(const char*, const char*) override { return 0; }
    int select_database(const char*) override { return 0; }
    int last_rows() const override { return 0; }
    const char* error() const override { return "throwing"; }
    void close() override {}

private:
    int m_executes = 0;
};

#endif  // SERVERTEST_MOCK_DATABASE_H
