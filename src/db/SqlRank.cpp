/**
 * @file SqlRank.cpp
 * @brief 排行榜  */

#include <windows.h>

#include <stdio.h>

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

            const std::string name = truncated_copy(row[1].c_str(), 39);
            const std::string fam  = truncated_copy(row[2].c_str(), 19);

            if (auto* handler = dynamic_cast<IActionHandler*>(me))
                handler->handle_action(
                    "update", NULL,
                    snprintf(msg, 255, "%d %s %s %s %s", i, row[0].c_str(),
                             name.empty() ? "无名氏" : name.c_str(),
                             fam.empty() ? "无门派" : fam.c_str(),
                             row[3].c_str()));
        }

        return 1;
    } catch (...) {
        LOG_ERROR("paihangbang:unknown!");
        return false;
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

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
