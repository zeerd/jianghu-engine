/**
 * @file SqlAccount.cpp
 * @brief 用户账号管理 + 频道/社交操作  */

// windows.h 必须最先包含：Global.h 有 using namespace std;，
// 后置引入 windows.h(rpcndr.h 的 byte)会与 std::byte 撞成 C2872。
#include <windows.h>

#include <stdio.h>

#include <string>

#include "Global.h"
#include "IDatabase.h"
#include "Sql.h"
#include "SqlEscape.h"
#include "LoginRegister.h"

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
        // SQLite 无 OLD_PASSWORD()：cc_verifypass 由应用层按明文存取并等值比较
        // （见 db_change_verify_password）；MySQL 沿用服务端 OLD_PASSWORD
        // 哈希。
        if (g_db->is_sqlite()) {
            snprintf(msg, sizeof(msg),
                     "select cc_name from jianghudb where cc_id=%s and "
                     "cc_verifypass='%s'",
                     id, tmp);
        }
        else {
            snprintf(msg, sizeof(msg),
                     "select cc_name from jianghudb where cc_id=%s and "
                     "cc_verifypass=OLD_PASSWORD('%s')",
                     id, tmp);
        }
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

// 注册新账号：校验参数后向 jianghudb 插入一行基本资料（不含 cc_savedata，
// 正式档案由首次登录时 verify_user 的新人分支建立，与巫师账号自举同路径）。
// 成功返回新账号 id（>0）；失败返回 0，并把错误信息写入 err。
// 参数顺序：name, passwd, email, gender, str, intel, con, dex
long db_register_account(const char* name, const char* passwd,
                         const char* email, const char* gender, int str,
                         int intel, int con, int dex, char* err, int errlen)
{
    if (err && errlen > 0) err[0] = 0;

    if (!g_db) {
        if (err) snprintf(err, errlen, "数据库未就绪，请稍后再试。");
        return 0;
    }

    try {
        // 四维合法性：单项 10~30，总和 84（与 verify_user 一致）
        if (str < 10 || str > 30 || con < 10 || con > 30 || intel < 10
            || intel > 30 || dex < 10 || dex > 30) {
            if (err)
                snprintf(err, errlen, "天赋属性非法：单项须在 10 到 30 之间。");
            return 0;
        }
        if (str + con + intel + dex != 84) {
            if (err) snprintf(err, errlen, "天赋属性非法：四项总和必须为 84。");
            return 0;
        }

        // 名字校验（优先走 Lua 的 login.check_name）
        const char* name_err = check_legal_name(name);
        if (name_err && name_err[0]) {
            if (err) snprintf(err, errlen, "%s", name_err);
            return 0;
        }

        // 邮箱校验：简单格式检查（含 @ 且 @ 前后非空），不做查重
        if (!email || email[0] == '\0') {
            if (err) snprintf(err, errlen, "请填写邮箱地址。");
            return 0;
        }
        const char* at = strchr(email, '@');
        if (!at || at == email || at[1] == '\0') {
            if (err) snprintf(err, errlen, "邮箱格式不正确。");
            return 0;
        }

        // 密码：cc_passwd varchar(22)；登录流程会剔除单引号，此处同样剔除，
        // 保证存库密码与登录校验串一致。
        char pass[24];
        snprintf(pass, sizeof(pass), "%s", passwd ? passwd : "");
        size_t plen = strlen(pass);
        for (char* p = pass; *p; ++p) {
            if (*p == '\'') *p = ' ';
        }
        if (plen < 4 || plen > 22) {
            if (err)
                snprintf(err, errlen, "密码长度必须在 4 到 22 个字符之间。");
            return 0;
        }

        // 性别
        if (strcmp(gender, "男性") != 0 && strcmp(gender, "女性") != 0) {
            if (err) snprintf(err, errlen, "性别只能是男性或女性。");
            return 0;
        }

        // 注意：不再查重名字，允许同名不同账号；cc_id 由数据库 AUTO_INCREMENT
        // 自动分配
        std::string esc_name  = sql_escape_literal(name);
        std::string esc_email = sql_escape_literal(email);

        // 起手值列与 init_new_char 同源（NEW_CHAR_*）：在首次登录建档之前，
        // 账号行的年龄/等级也应是新角色的真实起手值，而不是列默认的 0。
        // 经验保持缺省 0；cc_savedata 仍按设计由首次登录的新人分支建立
        // （见 verify_user）。
        std::string sql
            = "insert into jianghudb (cc_name, cc_passwd, cc_email, cc_gender, "
              "cc_str, cc_int, cc_con, cc_dex, cc_age, cc_level) values ('"
              + esc_name + "', '" + sql_escape_literal(pass) + "', '"
              + esc_email + "', '" + sql_escape_literal(gender) + "', "
              + std::to_string(str) + ", " + std::to_string(intel) + ", "
              + std::to_string(con) + ", " + std::to_string(dex) + ", "
              + std::to_string(NEW_CHAR_AGE) + ", "
              + std::to_string(NEW_CHAR_LEVEL) + ")";
        if (g_db->execute(sql.c_str()) != 0) {
            LOG_ERROR("db_register_account: insert fail! %s", g_db->error());
            if (err) snprintf(err, errlen, "注册失败，请稍后再试。");
            return 0;
        }

        // 取回自增生成的 cc_id
        DbResultSet id_result;
        long next_id         = 0;
        const char* id_query = g_db->is_sqlite() ? "SELECT last_insert_rowid()"
                                                 : "SELECT LAST_INSERT_ID()";
        if (g_db->query(id_query, id_result) == 0 && !id_result.empty()
            && !id_result.rows[0].columns.empty()) {
            next_id = atol(id_result.rows[0][0].c_str());
        }
        else {
            if (err) snprintf(err, errlen, "注册失败，无法获取账号 ID。");
        }
        return next_id;
    } catch (...) {
        LOG_ERROR("db_register_account: unknown exception");
        if (err) snprintf(err, errlen, "注册失败，请稍后再试。");
        return 0;
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
        // SQLite 无 OLD_PASSWORD()：直接写入明文，由 db_verify_password
        // 等值比较
        if (g_db->is_sqlite()) {
            snprintf(msg, sizeof(msg),
                     "update jianghudb set cc_verifypass='%s' where cc_id=%s",
                     new_pass, id);
        }
        else {
            snprintf(msg, sizeof(msg),
                     "update jianghudb set cc_verifypass = OLD_PASSWORD('%s') "
                     "where cc_id=%s",
                     new_pass, id);
        }
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
                     "insert into recruit_exp(re_master, re_id, re_name, "
                     "re_exp, re_apprentice) values(%s, %s, '%s', %ld, %d)",
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
                         "insert into relative(cc_id, cc_other, cc_point, "
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
                         "insert into relative(cc_id, cc_other, cc_point, "
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

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// register_char(name, passwd, email, gender, str, int, con, dex) -> id, err
// 注册新账号：成功返回新账号 id 与空错误串；失败返回 0 与错误信息。
int l_register_char(lua_State* L)
{
    const char* name   = luaL_checkstring(L, 1);
    const char* passwd = luaL_checkstring(L, 2);
    const char* email  = luaL_checkstring(L, 3);
    const char* gender = luaL_checkstring(L, 4);
    int str            = (int)luaL_checkinteger(L, 5);
    int intel          = (int)luaL_checkinteger(L, 6);
    int con            = (int)luaL_checkinteger(L, 7);
    int dex            = (int)luaL_checkinteger(L, 8);

    char err[256];
    long id = db_register_account(name, passwd, email, gender, str, intel, con,
                                  dex, err, sizeof(err));
    lua_pushinteger(L, id);
    lua_pushstring(L, err);
    return 2;
}
