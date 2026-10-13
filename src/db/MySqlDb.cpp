#include <windows.h>

#include <string>
#include "Global.h"
#include "IDatabase.h"
#include "mysql.h"
#include "mysqld_error.h"

// MySQL 后端实现
class CMySqlDb : public IDatabase {
public:
    CMySqlDb() : m_mysql(nullptr), m_last_error(""), m_rows(0) {}
    ~CMySqlDb() override { close(); }

    int connect(const char* host, const char* user, const char* pass,
                const char* db, int port = 0,
                const char* socket = nullptr) override
    {
        if (m_mysql) mysql_close(m_mysql);

        m_mysql = mysql_init(nullptr);
        if (!m_mysql) {
            m_last_error = "mysql_init failed";
            return 1;
        }

        unsigned int timeout = 5;
        mysql_options(m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

        if (!mysql_real_connect(m_mysql, host, user, pass, db, port, socket,
                                0)) {
            m_last_error = mysql_error(m_mysql);
            return 1;
        }

        m_rows = 0;
        return 0;
    }

    int execute(const char* sql) override
    {
        int rc = mysql_real_query(m_mysql, sql, (unsigned long)strlen(sql));
        if (rc != 0) {
            m_last_error = mysql_error(m_mysql);
            return rc;
        }
        m_rows = (int)mysql_affected_rows(m_mysql);
        return 0;
    }

    int query(const char* sql, DbResultSet& result) override
    {
        result.clear();

        int rc = mysql_real_query(m_mysql, sql, (unsigned long)strlen(sql));
        if (rc != 0) {
            m_last_error = mysql_error(m_mysql);
            return rc;
        }

        MYSQL_RES* res = mysql_store_result(m_mysql);
        if (res == nullptr) {
            // 没有结果集返回（INSERT/UPDATE/DELETE）
            m_rows = 0;
            return 0;
        }

        MYSQL_ROW row;
        unsigned int ncols = mysql_num_fields(res);
        m_rows             = 0;

        while ((row = mysql_fetch_row(res)) != nullptr) {
            DbRow dbRow;
            for (unsigned int i = 0; i < ncols; i++) {
                if (row[i]) {
                    dbRow.columns.push_back(row[i]);
                }
                else {
                    dbRow.columns.push_back("");
                }
            }
            result.rows.push_back(dbRow);
            m_rows++;
        }

        mysql_free_result(res);
        return 0;
    }

    int create_database(const char* dbname,
                        const char* charset = "utf8") override
    {
        std::string sql = "CREATE DATABASE IF NOT EXISTS " + std::string(dbname)
                          + " DEFAULT CHARACTER SET " + std::string(charset);
        return execute(sql.c_str());
    }

    int select_database(const char* dbname) override
    {
        if (mysql_select_db(m_mysql, dbname) != 0) {
            m_last_error = mysql_error(m_mysql);
            return 1;
        }
        return 0;
    }

    int last_rows() const override { return m_rows; }

    const char* error() const override
    {
        return m_mysql ? mysql_error(m_mysql) : m_last_error.c_str();
    }

    void close() override
    {
        if (m_mysql) {
            mysql_close(m_mysql);
            m_mysql = nullptr;
        }
        m_last_error.clear();
        m_rows = 0;
    }

private:
    MYSQL* m_mysql;
    std::string m_last_error;
    int m_rows;
};

// MySQL 工厂函数
std::unique_ptr<IDatabase> create_my_sql_db()
{
    LOG_DEBUG("Using Mysql...");
    return std::make_unique<CMySqlDb>();
}
