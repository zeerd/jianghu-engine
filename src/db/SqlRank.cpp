/**
 * @file SqlRank.cpp
 * @brief 排行榜与游戏记录  */

#include <windows.h>

#include <stdio.h>
#include <time.h>

#include "Combatd.h"
#include "Global.h"
#include "IActionHandler.h"

#include "IDatabase.h"
#include "Sql.h"

// ============================================================
// 排行榜
// ============================================================

int paihangbang(CContainer* me)
{
    try {
        char msg[255];
        DbResultSet result;

        snprintf(
            msg, 255,
            "select cc_id, cc_name, cc_family, cc_experice "
            "from jianghudb "
            "where cc_experice > 520000 and cc_freeze = 0 and cc_grant = 30 "
            "order by cc_experice desc "
            "LIMIT 0, 1000");

        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("paihangbang:query db fail! %s", g_db->error());
            return 0;
        }

        int n = result.size();

        for (int i = 0; i < n; i++) {
            const DbRow& row = result.rows[i];
            if (row.columns.empty()) {
                LOG_ERROR("paihangbang:database fail:row is NULL %ld.", i);
                continue;
            }

            char name[40], fam[20];
            strncpy(name, row[1].c_str(), 40);
            if (!row[2].empty())
                strncpy(fam, row[2].c_str(), 20);
            else
                fam[0] = 0;

            if (auto* handler = dynamic_cast<IActionHandler*>(me))
                handler->handle_action(
                    "update", NULL,
                    snprintf(msg, 255, "%d %s %s %s %s", i, row[0].c_str(),
                             name[0] ? name : "无名氏", fam[0] ? fam : "无门派",
                             row[3].c_str()));
        }

        return 1;
    } catch (...) {
        LOG_ERROR("paihangbang:unknown!");
        return false;
    }
}

// ============================================================
// 寻宝掉落记录 (t_mf 表)
// ============================================================

bool update_mf_record(const char* base_name, int energy)
{
    try {
        char msg[255];
        DbResultSet result;

        snprintf(msg, 255, "SELECT count FROM t_mf WHERE base_name = '%s'",
                 base_name);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("update_mf_record:query db fail!");
            return false;
        }

        unsigned long long n = result.size();
        if (n == 0) {
            snprintf(msg, 255,
                     "INSERT INTO t_mf (base_name, count, energy) VALUES "
                     "('%s', %d, %d)",
                     base_name, 1, energy);
            ret_code = g_db->execute(msg);
            if (ret_code != 0) {
                LOG_ERROR("update_mf_record:insert to db fail!");
                return false;
            }
        }
        else {
            const DbRow& row = result.rows[0];

            snprintf(msg, 255,
                     "update t_mf set count = '%ld' where base_name = '%s'",
                     atoi(row[0].c_str()) + 1, base_name);
            ret_code = g_db->execute(msg);

            if (ret_code != 0) {
                LOG_ERROR("update_mf_record:%s!", g_db->error());
                return false;
            }
        }

        return true;
    } catch (...) {
        LOG_ERROR("update_mf_record:unknown!");
        return false;
    }
}

// ============================================================
// 玩家历练记录 (t_record 表)
// ============================================================

bool sql_update_record(const char* id, int type, int level, const char* data,
                       bool update)
{
    try {
        LOG_DEBUG("sql_update_record: %s, %d, %d, %s, %s", id, type, level,
                  data, (update ? "update" : "-"));

        char msg[255];
        DbResultSet result;

        char savetime[80];
        struct tm* newtime;
        time_t current = g_current_time;
        newtime        = localtime(&current);

        sprintf(savetime, "%d/%02d/%02d %02d:%02d:%02d",
                1900 + newtime->tm_year, newtime->tm_mon + 1, newtime->tm_mday,
                newtime->tm_hour, newtime->tm_min, newtime->tm_sec);

        snprintf(msg, 255,
                 "SELECT cc_level FROM t_record WHERE cc_id = '%s' and cc_type "
                 "= %d;",
                 id, type);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("update_record:query db fail!");
            return false;
        }

        unsigned long long n = result.size();
        if (n == 0) {
            snprintf(msg, 255,
                     "INSERT INTO t_record "
                     "(cc_time, cc_id, cc_type, cc_level, cc_savedata) "
                     "VALUES ('%s', %s, %d, %d, '%s')",
                     savetime, id, type, level, data);
            ret_code = g_db->execute(msg);
            if (ret_code != 0) {
                LOG_ERROR("update_record:insert to db fail!");
                return false;
            }
        }
        else {
            const DbRow& row = result.rows[0];

            snprintf(msg, 255,
                     "update t_record set cc_time = '%s', cc_level=%d, "
                     "cc_savedata='%s' "
                     "where cc_id = '%s' and cc_type = %d and cc_level < %d",
                     savetime, level, data, id, type, level);
            ret_code = g_db->execute(msg);

            if (ret_code != 0) {
                LOG_ERROR("update_record:%s!", g_db->error());
                return false;
            }
        }

        return true;
    } catch (...) {
        LOG_ERROR("update_record:unknown!");
        return false;
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// 全局：sql_update_record(id, type, level, data, update) - 更新玩家历练记录
int l_sql_update_record(lua_State* L)
{
    const char* id       = luaL_checkstring(L, 1);
    int type             = (int)luaL_checkinteger(L, 2);
    int level            = (int)luaL_checkinteger(L, 3);
    const char* data     = luaL_optstring(L, 4, "");
    bool update          = lua_toboolean(L, 5) != 0;
    std::string data_gbk = data;
    lua_pushboolean(
        L,
        sql_update_record(id, type, level, data_gbk.c_str(), update) ? 1 : 0);
    return 1;
}
int l_update_mf_record(lua_State* L)
{
    const char* base_name = luaL_checkstring(L, 1);
    int energy            = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, update_mf_record(base_name, energy) ? 1 : 0);
    return 1;
}

// paihangbang(npc) -> number  排行榜查询
int l_paihangbang(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    lua_pushinteger(L, paihangbang(c));
    return 1;
}
