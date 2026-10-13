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
