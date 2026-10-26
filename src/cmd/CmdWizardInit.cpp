#include "CmdWizard.h"

CCmdWizard::CCmdWizard()
{
    // 巫师命令单源注册表：命令字 + handler + 最低 grant，三元组一一对应，
    // 与 docs/protocol.md §4.4 的巫师命令表逐条一致。
    // m_cmd 与 m_grant 由同一张表生成，杜绝两类漂移：
    //   - 有授权条目、无 handler（如历史上的
    //   reward_money/tail——不可达死条目）；
    //   - 有 handler、无授权条目（缺省 grant 为 0，任何巫师都能执行）。
    struct Entry {
        const char* name;
        func handler;
        int grant;
    };
    static const Entry entries[] = {
        // ---- 玩家管理 ----
        {"promote", promote, 50},
        {"kickout", kick, 60},
        {"whois", whois, 70},
        {"ban", ban, 60},
        {"unban", unban, 60},
        {"freeze", freeze, 70},
        {"unfreeze", unfreeze, 70},
        {"chpass", chpass, 80},
        {"chverifypass", chverifypass, 90},
        {"recover", recover, 90},
        {"snoop", snoop, 60},
        {"chblk", chblk, 50},
        {"wholist", who, 50},

        // ---- 世界操作 ----
        {"goto", goto_cmd, 50},
        {"summon", summon, 50},
        {"clone", clone_object, 80},
        {"clonenpc", clone_npc, 80},
        {"smash", smash, 80},
        {"where", where_me, 70},
        {"stop", stop_battle, 50},
        {"data", data, 70},
        {"update", update, 80},

        // ---- 系统管理 ----
        {"full", full, 70},
        {"call", call, 90},
        {"set_skill", set_skill, 90},
        {"clear_skill", clear_skill, 100},
        {"restore_exp", restore_exp, 100},
        {"recruit_exp", recruit_exp, 50},
        {"print_mapping", print_mapping, 50},
        {"reboot", reboot, 90},
        {"reward_exp", reward_exp, 80},
        {"sub_exp", sub_exp, 80},
        {"systemparam", sys_param, 90},
        {"skillparam", skill_param, 90},
        {"reload", reload_lua, 80},
        {"force_save", save, 80},
    };

    for (const Entry& e : entries) {
        m_cmd[e.name] = e.handler;
        m_grant.set(e.name, e.grant);
    }
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
