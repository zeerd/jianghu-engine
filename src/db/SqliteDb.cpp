#include <windows.h>

#include <stdio.h>
#include <string.h>
#include "Global.h"
#include "../../3rd/sqlite/sqlite3.h"
#include "IDatabase.h"

// SQLite 后端实现
class CSqliteDb : public IDatabase {
public:
    CSqliteDb() : m_db(nullptr), m_last_error(""), m_rows(0) {}
    ~CSqliteDb() override { close(); }

    int connect(const char* host, const char* user, const char* pass,
                const char* db, int port = 0,
                const char* socket = nullptr) override
    {
        if (m_db) close();

        // SQLite 的 "host/user/pass" 被忽略，db 是文件路径
        // 如果 db 为空，使用内存数据库
        const char* dbPath = db ? db : ":memory:";

        int rc = sqlite3_open_v2(
            dbPath, &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        if (rc != SQLITE_OK) {
            m_last_error = sqlite3_errmsg(m_db);
            return rc;
        }

        // 设置文本编码为 UTF-8（默认）
        // SQLite 原生就是 UTF-8，不需要 set names

        return 0;
    }

    int execute(const char* sql) override
    {
        char* err = nullptr;
        int rc    = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            m_last_error = err ? err : "unknown error";
            sqlite3_free(err);
            return rc;
        }
        return 0;
    }

    int query(const char* sql, DbResultSet& result) override
    {
        result.clear();

        sqlite3_stmt* stmt = nullptr;
        int rc             = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            m_last_error = sqlite3_errmsg(m_db);
            return rc;
        }

        // 获取列数
        int ncols = sqlite3_column_count(stmt);
        m_rows    = 0;

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            DbRow row;
            for (int i = 0; i < ncols; i++) {
                const unsigned char* val = sqlite3_column_text(stmt, i);
                if (val) {
                    row.columns.push_back((const char*)val);
                }
                else {
                    row.columns.push_back("");
                }
            }
            result.rows.push_back(row);
            m_rows++;
        }

        if (rc != SQLITE_DONE) {
            m_last_error = sqlite3_errmsg(m_db);
            sqlite3_finalize(stmt);
            return rc;
        }

        sqlite3_finalize(stmt);
        return 0;
    }

    int create_database(const char* dbname,
                        const char* charset = "utf8") override
    {
        // SQLite 不需要创建数据库，文件即数据库
        // 这个操作在 connect 时已经隐式完成
        (void)dbname;
        (void)charset;
        return 0;
    }

    int select_database(const char* dbname) override
    {
        // SQLite 通过打开不同文件来切换数据库
        // 可以使用 ATTACH DATABASE 来实现类似功能
        if (!dbname || dbname[0] == '\0') return 1;

        std::string sql
            = "ATTACH DATABASE '" + std::string(dbname) + "' AS attached_db";
        return execute(sql.c_str());
    }

    bool is_sqlite() const override { return true; }

    int last_rows() const override { return m_rows; }

    const char* error() const override { return m_last_error.c_str(); }

    void close() override
    {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        m_last_error.clear();
        m_rows = 0;
    }

private:
    sqlite3* m_db;
    std::string m_last_error;
    int m_rows;
};

// 工厂方法实现
std::unique_ptr<IDatabase> IDatabase::create(const char* type)
{
    // 缺省与 "mysql" 都产出 MySQL 后端（由 MySqlDb.cpp 提供）
    if (!type || strcmp(type, "mysql") == 0) {
        return create_my_sql_db();
    }
    if (strcmp(type, "sqlite") == 0) {
        return std::make_unique<CSqliteDb>();
    }
    return nullptr;
}

// SQLite 专用工厂函数
std::unique_ptr<IDatabase> create_sqlite_db()
{
    LOG_DEBUG("Using Sqlite3...");
    return std::make_unique<CSqliteDb>();
}
