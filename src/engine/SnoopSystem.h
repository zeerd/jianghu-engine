/**
 * @file SnoopSystem.h
 * @brief 监听子系统 — 管理玩家间的监听关系与消息转发。
 *
 *
 */

#pragma once

#include <cstdint>
#include <list>
#include <memory>

class CChar;

/**
 * @brief 监听系统，维护 listener → victim 映射链表。
 *
 * 职责：
 * - snoop()          添加/替换监听关系
 * - query_snooping() 查询监听目标
 * - do_snooping()    转发被监听者的消息
 * - remove_snoop()   按监听者或被监听者移除
 * - ReleaseMe()      清空全部监听
 */
class CSnoopSystem {
public:
    CSnoopSystem() = default;
    ~CSnoopSystem();

    // 禁止拷贝（持有裸指针链表）
    CSnoopSystem(const CSnoopSystem&)            = delete;
    CSnoopSystem& operator=(const CSnoopSystem&) = delete;

    /** @brief 加入/替换监听。victim == NULL 时仅移除 me 的旧监听。 */
    void snoop(CChar* me, CChar* victim = nullptr);

    /** @brief 返回 me 当前监听的 victim；无则返回 nullptr。 */
    CChar* query_snooping(CChar* me);

    /** @brief 将 victim 收到的 msg 转发给监听者。 */
    void do_snooping(CChar* victim, char* msg);

    /** @brief 从链表中移除 me（不论 me 是监听者还是被监听者）。 */
    void remove_snoop(CChar* me);

    /** @brief 清空监听链表。 */
    void release_me();

private:
    typedef struct _snoop {
        CChar* listener;  // 监听者
        CChar* victim;    // 被监听者
    } snoop_t;

    std::list<std::unique_ptr<snoop_t>> m_snoop_list;  // 监听链表
};
