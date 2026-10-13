/**
 * @file SqlContainer.cpp
 * @brief 通用容器CRUD + 玩家物品 + 箱子操作  */

#include <stdio.h>

#include <string>
#include <vector>

#include "Global.h"
#include "IDatabase.h"
#include "Sql.h"
#include "SqlEscape.h"

// ============================================================
// 容器/物品/房间/NPC 存储操作 (对应 Container.cpp)
// ============================================================

bool db_save_container(const char* table, const char* obj_id,
                       const char* obj_name, const char* obj_data,
                       bool is_update)
{
    if (!g_db) return false;

    try {
        // 使用 std::string 动态构建 SQL，避免固定缓冲区截断大数据
        // obj_data 是 JSON 文本，含大量反斜杠与单引号；连接已设
        // NO_BACKSLASH_ESCAPES，故仅需对单引号翻倍即可安全入 blob。
        const std::string name_esc = sql_escape_literal(obj_name);
        const std::string data_esc = sql_escape_literal(obj_data);
        const std::string id_esc   = sql_escape_literal(obj_id);

        std::string msg;
        if (is_update) {
            msg = "update ";
            msg += table;
            msg += " set obj_name='";
            msg += name_esc;
            msg += "', obj_data='";
            msg += data_esc;
            msg += "' where obj_id='";
            msg += id_esc;
            msg += "'";
        }
        else {
            msg = "insert ";
            msg += table;
            msg += "(obj_id, obj_name, obj_data) values('";
            msg += id_esc;
            msg += "', '";
            msg += name_esc;
            msg += "', '";
            msg += data_esc;
            msg += "')";
        }

        int ret_code = g_db->execute(msg.c_str());
        if (ret_code != 0) {
            LOG_ERROR("db_save_container: Failed to execute: %s, Error: %s",
                      msg.c_str(), g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_save_container: unknown exception");
        return false;
    }
}

bool db_load_container(const char* table, const char* obj_id,
                       std::string& out_data)
{
    if (!g_db) return false;

    try {
        char msg[512];
        snprintf(msg, sizeof(msg), "select obj_data from %s where obj_id='%s'",
                 table, obj_id);
        DbResultSet result;
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_load_container: %s query db fail! %s", table,
                      g_db->error());
            return false;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return false;
        }

        out_data = result.rows[0][0];
        return true;
    } catch (...) {
        LOG_ERROR("db_load_container: unknown exception");
        return false;
    }
}

bool db_delete_container(const char* table, const char* obj_id)
{
    if (!g_db) return false;

    try {
        char msg[512];
        snprintf(msg, sizeof(msg), "delete from %s where obj_id='%s'", table,
                 obj_id);
        int ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_delete_container: query db fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_delete_container: unknown exception");
        return false;
    }
}

bool db_lock_container(const char* table, const char* obj_id, int locked,
                       int& out_current_lock)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        // 查询当前锁状态
        snprintf(msg, sizeof(msg), "select obj_lock from %s where obj_id='%s'",
                 table, obj_id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_lock_container: query db fail!");
            return false;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return false;
        }

        int haslock      = atoi(result.rows[0][0].c_str());
        out_current_lock = haslock;

        // 检查是否冲突
        if ((locked && haslock) || (!locked && !haslock)) {
            return false;
        }

        // 更新锁状态
        snprintf(msg, sizeof(msg),
                 "update %s set obj_lock = %d where obj_id='%s'", table, locked,
                 obj_id);
        ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_lock_container: Failed to execute: %s",
                      g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_lock_container: unknown exception");
        return false;
    }
}

// ============================================================
// 玩家物品存储操作
// ============================================================

bool db_save_object(const char* owner_id, long grid, const char* base_name,
                    const char* savedata)
{
    if (!g_db) return false;

    try {
        // 检查是否存在
        char query_buf[256];
        snprintf(
            query_buf, sizeof(query_buf),
            "select base_name from t_saveobj where owner=%s and grid = %ld",
            owner_id, grid);
        DbResultSet result;
        int ret_code = g_db->query(query_buf, result);
        if (ret_code != 0) {
            LOG_ERROR("db_save_object: query db fail! %s", g_db->error());
            return false;
        }

        bool is_update = !result.empty() && result.size() > 0;

        // 使用 std::string 动态构建 SQL，避免固定缓冲区截断大数据
        // savedata 为 JSON 文本，与 obj_data 同策略：仅翻倍单引号。
        const std::string base_esc = sql_escape_literal(base_name);
        const std::string data_esc = sql_escape_literal(savedata);
        std::string msg;
        if (is_update) {
            msg = "update t_saveobj set base_name='";
            msg += base_esc;
            msg += "', savedata='";
            msg += data_esc;
            msg += "' where owner = ";
            msg += owner_id;
            msg += " and grid = ";
            msg += std::to_string(grid);
        }
        else {
            msg = "insert t_saveobj(owner, grid, base_name, savedata) values(";
            msg += owner_id;
            msg += ", ";
            msg += std::to_string(grid);
            msg += ", '";
            msg += base_esc;
            msg += "', '";
            msg += data_esc;
            msg += "')";
        }

        ret_code = g_db->execute(msg.c_str());
        if (ret_code != 0) {
            LOG_ERROR("db_save_object: execute fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_save_object: unknown exception");
        return false;
    }
}

bool db_load_object(const char* owner_id, long grid, std::string& out_base_name,
                    std::string& out_savedata)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        snprintf(msg, sizeof(msg),
                 "select base_name, savedata from t_saveobj where owner =%s "
                 "and grid = %ld",
                 owner_id, grid);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_load_object: query db fail! %s", g_db->error());
            return false;
        }

        if (result.empty() || result.rows[0].columns.size() < 2) {
            return false;
        }

        out_base_name = result.rows[0][0];
        out_savedata  = result.rows[0][1];
        return true;
    } catch (...) {
        LOG_ERROR("db_load_object: unknown exception");
        return false;
    }
}

// ============================================================
// 箱子存储操作 (t_box) - 统一使用通用容器接口
// ============================================================

// 按房间前缀查询所有箱子（obj_id 格式为 roomName_boxNum）
bool db_load_room_boxes(const char* room_base_name,
                        std::vector<std::string>& out_data)
{
    if (!g_db) return false;

    try {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "select obj_data from t_box where obj_id LIKE '%s_%%' "
                 "order by obj_id",
                 room_base_name);
        DbResultSet result;
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_load_room_boxes: query db fail! %s", g_db->error());
            return false;
        }

        for (int i = 0; i < result.size(); i++) {
            if (!result.rows[i].columns.empty() && !result.rows[i][0].empty()) {
                out_data.push_back(result.rows[i][0]);
            }
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_load_room_boxes: unknown exception");
        return false;
    }
}
