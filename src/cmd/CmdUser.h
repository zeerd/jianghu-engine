/**
 * @file CmdUser.h
 * @brief 用户级命令处理（技能/账号/组队/社交等）。
 */
#pragma once

#include "CmdHandle.h"
#include "PartyConfig.h"

// 自由函数声明
bool verify_user(const char* userid, char* passwd);
int change_password(const char* id, const char* pass);
int verify_password(const char* id, const char* pass);
int change_verify_password(const char* id, const char* pass);
void setup_char(CChar* me);
char* check_legal_name(const char* name);

#define MAX_ENV_VARS     10
#define VOTE_AGE_LIMIT   15
#define VOTE_LEVEL_LIMIT 5

// --- test fixture friends ---
class CmdUserSkillTest;
class CmdUserCardTest;
class CmdUserVendorTest;
class CmdUserAccountTest;
class CmdUserPartyTest;
class CmdUserSocialTest;

class CCmdUser {
public:
    CCmdUser();
    ~CCmdUser();

    int handlecommand(char* cmd, CChar* me, int32_t target, char* arg);

    friend class CmdUserSkillTest;
    friend class CmdUserCardTest;
    friend class CmdUserVendorTest;
    friend class CmdUserAccountTest;
    friend class CmdUserPartyTest;
    friend class CmdUserSocialTest;

private:
    typedef int (*func)(CChar* me, int32_t target, char* arg);
    typedef map<string, func, less<string> > CMDMAP;
    CMDMAP m_cmd;

    // ---- 技能/信息 ----
    static int info(CChar* me, int32_t target, char* arg);
    static int item(CChar* me, int32_t target, char* arg);
    static int skill(CChar* me, int32_t target, char* arg);
    static int download_skill(CChar* me, int32_t target, char* arg);
    static int download_object(CChar* me, int32_t target, char* arg);
    static int downloaduser(CChar* me, int32_t target, char* arg);

    // ---- 账号/设置 ----
    static int quit(CChar* me, int32_t target, char* arg);
    static int save(CChar* me, int32_t target, char* arg);
    static int set_param(CChar* me, int32_t target, char* arg);
    static int unset(CChar* me, int32_t target, char* arg);
    static int mudage(CChar* me, int32_t target, char* arg);
    static int passwd(CChar* me, int32_t target, char* arg);
    static int validate(CChar* me, int32_t target, char* arg);
    static int verify_pass(CChar* me, int32_t target, char* arg);
    static int upgrade(CChar* me, int32_t target, char* arg);
    static int tune(CChar* me, int32_t target, char* arg);
    static int refuse_tell(CChar* me, int32_t target, char* arg);
    static int listen(CChar* me, int32_t target, char* arg);
    static int refuse(CChar* me, int32_t target, char* arg);

    // ---- 组队系统 ----
    static int party(CChar* me, int32_t target, char* arg);
    static int accept_party(CChar* me, int32_t target, char* arg);
    static int partyleave(CChar* me, int32_t target, char* arg);
    static int partykick(CChar* me, int32_t target, char* arg);
    static int partyfollow(CChar* me, int32_t target, char* arg);
    static int partylist(CChar* me, int32_t target, char* arg);
    static int pair(CChar* me, int32_t target, char* arg);
    static int accept_pair(CChar* me, int32_t target, char* arg);
    static int pairleave(CChar* me, int32_t target, char* arg);
    static int pairkick(CChar* me, int32_t target, char* arg);
    static int pairfollow(CChar* me, int32_t target, char* arg);
    static int pairlist(CChar* me, int32_t target, char* arg);

    // ---- 社交/交易 ----
    static int talk(CChar* me, int32_t target, char* arg);
    static int vote(CChar* me, int32_t target, char* arg);
    static int determine_total();
    static int counting(CMapping* players, const char* myvariable);
    static void nullify_vote(CContainer* ob, int32_t vote_id, int32_t param2);
    static int nick_name(CChar* me, int32_t target, char* arg);
    static int recruit_exp(CChar* me, int32_t target, char* arg);
    static int invite(CChar* me, int32_t target, char* arg);
    static int accept(CChar* me, int32_t target, char* arg);
    static int mv(CChar* me, int32_t target, char* arg);
    static int loc(CChar* me, int32_t target, char* arg);
    static int who(CChar* me, int32_t target, char* arg);

    // ---- 月卡/商业 ----
    static int list_goods(CChar* me, int32_t target, char* arg);
    static int buy(CChar* me, int32_t target, char* arg);
    static int card_help(CChar* me, int32_t target, char* arg);
    static int add_card(CChar* me, int32_t target, char* arg);
    static int remove_card(CChar* me, int32_t target, char* arg);
    static int show_cards(CChar* me, int32_t target, char* arg);
};