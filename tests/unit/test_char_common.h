// test_char_common.h — Shared helpers for CChar split-file unit tests.
//
// Provides a lightweight in-memory IDatabase mock so that the Char*.cpp
// functions which touch g_db (SaveToServer / LoadFromServer / object save)
// can be exercised without a real MySQL connection.
//
#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "IDatabase.h"

// ------------------------------------------------------------------
// CharMockDb — minimal IDatabase returning canned query results.
// ------------------------------------------------------------------
class CharMockDb : public IDatabase {
public:
    int connect(const char*, const char*, const char*, const char*, int,
                const char*) override
    {
        return 0;
    }
    int execute(const char* sql) override
    {
        m_lastSql = sql ? sql : "";
        return m_execFail ? 1 : 0;
    }
    int query(const char* sql, DbResultSet& result) override
    {
        m_lastSql = sql ? sql : "";
        result.clear();
        if (m_queryFail) return 1;
        auto it = m_results.find(m_lastSql);
        if (it != m_results.end()) {
            result = it->second;
        }
        return 0;
    }
    int create_database(const char*, const char*) override { return 0; }
    int select_database(const char*) override { return 0; }
    int last_rows() const override { return 0; }
    const char* error() const override { return "mock"; }
    void close() override {}

    // --- test controls ---
    void SetResult(const std::string& sql, DbResultSet rs)
    {
        m_results[sql] = rs;
    }
    void SetExecFail(bool f) { m_execFail = f; }
    void SetQueryFail(bool f) { m_queryFail = f; }
    std::string LastSql() const { return m_lastSql; }

    static DbResultSet SingleRow(std::initializer_list<std::string> cols)
    {
        DbResultSet rs;
        DbRow row;
        for (auto& c : cols) row.columns.push_back(c);
        rs.rows.push_back(row);
        return rs;
    }

private:
    std::map<std::string, DbResultSet> m_results;
    std::string m_lastSql;
    bool m_execFail  = false;
    bool m_queryFail = false;
};
