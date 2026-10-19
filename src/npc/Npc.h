/**
 * @file Npc.h
 * @brief NPC 基类，管理对话/行为/自动战斗/师徒/任务。
 */

#pragma once

#include "Char.h"
#include "FBRoom.h"

#define NPC_BEGIN(npc_name)        \
    class npc_name : public CNpc { \
    public:                        \
        npc_name() {};             \
        virtual ~npc_name() {};
#define NPC_END \
    }           \
    ;

/**
 * @brief 通用对话钩子（hooks.pre_talk）的咨询结果。
 *
 * handled     钩子接管本次对话（引擎终止对话，不再走任何询问菜单）。
 * engine_flow 钩子不接管，但本次对话归引擎标准询问流程。
 *             两者皆假 = 钩子不适用，对话归对象自身流程。
 */
struct TalkHookResult {
    bool handled     = false;
    bool engine_flow = false;
};

// 咨询通用对话钩子（hooks.pre_talk）：脚本侧（任务等）是否接管本次对话。
// 钩子缺席 = 不适用，调用方按自身默认流程继续。
TalkHookResult query_talk_hook(CNpc* npc, CChar* me, char* ask);

/**
 * @brief NPC 基类，管理对话、行为、自动战斗、师徒与任务。
 */
class CNpc : public CChar {
public:
    virtual int accept_fight(CChar* me);
    int perform_action(char* magic, int target = 1);
    void random_move();
    virtual int recognize_apprentice(CChar* me);
    void add_money(int32_t amount);
    virtual int accept_object(CChar* me, CContainer* item);
    virtual int do_talk(CChar* me, char* ask = NULL);
    // 引擎标准询问流程（C++ 询问菜单）：不含通用对话钩子。
    // 供已自行咨询过钩子的调用方回退调用，避免同一次对话重复咨询钩子。
    int do_talk_inquiry(CChar* me, char* ask);
    CContainer* carry_object(const char* base_name, int32_t amount = 1,
                             int idx = 0);
    int return_home(CContainer* home);
    int self_action();

    virtual void create();
    virtual int32_t query_attr(int attr);
    virtual char* chat_msg_combat();
    virtual char* chat_msg();

    virtual void wield_ownered_item(const char* name);

    CNpc() { set("grant", GRANT_NPC); };
    virtual ~CNpc();

protected:
    typedef char* (*INQUERYFUNC)(CNpc* this_object, CChar* this_player);

    typedef struct _tagInquiry {
        string topic;
        string answer;
        INQUERYFUNC func = nullptr;

    } Inquiry_t;

    typedef map<string, std::unique_ptr<Inquiry_t>, less<string> > LISTINQUIRY;
    LISTINQUIRY m_list_inquiry;

    virtual char* greeting(CChar* me);
    void set_inquiry(const char* msg, INQUERYFUNC func,
                     const char* topic = NULL);
    void set_inquiry(const char* ask, const char* answer,
                     const char* topic = NULL);
};
