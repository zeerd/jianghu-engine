#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

/**
 * @file ServerConfig.h
 * @brief 服务器通用玩法开关配置，从 config.ini 的 [server] section 读取。
 */

#include "IniParser.h"

/**
 * @brief 服务器玩法开关，控制内功加血、内功冲突、施法准备与
 *        背叛师门校验等运行期行为。
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

    // 指令限流（1=16条/5秒边界丢弃；0=关闭，E2E 集成测试用）
    static bool rate_limit() { return s_rate_limit; }

private:
    static bool s_force_add_hp;
    static bool s_no_force_conflict;
    static bool s_force_no_need_prepare;
    static bool s_no_betrayer_valid;
    static bool s_rate_limit;
};

#endif  // SERVER_CONFIG_H
