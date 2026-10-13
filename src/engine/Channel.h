/**
 * @file Channel.h
 * @brief 频道系统门面（Facade），聚合聊天/表情/监听/投票/封禁/关系六个子系统。
 *
 * 子系统：
 * - ChatChannel         聊天频道广播与过滤
 * - EmoteSystem         表情定义与处理
 * - SnoopSystem         监听管理
 * - BanlistManager      IP 封禁与登录控制
 * - VotingSystem        投票管理
 * - RelationshipTracker 师徒招募与亲密度
 */

#pragma once

#include <vector>
#include <string>

#include "BanlistManager.h"
#include "ChatChannel.h"
#include "Container.h"
#include "EmoteSystem.h"
#include "RelationshipTracker.h"
#include "SnoopSystem.h"
#include "VotingSystem.h"

class CUser;

/**
 * @brief 频道系统门面，聚合六个子系统并保持原有 public 接口。
 *
 * public 数据成员（banlist / voted_players 等）为引用别名，
 * 直接指向对应子系统的成员，调用方语法不变。
 */
class CChannel : public CContainer {
public:
    // ========================================================================
    //  聊天频道（委托给 ChatChannel）
    // ========================================================================
    int do_channel(CContainer* me, CChar* who, const char* arg1,
                   const char* arg2);
    int check_channel_msg(CChar* me, const char* verb, const char* msg,
                          const char* block_key);

    // ========================================================================
    //  表情（委托给 EmoteSystem）
    // ========================================================================
    char* do_emote(CChar* me, CChar* target, const char* verb,
                   int channel_emote, const char* who = NULL);
    void download_emote(CUser* me);

    // ========================================================================
    //  监听（委托给 SnoopSystem）
    // ========================================================================
    void snoop(CChar* me, CChar* victim = NULL);
    CChar* query_snooping(CChar* me);
    void do_snooping(CChar* victim, char* msg);
    void remove_snoop(CChar* me);

    // ========================================================================
    //  关系（委托给 RelationshipTracker）
    // ========================================================================
    void update_relative(const char* me, const char* target, int32_t pt,
                         int flag = -1);
    CMapping* query_relative(const char* me, const char* target);
    void update_recruit(const char* master, const char* id, const char* name,
                        int32_t exp, int apprentice);
    CMapping* query_recruit(const char* id);

    // ========================================================================
    //  生命周期
    // ========================================================================
    void create();
    void release_me();
    CChannel();
    virtual ~CChannel();

    // ========================================================================
    //  public 数据成员 — 引用别名（指向子系统内部数据）
    // ========================================================================

    // 投票用
    CMapping& voted_players;
    CMapping& votes_casted;

    // 黑名单
    CMapping& banlist;

    // IP地址记录
    CMapping& loginlist;

    // IP地址登陆控制
    CMapping& loginlimit;

    // ========================================================================
    //  子系统访问器（供需要直接操作子系统的调用方使用）
    // ========================================================================
    CChatChannel& chat() { return m_chat; }
    CEmoteSystem& emote() { return m_emote; }
    CSnoopSystem& snoop_sys() { return m_snoop; }
    CBanlistManager& banlist_mgr() { return m_banlist; }
    CVotingSystem& voting() { return m_voting; }
    CRelationshipTracker& relationship() { return m_relationship; }

private:
    // ========================================================================
    //  子系统实例（声明顺序即初始化顺序）
    // ========================================================================
    CChatChannel m_chat;
    CEmoteSystem m_emote;
    CSnoopSystem m_snoop;
    CBanlistManager m_banlist;
    CVotingSystem m_voting;
    CRelationshipTracker m_relationship;
};
