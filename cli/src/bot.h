#pragma once

#include <cstdint>
#include <random>
#include <string>

class ClientCore;

// 压力测试 bot：登录后在动作权重表里随机选动作循环执行（走/看/说/聊/
// 打架/拾取/买卖），供人工观察服务器卡顿。决策层是纯数据函数（WorldView
// 快照 → 动作），与连接/UI 解耦，可直接单测；发包与菜单回灌在 run_bot。
namespace bot {

/// 决策可见的世界状态：由 GameState 快照而来的纯数据。
struct WorldView {
    bool logged_in          = false;
    bool in_battle          = false;
    bool busy               = false;
    bool in_menu            = false;
    bool waiting_menu_input = false;
    int menu_item_count     = 0;
    int exit_count          = 0;
    int npc_count           = 0;
    int ground_item_count   = 0;
    int inventory_count     = 0;
    int vendor_item_count   = 0;  // 已 /list 探明的货架条数（0=目标非商人）
    int64_t hp              = 0;
    int64_t max_hp          = 0;
};

/// 随机行动类别。
enum class Action {
    Wait,   // 无可行动作或战斗/忙碌/濒死——只等
    Move,   // 随机出口
    Look,   // 看房间
    Say,    // 房间说话
    Chat,   // 频道发言
    Ask,    // /ask 随机 NPC
    Fight,  // /kill 随机 NPC（输赢无所谓）
    Get,    // 拾取地上物品
    Probe,  // /list 探测随机 NPC 是否商人
    Sell,   // 向已确认商人卖背包物品
    Buy,    // 从已确认商人买一件
};

/// 行动决策：按权重在「当前可用」的动作里随机挑一个。战斗中、忙碌、
/// 登录前、濒死（hp<=0）一律 Wait；hp 低于上限两成时不开新战（heal_up
/// 每拍回血，等得起）。所有分支都必须可被输入条件复现——权重表是唯一
/// 决策依据，execute 侧不再做二次筛选。
Action decide(const WorldView& w, std::mt19937& rng);

/// 菜单态决策。menu_rounds 是本次连续处理菜单的轮数：达 6 轮仍未脱出
/// 即 Cancel 兜底，防止 bot 陷在多层菜单里空转（cancel_menu 是本地
/// 关闭，不发包）。
enum class MenuChoice { Select, Input, Cancel };
MenuChoice decide_menu(const WorldView& w, int menu_rounds);

/// 动作间隔（ms）。500~1500ms 均匀随机：单周期最多 2 条命令，均值
/// 约 1.5 条/秒。服务端限流是「每 16 条须间隔 ≥5 秒」——均值口径下
/// 16 条约需 10 秒，距 5 秒线余量充足（告警需连续 4 次才封号）。
int next_delay_ms(std::mt19937& rng, int min_ms = 500, int max_ms = 1500);

/// 菜单步间隔（ms）。菜单单步单命令，600~1200ms 保证 5 秒窗口 ≤8 条。
int menu_delay_ms(std::mt19937& rng, int min_ms = 600, int max_ms = 1200);

}  // namespace bot

/// bot 主循环：等自动登录 → 随机行动直到断线/停止。
/// 返回码：0=正常停止；2=登录失败或超时（凭据问题，调用方不重试）；
/// 3=服务器断开（调用方退避后可重建 ClientCore 重连）。
int run_bot(ClientCore& core);
