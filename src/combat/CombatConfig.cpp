/**
 * @file CombatConfig.cpp
 * @brief Global combat system configuration */

#include <windows.h>

#include "CombatConfig.h"
#include "GameInit.h"

CCombatConfig g_combat_config;

CCombatConfig::CCombatConfig()
    : m_ip_limit(g_ip_limit),
      m_login_limit(2500),
      m_kick_idle(true),
      m_no_check_house(true),
      m_debug(false)
{
}

void CCombatConfig::set(const char* key, int32_t val)
{
    if (!key || !key[0]) return;

    // Update typed members if key matches
    if (strcmp(key, "ip_limit") == 0) {
        m_ip_limit = (int)val;
        return;
    }
    if (strcmp(key, "login_limit") == 0) {
        m_login_limit = (int)val;
        return;
    }
    if (strcmp(key, "kick_idle") == 0) {
        m_kick_idle = (val != 0);
        return;
    }
    if (strcmp(key, "no_check_house") == 0) {
        m_no_check_house = (val != 0);
        return;
    }
    if (strcmp(key, "debug") == 0) {
        m_debug = (val != 0);
        return;
    }

    // Fallback to dynamic store
    DynamicEntry& e = m_dynamic[key];
    e.int_val       = val;
}

void CCombatConfig::set(const char* key, const char* val)
{
    if (!key || !key[0]) return;

    if (strcmp(key, "debug") == 0) {
        m_debug = (val && val[0] && val[0] != '0');
        return;
    }

    DynamicEntry& e = m_dynamic[key];
    e.str_val       = val ? val : "";
    e.has_str       = true;
}

void CCombatConfig::del(const char* key)
{
    if (!key || !key[0]) return;
    m_dynamic.erase(key);
}

int32_t CCombatConfig::query(const char* key) const
{
    if (!key || !key[0]) return 0;

    if (strcmp(key, "ip_limit") == 0) return m_ip_limit;
    if (strcmp(key, "login_limit") == 0) return m_login_limit;
    if (strcmp(key, "kick_idle") == 0) return m_kick_idle ? 1 : 0;
    if (strcmp(key, "no_check_house") == 0) return m_no_check_house ? 1 : 0;
    if (strcmp(key, "debug") == 0) return m_debug ? 1 : 0;

    auto it = m_dynamic.find(key);
    if (it != m_dynamic.end()) return it->second.int_val;
    return 0;
}

const char* CCombatConfig::querystr(const char* key) const
{
    static char empty[] = "";
    if (!key || !key[0]) return empty;

    auto it = m_dynamic.find(key);
    if (it != m_dynamic.end() && it->second.has_str)
        return it->second.str_val.c_str();
    return empty;
}
