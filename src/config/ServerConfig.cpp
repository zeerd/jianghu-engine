/**
 * @file ServerConfig.cpp
 * @brief 服务器通用玩法开关配置的静态成员与初始化实现 */

#include "ServerConfig.h"

bool CServerConfig::s_force_add_hp          = false;
bool CServerConfig::s_no_force_conflict     = false;
bool CServerConfig::s_force_no_need_prepare = false;
bool CServerConfig::s_no_betrayer_valid     = false;
bool CServerConfig::s_rate_limit            = true;

void CServerConfig::init()
{
    s_force_add_hp      = ini_get_bool("server", "force_add_hp", 0) != 0;
    s_no_force_conflict = ini_get_bool("server", "no_force_conflict", 0) != 0;
    s_force_no_need_prepare
        = ini_get_bool("server", "force_no_need_prepare", 0) != 0;
    s_no_betrayer_valid = ini_get_bool("server", "no_betrayer_valid", 0) != 0;
    s_rate_limit        = ini_get_bool("server", "rate_limit", 1) != 0;
}
