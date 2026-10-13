#ifndef PARTY_CONFIG_H
#define PARTY_CONFIG_H

/**
 * @file PartyConfig.h
 * @brief 组队系统配置管理，从 config.ini [party] section 读取配置。
 */

#include "IniParser.h"

/**
 * @brief 组队系统配置，提供最大人数/经验共享/跟随战斗/等级差等参数。
 */
class CPartyConfig {
public:
    // 初始化（服务器启动时调用一次）
    static void init();

    // 队伍最大人数（包括队长）
    static int max_members() { return s_max_members; }

    // 是否启用组队经验共享
    // true  = 按原逻辑分享经验
    // false = 只要组队了，所有人（包括队长）都不得经验
    static bool share_exp() { return s_share_exp; }

    // 队员跟随队长自动进入战斗
    static bool auto_follow_fight() { return s_auto_follow_fight; }

    // 夫妻组队允许的等级差
    static int couple_level_diff() { return s_couple_level_diff; }

private:
    static int s_max_members;
    static bool s_share_exp;
    static bool s_auto_follow_fight;
    static int s_couple_level_diff;
};

#endif  // PARTY_CONFIG_H