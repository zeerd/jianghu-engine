

/**
 * @file CombatConfig.h
 * @brief 战斗系统全局配置管理，提供类型化访问器与通用键值接口。
 */
#pragma once

#include <cstdint>
#include <string>
#include <map>

/**
 * @brief 战斗系统配置，管理 IP 限制、登录限制、调试开关等参数。
 */
class CCombatConfig {
public:
    CCombatConfig();

    // --- Typed accessors for known config keys ---
    int ip_limit() const { return m_ip_limit; }
    int login_limit() const { return m_login_limit; }
    bool kick_idle() const { return m_kick_idle; }
    bool no_check_house() const { return m_no_check_house; }
    bool debug() const { return m_debug; }

    // --- Generic key-value interface (for CmdWizard dynamic config) ---
    void set(const char* key, int32_t val);
    void set(const char* key, const char* val);
    void del(const char* key);
    int32_t query(const char* key) const;
    const char* querystr(const char* key) const;

private:
    int m_ip_limit;
    int m_login_limit;
    bool m_kick_idle;
    bool m_no_check_house;
    bool m_debug;

    // Dynamic config for keys set via CmdWizard
    struct DynamicEntry {
        int32_t int_val;
        std::string str_val;
        bool has_str;
        DynamicEntry() : int_val(0), has_str(false) {}
    };
    mutable std::map<std::string, DynamicEntry> m_dynamic;
};

extern CCombatConfig g_combat_config;
