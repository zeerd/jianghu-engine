#include "CmdUser.h"

CCmdUser::CCmdUser()
{
    m_cmd["skill"]          = skill;
    m_cmd["downloadskill"]  = download_skill;
    m_cmd["downloadobject"] = download_object;
    m_cmd["downloaduser"]   = downloaduser;

    m_cmd["quit"]  = quit;
    m_cmd["save"]  = save;
    m_cmd["talk"]  = talk;
    m_cmd["ask"]   = talk;
    m_cmd["set"]   = set_param;
    m_cmd["unset"] = unset;

    m_cmd["refuse"] = refuse;

    m_cmd["mudage"]     = mudage;
    m_cmd["password"]   = passwd;
    m_cmd["verifypass"] = verify_pass;
    m_cmd["upgrade"]    = upgrade;
    m_cmd["tune"]       = tune;
    m_cmd["vote"]       = vote;

    m_cmd["refusetell"] = refuse_tell;
    m_cmd["wantlisten"] = listen;

    m_cmd["nickname"] = nick_name;
    m_cmd["rexp"]     = recruit_exp;

    m_cmd["invite"] = invite;
    m_cmd["accept"] = accept;
    m_cmd["mv"]     = mv;
    m_cmd["loc"]    = loc;

    m_cmd["who"] = who;

    m_cmd["listgoods"] = list_goods;

    // 组队系统
    m_cmd["party"]        = party;
    m_cmd["accept_party"] = accept_party;
    m_cmd["partyleave"]   = partyleave;
    m_cmd["partykick"]    = partykick;
    m_cmd["partyfollow"]  = partyfollow;
    m_cmd["partylist"]    = partylist;

    // 夫妻组队（复用 party 逻辑）
    m_cmd["pair"]        = pair;
    m_cmd["accept_pair"] = accept_pair;
    m_cmd["pairleave"]   = pairleave;
    m_cmd["pairkick"]    = pairkick;
    m_cmd["pairfollow"]  = pairfollow;
    m_cmd["pairlist"]    = pairlist;

    m_cmd["buy"]      = buy;
    m_cmd["validate"] = validate;
}

CCmdUser::~CCmdUser() {}

int CCmdUser::handlecommand(char* cmd, CChar* me, int32_t target, char* arg)
{
    CMDMAP::iterator p;
    if ((p = m_cmd.find(cmd)) == m_cmd.end()) return -1;
    return ((*p).second)(me, target, arg);
}
