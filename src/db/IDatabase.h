/**
 * @file IDatabase.h
 * @brief 数据库抽象接口层，支持 MySQL / SQLite 后端切换。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * @brief 单行查询结果，以列索引访问。
 */
struct DbRow {
    std::vector<std::string> columns;

    const std::string& operator[](int idx) const
    {
        static const std::string empty = "";
        if (idx < 0 || idx >= (int)columns.size()) return empty;
        return columns[idx];
    }
};

/**
 * @brief 查询结果集，包含多行 DbRow。
 */
struct DbResultSet {
    std::vector<DbRow> rows;

    int size() const { return (int)rows.size(); }
    bool empty() const { return rows.empty(); }

    void clear() { rows.clear(); }
};

/**
 * @brief 数据库抽象接口，定义连接/执行/查询/工厂方法。
 */
class IDatabase {
public:
    virtual ~IDatabase() = default;

    // 连接数据库
    // Returns: 0=success, non-zero=error code
    virtual int connect(const char* host, const char* user, const char* pass,
                        const char* db, int port = 0,
                        const char* socket = nullptr) = 0;

    // 执行DDL/DML（无结果集）
    // Returns: 0=success, non-zero=error code
    virtual int execute(const char* sql) = 0;

    // 执行SELECT查询
    // Returns: 0=success, non-zero=error code
    virtual int query(const char* sql, DbResultSet& result) = 0;

    // 创建表（从DDL语句）
    // Returns: 0=success, non-zero=error code
    virtual int create_table(const char* ddl) { return execute(ddl); }

    // 创建数据库
    // Returns: 0=success, non-zero=error code
    virtual int create_database(const char* dbname,
                                const char* charset = "utf8") = 0;

    // 选择数据库
    // Returns: 0=success, non-zero=error code
    virtual int select_database(const char* dbname) = 0;

    // 后端方言标识：SQLite 与 MySQL 的建表语法、内置函数不同，拼 SQL 时需区分。
    virtual bool is_sqlite() const { return false; }

    // 设置字符集
    // Returns: 0=success, non-zero=error code
    virtual int set_charset(const char* charset)
    {
        return execute(("set names '" + std::string(charset) + "'").c_str());
    }

    // 获取结果行数
    virtual int last_rows() const = 0;

    // 获取最后错误信息
    virtual const char* error() const = 0;

    // 释放资源
    virtual void close() = 0;

    // 工厂方法
    static std::unique_ptr<IDatabase> create(const char* type = "mysql");
};

// MySQL 后端工厂函数
std::unique_ptr<IDatabase> create_my_sql_db();

// SQLite 后端工厂函数
std::unique_ptr<IDatabase> create_sqlite_db();

// 全局数据库实例（非拥有指针：生产环境由 Sql.cpp 的内部 owner 持有，
// 测试时可指向栈上/全局 mock 实现）
extern IDatabase* g_db;
