#include <windows.h>

#include <stdio.h>

#include "Combatd.h"
#include "Channel.h"
#include "Global.h"
#include "User.h"

#include "Sql.h"

char* check_legal_name(const char* name);

#include "IDatabase.h"

bool card_init(void)
{
    const char* ddl
        = "CREATE TABLE IF NOT EXISTS `card_jx` "
          "( "
          "`idx` INT NOT NULL AUTO_INCREMENT"
          " , `card` TINYTEXT NOT NULL"
          " , `id` INT NULL"
          " , `begintime` INT NULL"
          " , `endtime` INT NULL"
          " , `activetime` INT NULL"
          " , PRIMARY KEY (`idx`)"
          ") ENGINE = MyISAM CHARSET=latin1;";

    try {
        if (g_db->execute(ddl) != 0) {
            LOG_ERROR("card_init:query db fail!");
            return false;
        }
    } catch (...) {
        LOG_ERROR("card_init:unknown!");
        return false;
    }

    return true;
}

bool register_card(const char* card, const char* id)
{
    card_init();

    LOG_INFO("register_cardl: %s -> %s", card, id);

    try {
        char msg[255];
        DbResultSet result;
        DbResultSet result2;

        snprintf(msg, 255,
                 "SELECT IFNULL(endtime, 0) as e ,IFNULL(begintime, 0) as b "
                 "FROM card_jx WHERE card = '%s' ORDER BY e DESC",
                 card);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("register_card:query db fail!");
            return false;
        }

        if (result.empty()) {
            LOG_ERROR("database fail: card:%s store null", card);
            return 0;
        }
        int n = result.size();
        if (n == 0) {
            return notify_fail("非可用卡号。");
        }

        const DbRow& row = result.rows[0];
        if (row.columns.empty()) {
            return notify_fail("非可用卡号！");
        }

        time_t now;
        time(&now);

        time_t endtime = 0;
        if (!row[0].empty()) {
            endtime = (time_t)atol(row[0].c_str());
        }
        time_t begintime = 0;
        if (!row[1].empty()) {
            begintime = (time_t)atol(row[1].c_str());
        }

        if (begintime
            == 0) {  // 只有没用过的卡才可以续在其他卡的后面，用过的开始时间就固定了
            snprintf(msg, 255,
                     "SELECT IFNULL(endtime, 0) as e FROM card_jx WHERE id = "
                     "'%s' ORDER BY e DESC",
                     id);
            int ret2 = g_db->query(msg, result2);
            if (ret2 != 0) {
                LOG_ERROR("register_card:query db fail!");
                return false;
            }

            if (!result2.empty()) {
                int n = result2.size();
                if (n > 0) {
                    const DbRow& row = result2.rows[0];
                    if (!row.columns.empty()) {
                        time_t et = 0;
                        if (!row[0].empty()) {
                            et = (time_t)atol(row[0].c_str());
                            if (et > now) {
                                begintime = et;
                            }
                            else {
                                begintime = now;
                            }
                        }
                    }
                }
            }
        }

        if (endtime == 0) {  // 空卡 初始化

            int value = 2592000 * 3;  // 30天*3
            time_t bt, et;

            if (begintime == 0) {
                time(&bt);
            }
            else {
                bt = begintime;
            }
            et = bt + (time_t)value;

            snprintf(msg, 255,
                     "update card_jx set activetime = '%ld', begintime = "
                     "'%ld', endtime = %ld, id = '%s' where card = '%s'",
                     (long)now, (long)bt, (long)et, id, card);
            ret_code = g_db->execute(msg);

            if (ret_code != 0) {
                LOG_ERROR("register_card:%s!", g_db->error());
                return notify_fail("卡注册失败。");
            }
        }
        else {
            return notify_fail("此卡已经被注册过了。");
        }

        return true;
    } catch (...) {
        LOG_ERROR("register_card:unknown!");
        return false;
    }
}

bool verify_id(const char* id, char* endtime_s)
{
    card_init();

    try {
        char msg[255];
        DbResultSet result;

        snprintf(msg, 255,
                 "SELECT IFNULL(endtime, 0) as e FROM card_jx WHERE id = '%s' "
                 "ORDER BY e DESC",
                 id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("verify_id:query db fail!");
            return false;
        }

        if (result.empty()) {
            LOG_ERROR("database fail: card_jx id:%s store null", id);
            return 0;
        }
        int n = result.size();
        if (n == 0) {
            return notify_fail("该账号没有绑定任何游戏卡1。");
        }

        const DbRow& row = result.rows[0];
        if (row.columns.empty()) {
            return notify_fail("该账号没有绑定任何游戏卡2。");
        }

        time_t endtime = (time_t)atol(row[0].c_str());

        if (endtime == 0) {  // 无卡
            return notify_fail("该账号没有绑定任何游戏卡3。");
        }
        else {
            time_t t;
            time(&t);

            if (t > endtime) {
                if (endtime != NULL) {
                    struct tm* newtime;
                    newtime = localtime(&endtime);
                    sprintf(endtime_s, "%d/%02d/%02d %02d:%02d:%02d",
                            1900 + newtime->tm_year, newtime->tm_mon + 1,
                            newtime->tm_mday, newtime->tm_hour, newtime->tm_min,
                            newtime->tm_sec);
                }
                return notify_fail("游戏卡到期。");
            }
        }

        return true;
    } catch (...) {
        LOG_ERROR("verify_id:unknown!");
        return false;
    }
}

