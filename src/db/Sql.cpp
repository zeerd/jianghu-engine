#include <windows.h>

#include <stdio.h>

#include "Combatd.h"
#include "Channel.h"
#include "Global.h"
#include "User.h"

#include "IDatabase.h"
#include "Sql.h"
#include "IniParser.h"

bool prepare_tables(void);
char* check_legal_name(const char* name);

// 全局数据库实例
IDatabase* g_db = nullptr;

long iTelLinHand;
double g_game_count;  // 游戏每分种消耗的点数

// 恢复数据
bool restore_data_table()
{
    try {
        char msg[255];
        DbResultSet result;

        snprintf(msg, 255, "select cc_id from jianghudb where cc_server =%d",
                 g_server_id);
        if (g_db->query(msg, result) != 0) {
            LOG_ERROR("Restore_Data:query db fail!");
            return false;
        }

        int n = result.size();

        LOG_INFO("restore users:%d", n);

        for (int i = 0; i < n; i++) {
            const DbRow& row = result.rows[i];
            if (row.columns.empty()) {
                LOG_ERROR("Restore_Data:database fail:row is NULL %ld.", i);
                continue;
            }

            // 账号
            CUser* me = add_user();
            me->set("id", row[0].c_str());

            LOG_INFO("restore user (%s) data!", row[0].c_str());

            if (!me->load()) {
                LOG_INFO("restore user (%s) data fail!", row[0].c_str());
            }
            else {
                me->save_to_server();
                me->set_temp("online", g_server_id);
            }

            destruct(me);
        }

        return true;
    } catch (...) {
        LOG_ERROR("Restore_Data:unknown!");
        return false;
    }
}

// 确保巫师账号存在（cc_id=1 巫师1，cc_id=2 巫师2）
bool ensure_wizard_accounts()
{
    try {
        DbResultSet result;

        // 查询已存在的巫师账号
        if (g_db->query("SELECT cc_id FROM jianghudb WHERE cc_id IN (1, 2)",
                        result)
            != 0) {
            LOG_ERROR("EnsureWizardAccounts: query db fail!");
            return false;
        }

        bool has_wizard1 = false;
        bool has_wizard2 = false;
        int n            = result.size();
        for (int i = 0; i < n; i++) {
            const DbRow& row = result.rows[i];
            if (row.columns.empty()) continue;
            int id = atoi(row[0].c_str());
            if (id == 1) has_wizard1 = true;
            if (id == 2) has_wizard2 = true;
        }

        char msg[1024];

        // 从配置文件读取巫师默认密码，读不到则使用 12345678
        const char* wizard_passwd = ini_get("wizard", "password", "12345678");

        // 创建巫师1（男性）
        if (!has_wizard1) {
            snprintf(
                msg, sizeof(msg),
                "INSERT INTO jianghudb (cc_id, cc_name, cc_passwd, "
                "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                "VALUES (1, '巫师一号', '%s', 21, 21, 21, 21, '男性', 100)",
                wizard_passwd);
            if (g_db->execute(msg) != 0) {
                LOG_ERROR("EnsureWizardAccounts: create wizard1 fail! %s",
                          g_db->error());
                return false;
            }
            LOG_INFO("EnsureWizardAccounts: created wizard1 (cc_id=1)");
        }

        // 创建巫师2（女性）
        if (!has_wizard2) {
            snprintf(
                msg, sizeof(msg),
                "INSERT INTO jianghudb (cc_id, cc_name, cc_passwd, "
                "cc_str, cc_int, cc_con, cc_dex, cc_gender, cc_grant) "
                "VALUES (2, '巫师二号', '%s', 21, 21, 21, 21, '女性', 100)",
                wizard_passwd);
            if (g_db->execute(msg) != 0) {
                LOG_ERROR("EnsureWizardAccounts: create wizard2 fail! %s",
                          g_db->error());
                return false;
            }
            LOG_INFO("EnsureWizardAccounts: created wizard2 (cc_id=2)");
        }

        return true;
    } catch (...) {
        LOG_ERROR("EnsureWizardAccounts: unknown!");
        return false;
    }
}

