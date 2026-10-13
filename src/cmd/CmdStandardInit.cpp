#include "CmdStandard.h"

CCmdStandard::CCmdStandard()
{
    m_standard_cmd["kill"]   = kill;
    m_standard_cmd["cast"]   = cast;
    m_standard_cmd["join"]   = join_battle;
    m_standard_cmd["guard"]  = guard;
    m_standard_cmd["fight"]  = fight;
    m_standard_cmd["follow"] = follow;
    m_standard_cmd["halt"]   = halt;
    m_standard_cmd["flee"]   = halt;

    m_standard_cmd["tell"] = tell;
    m_standard_cmd["say"]  = say;

    m_standard_cmd["get"]     = get;
    m_standard_cmd["drop"]    = drop;
    m_standard_cmd["wield"]   = wield;
    m_standard_cmd["unwield"] = un_wield;
    m_standard_cmd["use"]     = use;
    m_standard_cmd["go"]      = go_door;
    m_standard_cmd["give"]    = give;

    m_standard_cmd["apprentice"] = apprentice;
    m_standard_cmd["recruit"]    = recruit;
    m_standard_cmd["expell"]     = expell;
    m_standard_cmd["learn"]      = learn;
    m_standard_cmd["teach"]      = teach;

    m_standard_cmd["sleep"]    = Sleep;
    m_standard_cmd["exercise"] = exercise;
    m_standard_cmd["enable"]   = enable;
    m_standard_cmd["study"]    = study;
    m_standard_cmd["practice"] = practice;
    m_standard_cmd["prepare"]  = prepare;
    m_standard_cmd["abandon"]  = abandon;
    m_standard_cmd["enforce"]  = enforce;
    m_standard_cmd["look"]     = look;
}

CCmdStandard::~CCmdStandard() {}

int CCmdStandard::handlecommand(char* cmd, CChar* me, int32_t target, char* arg)
{
    CMDMAP::iterator p;
    if ((p = m_standard_cmd.find(cmd)) == m_standard_cmd.end()) return -1;
    return ((*p).second)(me, target, arg);
}
