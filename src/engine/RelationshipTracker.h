/**
 * @file RelationshipTracker.h
 * @brief 关系子系统 — 管理师徒招募与玩家亲密度。
 *
 *
 */

#pragma once

#include <cstdint>

#include "Mapping.h"

/**
 * @brief 关系跟踪器，负责师徒招募数据的 DB 读写与亲密度查询。
 *
 * 职责：
 * - update_recruit / query_recruit   师徒招募（DB 持久化）
 * - update_relative / query_relative 玩家亲密度（DB 持久化）
 */
class CRelationshipTracker {
public:
    CRelationshipTracker()  = default;
    ~CRelationshipTracker() = default;

    // 禁止拷贝（持有 CMapping 缓存）
    CRelationshipTracker(const CRelationshipTracker&)            = delete;
    CRelationshipTracker& operator=(const CRelationshipTracker&) = delete;

    /** @brief 从经验表里返回师徒招募数据。 */
    CMapping* query_recruit(const char* id);

    /** @brief 更新 recruit_exp 表，玩家存盘时呼叫。 */
    void update_recruit(const char* master, const char* id, const char* name,
                        int32_t exp, int apprentice);

    /** @brief 返回两个人的关系表：友好度、关系类型。 */
    CMapping* query_relative(const char* me, const char* target);

    /** @brief 更新关系表。 */
    void update_relative(const char* me, const char* target, int32_t pt,
                         int flag = -1);

    /** @brief 清空缓存（create 时调用）。 */
    void clear()
    {
        recruit_exp.clear();
        relative.clear();
    }

private:
    // 收徒用缓存
    CMapping recruit_exp;

    // 江湖亲密度缓存
    CMapping relative;
};