bool init_sql_server(const char* ipaddress, const char* name,
                     const char* password)
{
    static std::unique_ptr<IDatabase> s_dbOwner;

    const std::string db_type = get_db_type();

    if (db_type == "sqlite") {
        s_dbOwner          = create_sqlite_db();
        g_db               = s_dbOwner.get();
        const char* dbPath = ini_get("database", "path", "data/game.db");
        if (g_db->connect(nullptr, nullptr, nullptr, dbPath) != 0) {
            LOG_FATAL(
                "InitSQLServer -- Failed to connect to SQLite: Error: %s\n",
                g_db->error());
            return false;
        }
    }
    else {
        s_dbOwner = create_my_sql_db();
        g_db      = s_dbOwner.get();
        if (g_db->connect(ipaddress, name, password, NULL) != 0) {
            LOG_FATAL(
                "InitSQLServer -- Failed to connect to database: Error: %s\n",
                g_db->error());
            return false;
        }

        char sql[256];
        snprintf(sql, 255,
                 "CREATE DATABASE IF NOT EXISTS %s DEFAULT CHARACTER SET utf8 "
                 "DEFAULT COLLATE utf8_general_ci;",
                 g_server_dbname);
        if (g_db->execute(sql) != 0) {
            LOG_FATAL("InitSQLServer -- Failed to create database: Error: %s\n",
                      g_db->error());
            return false;
        }

        if (g_db->select_database(g_server_dbname /* "Main"*/) != 0) {
            LOG_FATAL("InitSQLServer -- Failed to select database(%s): %s\n",
                      g_server_dbname, g_db->error());
            return false;
        }
        g_db->set_charset("gb2312");
        // 存盘 JSON 大量使用反斜杠（" -> \"、GBK 尾字节 0x5C -> \\）。追加
        // NO_BACKSLASH_ESCAPES 使字面量内的反斜杠退化为普通字符，从而 JSON
        // 字节可原样存入 blob；单引号仍由 sql_escape_literal 翻倍处理。
        // 用 CONCAT 追加而非整体赋值，保留 STRICT_* 等既有 sql_mode。
        if (g_db->execute("set session sql_mode="
                          "concat(@@sql_mode, ',NO_BACKSLASH_ESCAPES')")
            != 0) {
            LOG_FATAL("InitSQLServer -- Failed to set sql_mode: %s\n",
                      g_db->error());
            return false;
        }
    }

    if (!prepare_tables()) {
        LOG_FATAL("InitSQLServer -- Failed to create tables: %s\n",
                  g_db->error());
        return false;
    }

    if (!ensure_wizard_accounts()) {
        LOG_FATAL("InitSQLServer -- Failed to ensure wizard accounts: %s\n",
                  g_db->error());
        return false;
    }

    LOG_DEBUG("sql success. dbtype=%s", db_type.c_str());
    return restore_data_table();
}

bool verify_user(const char* userid, char* passwd)
{
    try {
        DbResultSet result;
        char szQuery[512];

        // 检查用户ID和密码是否正确
        snprintf(szQuery, 512,
                 "select * from jianghudb where cc_id=%s and cc_passwd='%s'",
                 userid, passwd);
        if (0 != g_db->query(szQuery, result)) {
            LOG_ERROR("user:%s passwd:%s query db fail!", userid, passwd);
            return false;
        }
        if (result.empty()) {
            LOG_ERROR("database fail: user:%s passwd:%s store null", userid,
                      passwd);
            return false;
        }
        int n = result.size();
        if (n == 0) {
            return false;
        }
        // paswd pass OK!

        if (result.rows[0].columns.empty()) {
            LOG_ERROR("database fail: user:%s passwd:%s row null", userid,
                      passwd);
            return false;
        }

        const DbRow& row = result.rows[0];

        if (row[17].empty())  // 是新人
        {
            char msg[255];

            // 从新读出属性数据，开始分配
            int str = 0, intel = 0, con = 0, dex = 0;
            try {
                str   = atoi(row[4].c_str());
                intel = atoi(row[5].c_str());
                con   = atoi(row[6].c_str());
                dex   = atoi(row[7].c_str());
            } catch (...) {
                LOG_ERROR("error property:%s", userid);
                notify_fail("未知的属性值！");
                return false;
            }

            // 判断四属性值是否合法（单项 10~30，总和不得超过 84）
            if (str < 10 || str > 30) {
                notify_fail("力量值非法。");
                return false;
            }

            if (con < 10 || con > 30) {
                notify_fail("体质值非法。");
                return false;
            }

            if (intel < 10 || intel > 30) {
                notify_fail("智慧值非法。");
                return false;
            }

            if (dex < 10 || dex > 30) {
                notify_fail("敏捷值非法。");
                return false;
            }

            if (str + con + intel + dex > 84) {
                notify_fail("天赋属性值总和不得超过84。");
                return false;
            }

            CUser me;
            // 走 init_new_char 建档：起手 age 必须早于 setup_char——缺了它，
            // 新人会带着随机 age 算出的虚高 max_hp 上线，hp 也一并被抬到
            // 上限之上。
            init_new_char(me, row[1].c_str(), userid, row[10].c_str(), str, con,
                          intel, dex);
            me.set("combat_exp", 0);

            CVector v;
            v.append("chat");
            v.append("rumor");

            me.set("channels", v);

            me.set("grant", atoi(row[20].c_str()));

            // 现在检查是否合法
            if (strlen(me.name(1)) < 2 || strlen(me.name(1)) > 14) {
                notify_fail("姓名长度非法。");
                return false;
            }

            strcpy(msg, check_legal_name(me.name(1)));
            if (msg[0]) {
                notify_fail(msg);
                return false;
            }

            me.save_to_server();
        }

        return true;
    } catch (...) {
        LOG_ERROR("VerifyUser:unknown!");
        return false;
    }
}

void disconnect_sql_server()
{
    try {
        g_db->close();
    } catch (...) {
        LOG_ERROR("DisconnectSQLServer:unknown!");
        return;
    }
}
