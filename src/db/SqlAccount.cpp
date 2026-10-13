/**
 * @file SqlAccount.cpp
 * @brief 用户账号管理 + 频道/社交操作  */

#include <stdio.h>

#include <string>

#include "Global.h"
#include "IDatabase.h"
#include "Sql.h"

// ============================================================
// 用户/账号操作 (对应 Login.cpp, User.cpp, Char.cpp)
// ============================================================

bool db_verify_password(const char* id, const char* pass)
{
    if (!g_db) return false;

    try {
        char msg[512], tmp[80];
        strncpy(tmp, pass, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        // 简单的转义处理
        for (char* p = tmp; *p; ++p) {
            if (*p == '\'') *p = ' ';
        }

        DbResultSet result;
        snprintf(
            msg, sizeof(msg),
            "select cc_name from jianghudb where cc_id=%s && cc_verifypass = "
            "OLD_PASSWORD('%s') ",
            id, tmp);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_verify_password: query db fail! %s", g_db->error());
            return false;
        }

        if (result.empty()) {
            return false;
        }

        return result.size() > 0;
    } catch (...) {
        LOG_ERROR("db_verify_password: unknown exception");
        return false;
    }
}

bool db_change_password(const char* id, const char* new_pass)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_passwd='%s' where cc_id=%s", new_pass,
                 id);
        int ret = g_db->execute(msg);
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_change_password: unknown exception");
        return false;
    }
}

bool db_change_verify_password(const char* id, const char* new_pass)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(
            msg, sizeof(msg),
            "update jianghudb set cc_verifypass = OLD_PASSWORD('%s') where "
            "cc_id=%s",
            new_pass, id);
        int ret = g_db->execute(msg);
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_change_verify_password: unknown exception");
        return false;
    }
}

bool db_set_online_flag(const char* id, int online)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_server = %d where cc_id = %s", online,
                 id);
        int ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_set_online_flag: query db fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_set_online_flag: unknown exception");
        return false;
    }
}

int db_query_server(const char* id)
{
    if (!g_db) return -1;

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_server from jianghudb where cc_id = %s", id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_server: query db fail! %s", g_db->error());
            return -1;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return -1;
        }

        return atoi(result.rows[0][0].c_str());
    } catch (...) {
        LOG_ERROR("db_query_server: unknown exception");
        return -1;
    }
}

int db_query_freeze(const char* id)
{
    if (!g_db) return 1;  // 默认冻结

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_freeze from jianghudb where cc_id=%s", id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_freeze: query db fail! %s", g_db->error());
            return 1;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return 1;
        }

        return atoi(result.rows[0][0].c_str());
    } catch (...) {
        LOG_ERROR("db_query_freeze: unknown exception");
        return 1;
    }
}

bool db_set_freeze(const char* id, int freeze)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_freeze = %d where cc_id=%s", freeze,
                 id);
        int ret = g_db->execute(msg);
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_set_freeze: unknown exception");
        return false;
    }
}

// ============================================================
// 频道/社交操作 (对应 Channel.cpp)
// ============================================================

bool db_query_recruit(const char* master_id, DbResultSet& result)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "select re_id, re_name, re_exp, re_apprentice from "
                 "recruit_exp where re_master=%s",
                 master_id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_recruit: query db fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_query_recruit: unknown exception");
        return false;
    }
}

bool db_update_recruit(const char* master, const char* id, const char* name,
                       long exp, int apprentice)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        snprintf(
            msg, sizeof(msg),
            "select re_exp from recruit_exp where re_master=%s and re_id=%s",
            master, id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_update_recruit: query db fail! %s", g_db->error());
            return false;
        }

        int n = result.size();
        if (n) {
            snprintf(msg, sizeof(msg),
                     "update recruit_exp set re_name='%s', re_exp=%ld, "
                     "re_apprentice=%d where re_master=%s and re_id=%s",
                     name, exp, apprentice, master, id);
        }
        else {
            snprintf(msg, sizeof(msg),
                     "insert recruit_exp(re_master, re_id, re_name, re_exp, "
                     "re_apprentice) values(%s, %s, '%s', %ld, %d)",
                     master, id, name, exp, apprentice);
        }

        ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_update_recruit: execute fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_update_recruit: unknown exception");
        return false;
    }
}

bool db_query_relative(const char* me, const char* target, long& out_point,
                       int& out_relative)
{
    if (!g_db) return false;

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_point, cc_relative from relative where cc_id=%s "
                 "and cc_other=%s",
                 me, target);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_relative: query db fail! %s", g_db->error());
            return false;
        }

        if (result.empty() || result.rows[0].columns.size() < 2) {
            return false;
        }

        out_point    = atol(result.rows[0][0].c_str());
        out_relative = atoi(result.rows[0][1].c_str());
        return true;
    } catch (...) {
        LOG_ERROR("db_query_relative: unknown exception");
        return false;
    }
}

bool db_update_relative(const char* me, const char* target, long pt, int flag)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        snprintf(msg, sizeof(msg),
                 "select cc_point from relative where cc_id=%s and cc_other=%s",
                 me, target);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_update_relative: query db fail! %s", g_db->error());
            return false;
        }

        int n = result.size();
        if (flag == -1) {  // 不修改关系
            if (n) {
                snprintf(msg, sizeof(msg),
                         "update relative set cc_point=%ld where cc_id=%s and "
                         "cc_other=%s",
                         pt, me, target);
            }
            else {
                snprintf(msg, sizeof(msg),
                         "insert relative(cc_id, cc_other, cc_point, "
                         "cc_relative) values(%s, %s, %ld, 0)",
                         me, target, pt);
            }
        }
        else {
            if (n) {
                snprintf(msg, sizeof(msg),
                         "update relative set cc_point=%ld, cc_relative=%d "
                         "where cc_id=%s and cc_other=%s",
                         pt, flag, me, target);
            }
            else {
                snprintf(msg, sizeof(msg),
                         "insert relative(cc_id, cc_other, cc_point, "
                         "cc_relative) values(%s, %s, %ld, %d)",
                         me, target, pt, flag);
            }
        }

        ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_update_relative: execute fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_update_relative: unknown exception");
        return false;
    }
}
