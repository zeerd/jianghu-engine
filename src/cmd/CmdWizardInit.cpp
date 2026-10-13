#include "CmdWizard.h"

CCmdWizard::CCmdWizard()
{
    m_grant.set("goto", 50);
    m_grant.set("summon", 50);
    m_grant.set("promote", 50);
    m_grant.set("kickout", 60);
    m_grant.set("whois", 70);
    m_grant.set("where", 70);
    m_grant.set("clone", 80);
    m_grant.set("clonenpc", 80);
    m_grant.set("smash", 80);
    m_grant.set("stop", 50);
    m_grant.set("full", 70);
    m_grant.set("call", 90);
    m_grant.set("set_skill", 90);
    m_grant.set("clear_skill", 100);
    m_grant.set("restore_exp", 100);
    m_grant.set("ban", 60);
    m_grant.set("unban", 60);
    m_grant.set("freeze", 70);
    m_grant.set("unfreeze", 70);
    m_grant.set("chpass", 80);
    m_grant.set("chverifypass", 90);
    m_grant.set("recover", 90);
    m_grant.set("wholist", 50);
    m_grant.set("snoop", 60);
    m_grant.set("chblk", 50);

    m_grant.set("reboot", 90);
    m_grant.set("reward_exp", 80);
    m_grant.set("sub_exp", 80);
    m_grant.set("reward_money", 80);
    m_grant.set("systemparam", 90);
    m_grant.set("skillparam", 90);
    m_grant.set("data", 70);

    m_grant.set("tail", 90);
    m_grant.set("update", 80);
    m_grant.set("reload", 80);
    m_grant.set("force_save", 80);

    m_cmd["goto"]         = goto_cmd;
    m_cmd["summon"]       = summon;
    m_cmd["promote"]      = promote;
    m_cmd["kickout"]      = kick;
    m_cmd["whois"]        = whois;
    m_cmd["where"]        = where_me;
    m_cmd["clone"]        = clone_object;
    m_cmd["clonenpc"]     = clone_npc;
    m_cmd["smash"]        = smash;
    m_cmd["stop"]         = stop_battle;
    m_cmd["full"]         = full;
    m_cmd["call"]         = call;
    m_cmd["set_skill"]    = set_skill;
    m_cmd["clear_skill"]  = clear_skill;
    m_cmd["restore_exp"]  = restore_exp;
    m_cmd["recruit_exp"]  = recruit_exp;
    m_cmd["ban"]          = ban;
    m_cmd["unban"]        = unban;
    m_cmd["freeze"]       = freeze;
    m_cmd["unfreeze"]     = unfreeze;
    m_cmd["chpass"]       = chpass;
    m_cmd["chverifypass"] = chverifypass;

    m_cmd["recover"] = recover;
    m_cmd["wholist"] = who;
    m_cmd["snoop"]   = snoop;

    m_cmd["reboot"] = reboot;

    m_cmd["reward_exp"] = reward_exp;
    m_cmd["sub_exp"]    = sub_exp;

    m_cmd["systemparam"] = sys_param;
    m_cmd["skillparam"]  = skill_param;

    m_cmd["data"]  = data;
    m_cmd["chblk"] = chblk;

    m_cmd["update"]     = update;
    m_cmd["force_save"] = save;
    m_cmd["reload"]     = reload_lua;

    m_cmd["print_mapping"] = print_mapping;

    m_cmd["createcard"] = create_card;
}

CCmdWizard::~CCmdWizard() {}

int CCmdWizard::handlecommand(char* cmd, CChar* me, int32_t target, char* arg)
{
    CMDMAP::iterator p;
    if ((p = m_cmd.find(cmd)) == m_cmd.end()) return -1;
    // 判断权限
    if (m_grant.query(cmd) > me->query("grant")) return -1;
    return ((*p).second)(me, target, arg);
}
