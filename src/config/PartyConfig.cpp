/**
 * @file PartyConfig.cpp
 * @brief 组队系统配置的静态成员与初始化实现 */

#include "PartyConfig.h"

int CPartyConfig::s_max_members        = 4;
bool CPartyConfig::s_share_exp         = true;
bool CPartyConfig::s_auto_follow_fight = true;
int CPartyConfig::s_couple_level_diff  = 10;

void CPartyConfig::init()
{
    s_max_members = ini_get_int("party", "max_members", 4);
    if (s_max_members < 2) s_max_members = 2;
    if (s_max_members > 10) s_max_members = 10;

    s_share_exp = ini_get_bool("party", "share_exp", 1);

    s_auto_follow_fight = ini_get_bool("party", "auto_follow_fight", 1);

    s_couple_level_diff = ini_get_int("party", "couple_level_diff", 10);
    if (s_couple_level_diff < 0) s_couple_level_diff = 0;
    if (s_couple_level_diff > 50) s_couple_level_diff = 50;
}
