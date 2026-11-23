// 构建/运行（仓库根）：.\run-unit-tests.ps1
//
// bot 决策层是「WorldView 快照 → 动作」的纯函数：这里用固定 seed 的
// mt19937 对每类前置状态反复采样，断言产出的动作恒落在该状态允许的
// 集合内——限速、不打架的血线、菜单 6 轮兜底这些纪律全在 decide 里，
// 决策层错了 run_bot 救不回来。
#include "gtest/gtest.h"

#include "bot.h"

namespace {

using bot::Action;
using bot::MenuChoice;
using bot::WorldView;

// 全动作可用的基线快照：血满、有出口有怪有货有商人。
WorldView full_view()
{
    WorldView w;
    w.logged_in         = true;
    w.hp                = 100;
    w.max_hp            = 100;
    w.exit_count        = 3;
    w.npc_count         = 2;
    w.ground_item_count = 1;
    w.inventory_count   = 1;
    w.vendor_item_count = 2;
    return w;
}

// 对同一前置状态采样 n 次，返回命中过的动作集合（按枚举值去重）。
std::vector<Action> sample_actions(const WorldView& w, int n)
{
    std::mt19937 rng(20261123);
    std::vector<Action> seen;
    for (int i = 0; i < n; ++i) {
        Action a = bot::decide(w, rng);
        bool dup = false;
        for (Action x : seen) {
            if (x == a) {
                dup = true;
                break;
            }
        }
        if (!dup) seen.push_back(a);
    }
    return seen;
}

bool contains(const std::vector<Action>& v, Action a)
{
    for (Action x : v) {
        if (x == a) return true;
    }
    return false;
}

}  // namespace

// 四种硬门槛状态一律 Wait：未登录（发了也进不了游戏）、战斗中（服务端
// 回合自行推进，插命令只会撞限流）、忙碌（服务端拒收）、濒死（hp<=0
// 等 heal_up，打出去必死循环）。任何一个状态采出了非 Wait 即为回归。
TEST(BotDecide, HardGatesAlwaysWait)
{
    WorldView w = full_view();

    w.logged_in = false;
    EXPECT_EQ(bot::decide(w, std::mt19937(1)), Action::Wait);

    w           = full_view();
    w.in_battle = true;
    EXPECT_EQ(bot::decide(w, std::mt19937(2)), Action::Wait);

    w      = full_view();
    w.busy = true;
    EXPECT_EQ(bot::decide(w, std::mt19937(3)), Action::Wait);

    w    = full_view();
    w.hp = 0;
    EXPECT_EQ(bot::decide(w, std::mt19937(4)), Action::Wait);
}

// 血线纪律：hp 不足上限四成时不开新战（Fight 的 available=false），
// 但其余动作照常可用。采样中出现 Fight 即为放行回归。
TEST(BotDecide, LowHpNeverStartsFight)
{
    WorldView w = full_view();
    w.hp        = 30;  // 30% < 四成线
    for (Action a : sample_actions(w, 500)) {
        EXPECT_NE(a, Action::Fight) << "hp=30% 不该主动开新战";
    }
    // 但不至于全停：低血仍应有非战斗动作可选
    EXPECT_GT(sample_actions(w, 500).size(), 1u);
}

// 可用性门槛逐项生效：对应场景缺席时该动作绝不出现，其余照跑。
TEST(BotDecide, UnavailableActionsNeverChosen)
{
    WorldView w              = full_view();
    w.exit_count             = 0;  // 没出口不走
    w.ground_item_count      = 0;  // 没东西不捡
    w.inventory_count        = 0;  // 空背包不卖
    w.vendor_item_count      = 0;  // 没探到商人不做买卖
    std::vector<Action> seen = sample_actions(w, 500);
    EXPECT_FALSE(contains(seen, Action::Move));
    EXPECT_FALSE(contains(seen, Action::Get));
    EXPECT_FALSE(contains(seen, Action::Sell));
    EXPECT_FALSE(contains(seen, Action::Buy));

    WorldView no_npc          = full_view();
    no_npc.npc_count          = 0;
    std::vector<Action> seen2 = sample_actions(no_npc, 500);
    EXPECT_FALSE(contains(seen2, Action::Ask));
    EXPECT_FALSE(contains(seen2, Action::Fight));
    EXPECT_FALSE(contains(seen2, Action::Probe));
}

