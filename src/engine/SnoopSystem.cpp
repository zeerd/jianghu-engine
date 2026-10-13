/**
 * @file SnoopSystem.cpp
 * @brief SnoopSystem implementation. */

#include "SnoopSystem.h"

#include "Char.h"
#include "Message.h"

// 加入监听链表
void CSnoopSystem::snoop(CChar* me, CChar* victim)
{
    for (auto it = m_snoop_list.begin(); it != m_snoop_list.end();) {
        if ((*it)->listener == me) {
            ((*it)->victim)->add_temp("client/snoop", -1);
            it = m_snoop_list.erase(it);
            break;
        }
        ++it;
    }

    // 增加
    if (victim) {
        auto node      = std::make_unique<snoop_t>();
        node->listener = me;
        node->victim   = victim;
        m_snoop_list.push_front(std::move(node));

        victim->add_temp("client/snoop", 1);  // 被监听
    }
}

CChar* CSnoopSystem::query_snooping(CChar* me)
{
    // 返回监听者
    for (auto& node : m_snoop_list) {
        if (node->listener == me) return node->victim;
    }

    return nullptr;
}

// 某人的接收信息过滤
void CSnoopSystem::do_snooping(CChar* victim, char* msg)
{
    char str[512];
    CChar* me;

    if (victim->query_temp("snoop_lock"))  // 锁定
        return;

    for (auto& node : m_snoop_list) {
        if (node->victim == victim) {
            me = node->listener;
            me->set_temp("snoop_lock", 1);  // 监听中，设置锁
            tell_object(me, snprintf(str, 500, "$HIY<监听>%s", msg));
            me->delete_temp("snoop_lock");
        }
    }
}

// 从监听链表中删掉，不管是监听者还是被监听者
void CSnoopSystem::remove_snoop(CChar* me)
{
    me->delete_temp("client/snoop");

    auto it = m_snoop_list.begin();
    while (it != m_snoop_list.end()) {
        if ((*it)->listener == me || (*it)->victim == me) {
            it = m_snoop_list.erase(it);
        }
        else {
            ++it;
        }
    }
}

void CSnoopSystem::release_me() { m_snoop_list.clear(); }

CSnoopSystem::~CSnoopSystem() { release_me(); }