bool unregister_card(const char* card, const char* id)
{
    card_init();

    LOG_INFO("unregister_card: %s <- %s", card, id);

    try {
        char msg[255];
        DbResultSet result;
        DbResultSet result2;

        snprintf(msg, 255,
                 "SELECT card,IFNULL(begintime, 0) as b,IFNULL(activetime, 0) "
                 "as a FROM card_jx WHERE id = '%s' ORDER BY endtime DESC",
                 id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("unregister_card:query db fail!");
            return false;
        }

        if (result.empty()) {
            LOG_ERROR("unregister_card: database fail: card:%s store null",
                      card);
            return 0;
        }
        int n = result.size();
        if (n == 0) {
            return notify_fail("该账号没有绑定任何游戏卡1。");
        }

        const DbRow& row = result.rows[0];
        if (row.columns.empty()) {
            return notify_fail("该账号没有绑定任何游戏卡2。");
        }

        if (strcmp(card, row[0].c_str())) {
            return notify_fail("只能移除时间线上的最后一张卡。");
        }
        else {
            time_t begintime = 0;
            if (!row[1].empty()) {
                begintime = (time_t)atol(row[1].c_str());
            }
            time_t activetime = 0;
            if (!row[2].empty()) {
                activetime = (time_t)atol(row[2].c_str());

                if (begintime > activetime) {
                    begintime = activetime;
                }

                snprintf(msg, 255,
                         "update card_jx set endtime = 0, begintime=%ld, id=0 "
                         "where card = '%s'",
                         (long)begintime, card);
                ret_code = g_db->execute(msg);

                if (ret_code != 0) {
                    LOG_ERROR("unregister_card:%s!", g_db->error());
                    return notify_fail("卡注册失败。");
                }
            }
        }

        result.clear();
        return true;
    } catch (...) {
        LOG_ERROR("unregister_card:unknown!");
        return false;
    }
}

bool create_card(CChar* me)
{
    card_init();

    try {
        char sql[255];
        char msg[255];
        UUID uuid;

        ::UuidCreate(&uuid);
        RPC_CSTR szUuid = NULL;
        if (::UuidToStringA(&uuid, &szUuid) == RPC_S_OK) {
            snprintf(sql, 255,
                     "INSERT INTO `card_jx` (`idx`, `card`, `id`, `begintime`, "
                     "`endtime`) VALUES (NULL, '%s', NULL, NULL, NULL);",
                     (char*)szUuid);
            snprintf(msg, 255, "新卡 ： %s", (char*)szUuid);
            ::RpcStringFreeA(&szUuid);

            int ret_code = g_db->execute(sql);
            if (ret_code != 0) {
                LOG_ERROR("create_card:query db fail!");
                return false;
            }
        }

        tell_object(me, msg);
        return true;
    } catch (...) {
        LOG_ERROR("create_card:unknown!");
        return false;
    }
}

bool show_cards(CChar* me)
{
    card_init();

    try {
        char msg[255];
        DbResultSet result;

        tell_object(me,
                    "               游戏卡                |       开始时间     "
                    " |       结束时间");

        snprintf(msg, 255,
                 "SELECT card, begintime, IFNULL(endtime, 0) as e FROM card_jx "
                 "WHERE id = '%s'",
                 me->id(1));
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("show_cards:query db fail!");
            return notify_fail("该账号没有绑定任何游戏卡。");
        }

        if (result.empty()) {
            LOG_ERROR("database fail: card_jx id:%s store null", me->id(1));
            return notify_fail("该账号没有绑定任何游戏卡。");
        }
        int n = result.size();
        if (n == 0) {
            return notify_fail("该账号没有绑定任何游戏卡。");
        }

        for (size_t i = 0; i < result.rows.size(); i++) {
            const DbRow& row = result.rows[i];

            struct tm* newtime;

            char endtime_s[65];
            time_t endtime = (time_t)atol(row[2].c_str());
            newtime        = localtime(&endtime);
            sprintf(endtime_s, "%d/%02d/%02d %02d:%02d:%02d",
                    1900 + newtime->tm_year, newtime->tm_mon + 1,
                    newtime->tm_mday, newtime->tm_hour, newtime->tm_min,
                    newtime->tm_sec);

            char begintime_s[65];
            time_t begintime = (time_t)atol(row[1].c_str());
            newtime          = localtime(&begintime);
            sprintf(begintime_s, "%d/%02d/%02d %02d:%02d:%02d",
                    1900 + newtime->tm_year, newtime->tm_mon + 1,
                    newtime->tm_mday, newtime->tm_hour, newtime->tm_min,
                    newtime->tm_sec);

            tell_object(me, snprintf(msg, 255, "%s | %s | %s", row[0].c_str(),
                                     begintime_s, endtime_s));
        }

        return true;
    } catch (...) {
        LOG_ERROR("show_cards:unknown!");
        return false;
    }
}