// 战斗是低频动作：全可用状态下权重 3/99 ≈ 3%。压测主力是走动/聊天等
// 日常负载，打架只作点缀——固定 seed 采样 2000 次，占比须压在 8% 以下
// （权重回调到两位数百分比时本用例即红）。
TEST(BotDecide, FightIsLowProbability)
{
    std::mt19937 rng(7);
    int fight_count = 0;
    const int total = 2000;
    for (int i = 0; i < total; ++i) {
        if (bot::decide(full_view(), rng) == Action::Fight) ++fight_count;
    }
    EXPECT_LT(fight_count * 100, total * 8)
        << "Fight 占比 " << (fight_count * 100 / total) << "% 超过 8%";
}

// 概率覆盖：全可用状态下 500 次采样应覆盖到动作表的绝大多数动作
// （含 Fight/Move/Sell 这些带门槛的），且永远不越出枚举语义（Wait 之外
// 的动作都要求各自前提，前面的用例已钉死；这里钉「不是全停」）。
TEST(BotDecide, FullViewCoversBroadActions)
{
    std::vector<Action> seen = sample_actions(full_view(), 500);
    EXPECT_TRUE(contains(seen, Action::Move));
    EXPECT_TRUE(contains(seen, Action::Fight));
    EXPECT_TRUE(contains(seen, Action::Look));
    EXPECT_GT(seen.size(), 6u);
}

// 全不可用（空房无出口，只剩 Look/Say/Chat 这类恒可用动作）不能崩、
// 不能返回垃圾值——恒可用三项之外无它。
TEST(BotDecide, BareViewOnlyAlwaysAvailableActions)
{
    WorldView w;
    w.logged_in = true;
    w.hp        = 100;
    w.max_hp    = 100;
    for (Action a : sample_actions(w, 300)) {
        EXPECT_TRUE(a == Action::Look || a == Action::Say || a == Action::Chat
                    || a == Action::Wait)
            << "bare view 不该选出带前提的动作";
    }
}

// 菜单决策契约：菜单开着且有条目 → Select；等自由文本 → Input；
// 6 轮上限一到无论何种态 → Cancel（兜底防陷菜单死循环）。
TEST(BotMenu, ChoiceByStateAndRoundCap)
{
    WorldView w;
    w.in_menu         = true;
    w.menu_item_count = 4;
    EXPECT_EQ(bot::decide_menu(w, 1), MenuChoice::Select);

    WorldView waiting;
    waiting.waiting_menu_input = true;
    EXPECT_EQ(bot::decide_menu(waiting, 1), MenuChoice::Input);

    // 菜单态但条目为空（e=1 尚未到齐的窗口）→ Cancel 而不是 Select(0)
    WorldView empty_menu;
    empty_menu.in_menu         = true;
    empty_menu.menu_item_count = 0;
    EXPECT_EQ(bot::decide_menu(empty_menu, 1), MenuChoice::Cancel);

    // 6 轮上限压过一切状态
    EXPECT_EQ(bot::decide_menu(w, 6), MenuChoice::Cancel);
    EXPECT_EQ(bot::decide_menu(waiting, 6), MenuChoice::Cancel);

    // 什么菜单态都没有 → Cancel（不该走到，防御）
    WorldView none;
    EXPECT_EQ(bot::decide_menu(none, 1), MenuChoice::Cancel);
}

// 延迟窗口钉死在声明的区间内：动作间隔 500~1500ms（均值口径 16 条约
// 10 秒，距服务端「16 条须间隔 ≥5 秒」的告警线余量充足）；菜单步
// 600~1200ms 单步单命令。
TEST(BotDelay, StaysInsideDeclaredWindows)
{
    std::mt19937 rng(42);
    for (int i = 0; i < 200; ++i) {
        int d = bot::next_delay_ms(rng);
        EXPECT_GE(d, 500);
        EXPECT_LE(d, 1500);
        int m = bot::menu_delay_ms(rng);
        EXPECT_GE(m, 600);
        EXPECT_LE(m, 1200);
    }
}
