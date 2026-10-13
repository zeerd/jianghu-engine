/**
 * @file RelationshipTracker.cpp
 * @brief RelationshipTracker implementation. */

#include "RelationshipTracker.h"

#include <cstdio>
#include <cstdlib>

#include "IDatabase.h"
#include "Sql.h"
#include "StringUtil.h"

// 从经验表里返回
CMapping* CRelationshipTracker::query_recruit(const char* id)
{
    char msg[255];
    DbResultSet result;
    if (!db_query_recruit(id, result)) {
        return nullptr;
    }

    recruit_exp.clear();

    if (!result.empty()) {
        int n = result.size();

        for (int i = 0; i < n; i++) {
            const DbRow& row = result.rows[i];

            recruit_exp.set(snprintf(msg, 255, "%s/name", row[0].c_str()),
                            row[1].c_str());
            recruit_exp.set(snprintf(msg, 255, "%s/exp", row[0].c_str()),
                            atol(row[2].c_str()));
            recruit_exp.set(snprintf(msg, 255, "%s/apprentice", row[0].c_str()),
                            atol(row[3].c_str()));
        }
    }

    return &recruit_exp;
}

// 更新 recruit_exp 表，玩家存盘时呼叫
void CRelationshipTracker::update_recruit(const char* master, const char* id,
                                          const char* name, int32_t exp,
                                          int apprentice)
{
    if (!db_update_recruit(master, id, name, exp, apprentice)) {
        return;
    }
}

// 返回两个人的关系表：友好度，关系(0:普通，1:夫妻 2:金兰 3:仇敌)
CMapping* CRelationshipTracker::query_relative(const char* me,
                                               const char* target)
{
    long point       = 0;
    int relative_val = 0;
    if (!db_query_relative(me, target, point, relative_val)) {
        relative.clear();
        return &relative;
    }

    relative.clear();
    relative.set("point", point);
    relative.set("relative", relative_val);

    return &relative;
}

// 更新关系表
void CRelationshipTracker::update_relative(const char* me, const char* target,
                                           int32_t pt, int flag)
{
    if (!db_update_relative(me, target, pt, flag)) {
        return;
    }
}
