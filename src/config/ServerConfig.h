#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

/**
 * @file ServerConfig.h
 * @brief 服务器通用玩法开关配置，从 config.ini 的 [server] section 读取。
 */

#include "IniParser.h"

/**
 * @brief 服务器玩法开关，控制内功加血、内功冲突、施法准备、
 *        背叛师门校验与转世（newlife）冲突检查等运行期行为。
 */
class CServerConfig {
public:
    // 初始化（服务器启动时调用一次）
    static void init();

    // 内功等级计入最大气血加成
    static bool force_add_hp() { return s_force_add_hp; }

    // 内功互冲突检查关闭（1=不检查内功冲突）
    static bool no_force_conflict() { return s_no_force_conflict; }

    // 施法不要求已准备内功（1=拥有任意内功即可施法）
    static bool force_no_need_prepare() { return s_force_no_need_prepare; }

    // 背叛师门者一律不得学艺（1=不再按技能差放行）
    static bool no_betrayer_valid() { return s_no_betrayer_valid; }

    // 转世（newlife）所学内功不参与冲突检查
    static bool newlife_no_conflict() { return s_newlife_no_conflict; }

private:
    static bool s_force_add_hp;
    static bool s_no_force_conflict;
    static bool s_force_no_need_prepare;
    static bool s_no_betrayer_valid;
    static bool s_newlife_no_conflict;
};

#endif  // SERVER_CONFIG_H
