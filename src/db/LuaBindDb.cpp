/**
 * @file LuaBindDb.cpp
 * @brief Lua binds moved from LuaBind.cpp. */

#include "LuaBindIncludes.h"

#include <algorithm>
#include <string>

// 全局：db_execute(sql) -> boolean - 执行 DDL/DML 语句

int l_db_execute(lua_State* L)

{
    const char* sql = luaL_checkstring(L, 1);

    int ret_code = g_db->execute(sql);

    if (ret_code != 0) {
        lua_pushboolean(L, 0);

        lua_pushstring(L, g_db->error());

        return 2;
    }

    lua_pushboolean(L, 1);

    return 1;
}

// 全局：get_db_type() -> string - 数据库类型（"mysql"/"sqlite"/"none"）
// 脚本据此选择方言相关的 DDL（如 card_core.init_table 建卡表）
int l_get_db_type(lua_State* L)

{
    lua_pushstring(L, get_db_type());

    return 1;
}

// 全局：db_register_ddl(sql) - 登记一条数据集专用建表语句
// 开机 prepare_tables 末尾统一执行；C++ 不认识具体表名，DDL 内容全归脚本。
// 同一条 DDL 重复登记视为无效（脚本热重载会重新执行注册代码）。
int l_db_register_ddl(lua_State* L)

{
    const char* sql = luaL_checkstring(L, 1);

    const std::string ddl = sql;
    if (std::find(g_dataset_table_ddl.begin(), g_dataset_table_ddl.end(), ddl)
        == g_dataset_table_ddl.end()) {
        g_dataset_table_ddl.push_back(ddl);
    }

    return 0;
}

// 全局：db_query(sql) -> table|nil, error - 执行 SELECT 查询

// 返回 {{col1, col2, ...}, ...} 或 nil, error_string

int l_db_query(lua_State* L)

{
    const char* sql = luaL_checkstring(L, 1);

    DbResultSet result;

    int ret_code = g_db->query(sql, result);

    if (ret_code != 0) {
        lua_pushnil(L);

        lua_pushstring(L, g_db->error());

        return 2;
    }

    // 构建 Lua table: {{col1, col2, ...}, ...}

    lua_newtable(L);

    for (size_t i = 0; i < result.rows.size(); i++) {
        const DbRow& row = result.rows[i];

        lua_newtable(L);

        for (size_t j = 0; j < row.columns.size(); j++) {
            lua_pushlstring(L, row[j].c_str(), row[j].size());

            lua_rawseti(L, -2, j + 1);
        }

        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}
