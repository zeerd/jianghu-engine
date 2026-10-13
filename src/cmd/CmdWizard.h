/**
 * @file CmdWizard.h
 * @brief 管理员命令处理（玩家管理/世界操作/系统管理等）。
 */
#pragma once

#include "CmdHandle.h"

// 自由函数声明
int change_password(const char* id, const char* pass);
int change_verify_password(const char* id, const char* pass);
int set_invalid_account(const char* id, int freeze);
extern bool g_reboot;
void printf_all_mappings();

#define MONSTER_NUM 313

// --- test fixture friends ---
class CmdWizardCardTest;
class CmdWizardWorldTest;
class CmdWizardAdminTest;
class CmdWizardSystemTest;

class CCmdWizard {
public:
    CCmdWizard();
    ~CCmdWizard();

    int handlecommand(char* cmd, CChar* me, int32_t target, char* arg);

    friend class CmdWizardCardTest;
    friend class CmdWizardWorldTest;
    friend class CmdWizardAdminTest;
    friend class CmdWizardSystemTest;

private:
    typedef int (*func)(CChar* me, int32_t target, char* arg);
    typedef map<string, func, less<string> > CMDMAP;
    CMDMAP m_cmd;
    CMapping m_grant;

    // ---- 玩家管理 ----
    static int promote(CChar* me, int32_t target, char* arg);
    static int kick(CChar* me, int32_t target, char* arg);
    static void do_kick(CContainer* ob, int32_t param1, int32_t param2);
    static int whois(CChar* me, int32_t target, char* arg);
    static int ban(CChar* me, int32_t target, char* arg);
    static int unban(CChar* me, int32_t target, char* arg);
    static int freeze(CChar* me, int32_t target, char* arg);
    static int unfreeze(CChar* me, int32_t target, char* arg);
    static int chpass(CChar* me, int32_t target, char* arg);
    static int chverifypass(CChar* me, int32_t target, char* arg);
    static int recover(CChar* me, int32_t target, char* arg);
    static int snoop(CChar* me, int32_t target, char* arg);
    static int chblk(CChar* me, int32_t target, char* arg);
    static int who(CChar* me, int32_t target, char* arg);

    // ---- 世界操作 ----
    static int goto_cmd(CChar* me, int32_t target, char* arg);
    static int summon(CChar* me, int32_t target, char* arg);
    static int clone_npc(CChar* me, int32_t target, char* arg);
    static int clone_object(CChar* me, int32_t target, char* arg);
    static int smash(CChar* me, int32_t target, char* arg);
    static int where_me(CChar* me, int32_t target, char* arg);
    static int stop_battle(CChar* me, int32_t target, char* arg);
    static void print_svalue(CChar* me, const char* key,
                             CMapping::svalue_t* sv);
    static int show_data(CChar* me, CContainer* who);
    static int data(CChar* me, int32_t target, char* arg);

    // ---- 系统管理 ----
    static int full(CChar* me, int32_t target, char* arg);
    static int sys_param(CChar* me, int32_t target, char* arg);
    static int skill_param(CChar* me, int32_t target, char* arg);
    static int call(CChar* me, int32_t target, char* arg);
    static int set_skill(CChar* me, int32_t target, char* arg);
    static int clear_skill(CChar* me, int32_t target, char* arg);
    static int restore_exp(CChar* me, int32_t target, char* arg);
    static int recruit_exp(CChar* me, int32_t target, char* arg);
    static int reboot(CChar* me, int32_t target, char* arg);
    static void do_reboot(CContainer* ob, int32_t param1, int32_t param2);
    static int reward_exp(CChar* me, int32_t target, char* arg);
    static int sub_exp(CChar* me, int32_t target, char* arg);
    static int print_mapping(CChar* me, int32_t target, char* arg);
    static int update(CChar* me, int32_t target, char* arg);
    static int reload_lua(CChar* me, int32_t target, char* arg);
    static int save(CChar* me, int32_t target, char* arg);

    // ---- 杂项 ----
    static int create_card(CChar* me, int32_t target, char* arg);
};