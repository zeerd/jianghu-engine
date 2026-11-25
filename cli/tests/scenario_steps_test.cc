// cli/tests/scenario_steps_test.cc — 无头剧本步骤执行器契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=x64
//   bin\cli\x64\jianghu_cli_tests.exe --gtest_filter=ScenarioSteps*
//
// 覆盖 scenario_steps.cc：run_step 全部 verb、条件求值（eval_cond /
// eval_attr_cond）、菜单选择（run_menu_step）与文本捕获（run_capture_step）。

#include "scenario_runner.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "client_core.h"
#include "config.h"
#include "game_state.h"
#include "mock_connection.h"
#include "protocol.h"
#include "test_tmp_dir.h"

namespace {

// 测试用子类：暴露 protected 的执行入口与内部状态。
class TestableScenarioRunner : public CScenarioRunner {
public:
    using CScenarioRunner::attr_snapshot;
    using CScenarioRunner::attr_value;
    using CScenarioRunner::CScenarioRunner;
    using CScenarioRunner::eval_attr_cond;
    using CScenarioRunner::eval_cond;
    using CScenarioRunner::run_capture_step;
    using CScenarioRunner::run_menu_step;
    using CScenarioRunner::run_step;
    using CScenarioRunner::strip_token;
    using CScenarioRunner::subst;
    using CScenarioRunner::wait_cond;

    const std::string& fail_reason() const { return fail_reason_; }
    std::map<std::string, std::string>& mutable_vars() { return vars_; }
};

// 把 NOTIFY 文本打包成注入连接的原始 wire 消息（&C=19&M=..& + GBK + XOR）。
std::string make_text_msg(const std::string& text)
{
    std::string body = "&C=19&M=" + text + "&";
    return protocol::encode_str(protocol::utf8_to_gbk(body)) + "\t";
}

}  // namespace

class ScenarioStepsTest : public ::testing::Test {
protected:
    Config cfg_;
    std::filesystem::path logs_dir_;
    std::unique_ptr<ClientCore> core_;
    ScenarioOptions opts_;
    MockConnection* conn_ = nullptr;

    void SetUp() override
    {
        logs_dir_ = test_tmp_dir() / "jianghu_cli_scenario_steps_test_logs";
        std::filesystem::create_directories(logs_dir_);
        cfg_.logs_dir = logs_dir_.string();

        auto mock = std::make_unique<MockConnection>();
        conn_     = mock.get();
        core_     = std::make_unique<ClientCore>(cfg_, std::move(mock));

        opts_.echo               = false;
        opts_.default_timeout_ms = 1000;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
    }

    TestableScenarioRunner make_runner()
    {
        return TestableScenarioRunner(*core_, opts_);
    }

    static GameMessage make_msg(
        int cmd,
        std::initializer_list<std::pair<std::string, std::string>> p = {})
    {
        GameMessage m;
        m.cmd = cmd;
        for (auto& [k, v] : p) m.params[k] = v;
        return m;
    }

    // 登录为 1001 号角色
    void login()
    {
        core_->state().handle_message(make_msg(18, {{"U", "1001"},
                                                    {"G", "0"},
                                                    {"V", "1.0"},
                                                    {"R", "0"},
                                                    {"M", "srv"},
                                                    {"l", "0"},
                                                    {"Y", "0"}}));
        conn_->clear_commands();
    }

    // 注入一条 NOTIFY 文本到输出快照
    void inject_text(const std::string& text)
    {
        conn_->inject_received(make_text_msg(text));
        core_->process_received();
    }

    // 打开一个单项菜单
    void open_menu(const std::string& cmd_text  = "$1 buy 1",
                   const std::string& item_text = "购买武器")
    {
        core_->state().handle_message(
            make_msg(27, {{"N", "200"}, {"s", item_text}, {"c", cmd_text}}));
        core_->state().handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));
    }
};

// ==================== eval_cond ====================

TEST_F(ScenarioStepsTest, EvalCondEmptyFails)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("   ", detail));
    EXPECT_NE(detail.find("empty condition"), std::string::npos);
}

TEST_F(ScenarioStepsTest, EvalCondAlwaysTrue)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("always", detail));
}

TEST_F(ScenarioStepsTest, EvalCondLoginRequiresPlayerId)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("login", detail));

    login();
    EXPECT_TRUE(runner.eval_cond("login", detail));
}

TEST_F(ScenarioStepsTest, EvalCondFlagsReflectState)
{
    auto runner = make_runner();
    std::string detail;

    EXPECT_FALSE(runner.eval_cond("battle", detail));
    EXPECT_TRUE(runner.eval_cond("battle_end", detail));
    core_->state().set_battle_flag_for_test(true);
    EXPECT_TRUE(runner.eval_cond("battle", detail));
    EXPECT_FALSE(runner.eval_cond("battle_end", detail));
}

// busy / not_busy：两个方向都从真实的 STATE 报文驱动（不加测试专用 setter），
// `busy` 等起始、`not_busy` 等结束，正是剧本「等到不忙」的用法。
TEST_F(ScenarioStepsTest, EvalCondBusyFollowsState)
{
    login();
    auto runner = make_runner();
    std::string detail;

    EXPECT_TRUE(runner.eval_cond("not_busy", detail));
    EXPECT_FALSE(runner.eval_cond("busy", detail));

    core_->state().handle_message(make_msg(23, {{"T", "1001"}, {"bsy", "1"}}));
    EXPECT_TRUE(runner.eval_cond("busy", detail));
    EXPECT_FALSE(runner.eval_cond("not_busy", detail));
}

// 起始锁存：忙窗可窄于轮询（起始/结束 STATE 可能紧邻），bsy=1 在接收线程
// 即时记账；wait busy 命中即消费，wait not_busy 收口也消费，不串进下一次命令。
TEST_F(ScenarioStepsTest, BusyLatchSurvivesFlipUntilConsumed)
{
    login();
    auto runner = make_runner();
    std::string detail;

    core_->state().handle_message(make_msg(23, {{"T", "1001"}, {"bsy", "1"}}));
    core_->state().handle_message(make_msg(23, {{"T", "1001"}, {"sta", "60"}}));
    EXPECT_TRUE(runner.eval_cond("busy", detail))
        << "结束 STATE 翻回后锁存仍在";
    EXPECT_TRUE(runner.eval_cond("not_busy", detail)) << "实时已不忙";

    EXPECT_TRUE(runner.wait_cond("busy", 50)) << "锁存命中";
    EXPECT_FALSE(runner.eval_cond("busy", detail)) << "命中即消费";

    core_->state().handle_message(make_msg(23, {{"T", "1001"}, {"bsy", "1"}}));
    core_->state().handle_message(make_msg(23, {{"T", "1001"}, {"sta", "60"}}));
    EXPECT_TRUE(runner.wait_cond("not_busy", 50)) << "收口即消费";
    EXPECT_FALSE(runner.eval_cond("busy", detail)) << "收口后不残留";
}

TEST_F(ScenarioStepsTest, EvalCondMenuAndDialog)
{
    auto runner = make_runner();
    std::string detail;

    open_menu();
    EXPECT_TRUE(runner.eval_cond("menu", detail));
    core_->state().cancel_menu();

    core_->state().handle_message(
        make_msg(27, {{"N", "100"}, {"n", "铁匠"}, {"m", "要什么？"}}));
    EXPECT_TRUE(runner.eval_cond("dialog", detail));
}

TEST_F(ScenarioStepsTest, EvalCondRoomMatchesNameMapOrId)
{
    core_->state().handle_message(make_msg(
        10, {{"R", "5001"}, {"I", "map_x"}, {"N", "武当"}, {"F", "0"}}));

    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("room 武当", detail));
    EXPECT_TRUE(runner.eval_cond("room map_x", detail));
    EXPECT_TRUE(runner.eval_cond("room 5001", detail));
    EXPECT_FALSE(runner.eval_cond("room 峨眉", detail));
}

TEST_F(ScenarioStepsTest, EvalCondTextNeedsNonEmptyNeedle)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("text", detail));
    EXPECT_NE(detail.find("needs a needle"), std::string::npos);
    EXPECT_FALSE(runner.eval_cond("text |||", detail));
}

TEST_F(ScenarioStepsTest, EvalCondNotextInvertsTextMatch)
{
    inject_text("欢迎来到新手频道");
    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("notext 年纪太小", detail));
    EXPECT_FALSE(runner.eval_cond("notext 新手", detail));
    EXPECT_FALSE(runner.eval_cond("notext 未知|新手", detail));
    EXPECT_TRUE(runner.eval_cond("notext 未知|没有", detail));
}

TEST_F(ScenarioStepsTest, EvalCondNotextNeedsNeedle)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("notext", detail));
    EXPECT_NE(detail.find("needs a needle"), std::string::npos);
    EXPECT_FALSE(runner.eval_cond("notext |||", detail));
}

// notext 的 wait 语义：全程监视，不该出现的文本一出现立即失败
TEST_F(ScenarioStepsTest, RunStepWaitNotextFailsWhenTextPresent)
{
    inject_text("你的年纪太小了，只能用新手频道说话。");
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"wait", "notext 年纪太小 timeout=500"}));
    EXPECT_NE(runner.fail_reason().find("unexpected text"), std::string::npos);
}

// notext 的 wait 语义：坚持到期满无出现即通过
TEST_F(ScenarioStepsTest, RunStepWaitNotextPassesWhenAbsent)
{
    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"wait", "notext 年纪太小 timeout=120"}));
}

TEST_F(ScenarioStepsTest, EvalCondObjectMatchesRoomObject)
{
    core_->state().handle_message(
        make_msg(10, {{"R", "5001"}, {"I", ""}, {"N", "客栈"}, {"F", "0"}}));
    core_->state().handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "2001"}, {"u", "1"}, {"i", "0"}, {"U", "0"}}));
    core_->state().handle_message(
        make_msg(17, {{"O", "2001"}, {"N", "店小二"}}));

    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("object 店小二", detail));
    EXPECT_TRUE(runner.eval_cond("object 店小", detail)) << "子串匹配";
    EXPECT_FALSE(runner.eval_cond("object 不存在", detail));
}

TEST_F(ScenarioStepsTest, EvalCondItemMatchesInventory)
{
    login();
    core_->state().handle_message(make_msg(
        11, {{"E", "1001"}, {"T", "500"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    core_->state().handle_message(
        make_msg(17, {{"O", "500"}, {"N", "铁剑"}, {"I", "sword_01"}}));

    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("item 铁剑", detail));
    EXPECT_TRUE(runner.eval_cond("item sword_01", detail));
    EXPECT_FALSE(runner.eval_cond("item 大刀", detail));
}

// wield <物品> <槽位>：断言物品真的落进了指定槽位。
// 只看装备回执文本是不够的——回执不说明落在哪个槽位。
TEST_F(ScenarioStepsTest, EvalCondWieldChecksSlot)
{
    login();
    core_->state().handle_message(make_msg(
        11, {{"E", "1001"}, {"T", "500"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    core_->state().handle_message(
        make_msg(17, {{"O", "500"}, {"N", "铁剑"}, {"I", "sword_01"}}));

    auto runner = make_runner();
    std::string detail;

    // 未装备（wield=0）时任何槽位都不算数
    EXPECT_FALSE(runner.eval_cond("wield 铁剑 右手", detail));
    EXPECT_FALSE(runner.eval_cond("wield 铁剑 未知", detail));

    // RTNITEM 把对象 500 的槽位置为 7（右手）
    core_->state().handle_message(make_msg(14, {{"I0", "500"}, {"E0", "7"}}));

    EXPECT_TRUE(runner.eval_cond("wield 铁剑 右手", detail))
        << "物品名子串匹配";
    EXPECT_TRUE(runner.eval_cond("wield sword_01 右手", detail)) << "按内部 id";
    EXPECT_TRUE(runner.eval_cond("wield 铁剑 7", detail)) << "接受数字槽位";
    EXPECT_FALSE(runner.eval_cond("wield 铁剑 左手", detail)) << "槽位不符";
    EXPECT_FALSE(runner.eval_cond("wield 大刀 右手", detail)) << "物品不在背包";

    // 语法错误要给出可读原因
    EXPECT_FALSE(runner.eval_cond("wield 铁剑", detail));
    EXPECT_NE(detail.find("wield condition needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, EvalCondUnknownKeyReportsDetail)
{
    auto runner = make_runner();
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("zzz value", detail));
    EXPECT_NE(detail.find("unknown condition key"), std::string::npos);
}

// ==================== eval_attr_cond ====================

TEST_F(ScenarioStepsTest, EvalAttrCondSyntaxAndNameErrors)
{
    auto runner = make_runner();
    std::string detail;

    EXPECT_FALSE(runner.eval_attr_cond("exp >=", detail));
    EXPECT_NE(detail.find("syntax"), std::string::npos);

    EXPECT_FALSE(runner.eval_attr_cond("foo >= 1", detail));
    EXPECT_NE(detail.find("unknown attr"), std::string::npos);

    EXPECT_FALSE(runner.eval_attr_cond("exp ~~ 1", detail)) << "未知运算符";
    EXPECT_NE(detail.find("unknown attr operator"), std::string::npos);
}

TEST_F(ScenarioStepsTest, EvalAttrCondComparesAllOperators)
{
    auto& a = core_->state().mutable_player_attr();
    a.exp   = 100;

    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_attr_cond("exp == 100", detail));
    EXPECT_TRUE(runner.eval_attr_cond("exp != 99", detail));
    EXPECT_TRUE(runner.eval_attr_cond("exp >= 100", detail));
    EXPECT_TRUE(runner.eval_attr_cond("exp <= 100", detail));
    EXPECT_TRUE(runner.eval_attr_cond("exp > 99", detail));
    EXPECT_TRUE(runner.eval_attr_cond("exp < 101", detail));
    EXPECT_FALSE(runner.eval_attr_cond("exp > 100", detail));

    // 全部支持的属性名都可解析（此处仅验证解析不报错）
    a.combat_exp = 1;
    a.hp         = 2;
    a.max_hp     = 3;
    a.mp         = 4;
    a.max_mp     = 5;
    a.level      = 6;
    a.shen       = 7;
    a.eff_hp     = 8;
    EXPECT_TRUE(runner.eval_attr_cond("combat_exp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("max_hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("eff_hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("mp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("max_mp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("level >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("shen >= 0", detail));
}

// 四维与 damage 来自 RTNINFO 的 E/F/G/H/J（四维是含 apply/* 装备修正的
// 合成值）：注册四维落点与装备临时属性的断言都读这里，值与字段必须对得上。
TEST_F(ScenarioStepsTest, EvalAttrCondFourDimAndDamage)
{
    auto& a  = core_->state().mutable_player_attr();
    a.str    = 24;
    a.con    = 23;
    a.intel  = 22;
    a.dex    = 21;
    a.damage = 10;

    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_attr_cond("str == 24", detail));
    EXPECT_TRUE(runner.eval_attr_cond("con == 23", detail));
    EXPECT_TRUE(runner.eval_attr_cond("int == 22", detail));
    EXPECT_TRUE(runner.eval_attr_cond("dex == 21", detail));
    EXPECT_TRUE(runner.eval_attr_cond("damage == 10", detail));
    EXPECT_FALSE(runner.eval_attr_cond("str == 23", detail));

    EXPECT_EQ(runner.attr_snapshot("attr str == 24"), "str=24");
    EXPECT_EQ(runner.attr_snapshot("attr damage >= 1"), "damage=10");
}

// eff_hp（有效气血上限）与 max_hp 是两回事：升级只抬 max_hp，eff_hp 会滞后，
// 「金创药把 eff_hp 补回 max_hp」这条测试点就靠它判定。
TEST_F(ScenarioStepsTest, EvalAttrCondEffHpTracksEffectiveCap)
{
    auto& a  = core_->state().mutable_player_attr();
    a.hp     = 363;
    a.max_hp = 489;
    a.eff_hp = 363;

    auto runner = make_runner();
    std::string detail;

    // 升级后：最大上限 489、有效上限仍停在 363
    EXPECT_TRUE(runner.eval_cond("attr max_hp >= 489", detail));
    EXPECT_TRUE(runner.eval_cond("attr eff_hp < 489", detail));
    EXPECT_FALSE(runner.eval_cond("attr eff_hp >= 489", detail));

    // 补满后：有效上限追上最大上限（且与当前 hp 区分开）
    a.eff_hp = 489;
    EXPECT_TRUE(runner.eval_cond("attr eff_hp >= 489", detail));
    EXPECT_FALSE(runner.eval_cond("attr eff_hp < 489", detail));
    EXPECT_TRUE(runner.eval_cond("attr hp < 489", detail))
        << "hp 未满时不应被 eff_hp 的断言误判";
}

TEST_F(ScenarioStepsTest, EvalCondAttrDispatchesToAttrEval)
{
    core_->state().mutable_player_attr().exp = 50;
    auto runner                              = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("attr exp >= 50", detail));
    EXPECT_FALSE(runner.eval_cond("attr exp > 50", detail));
}

// 属性断言超时必须带回**当前值**：只回显条件的话，失败时分不清是
// 「状态还没到位」还是「状态变了却没同步到客户端」——后者才是真 bug。
TEST_F(ScenarioStepsTest, WaitAttrTimeoutReportsCurrentValue)
{
    core_->state().mutable_player_attr().hp = 489;
    auto runner                             = make_runner();

    EXPECT_FALSE(runner.run_step({"wait", "attr hp < 489 timeout=80"}));
    EXPECT_NE(runner.fail_reason().find("timeout waiting for 'attr hp < 489'"),
              std::string::npos);
    EXPECT_NE(runner.fail_reason().find("hp=489"), std::string::npos)
        << "失败原因要带出当前值";

    // 属性名写错时仍走「unknown attr」，不误报成超时
    std::string detail;
    EXPECT_FALSE(runner.eval_cond("attr nosuch >= 1", detail));
    EXPECT_NE(detail.find("unknown attr"), std::string::npos);
}

// attr_snapshot 的边界：只有形如 `attr <已知名> <op> <值>` 才给得出快照，
// 其余一律空串（前置判定已由 eval_cond 拦掉，这里是防御分支的直接覆盖）。
TEST_F(ScenarioStepsTest, AttrSnapshotOnlyForWellFormedAttrCond)
{
    core_->state().mutable_player_attr().eff_hp = 363;
    auto runner                                 = make_runner();

    EXPECT_EQ(runner.attr_snapshot("attr eff_hp < 489"), "eff_hp=363");
    EXPECT_EQ(runner.attr_snapshot("text 你好"), "") << "非属性条件不附加快照";
    EXPECT_EQ(runner.attr_snapshot("attr eff_hp"), "") << "缺运算符与值时不猜";
    EXPECT_EQ(runner.attr_snapshot("attr nosuch >= 1"), "")
        << "未知属性名给不出快照";
}

// 非属性断言超时不附加「（当前 …）」，失败原因保持原样可被既有断言匹配
TEST_F(ScenarioStepsTest, WaitTextTimeoutKeepsPlainReason)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"wait", "text 根本不存在 timeout=80"}));
    EXPECT_NE(
        runner.fail_reason().find("timeout waiting for 'text 根本不存在'"),
        std::string::npos);
    EXPECT_EQ(runner.fail_reason().find("（当前"), std::string::npos);
}

// ==================== run_step：verb 分发 ====================

TEST_F(ScenarioStepsTest, RunStepLoginNeedsTwoArgs)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"login", "acc1"}));
    EXPECT_NE(runner.fail_reason().find("login needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepLoginProcessesInput)
{
    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"login", "acc1 pwd1"}));
    EXPECT_EQ(core_->state().account_id(), "acc1");
}

TEST_F(ScenarioStepsTest, RunStepCmdAndMenuinputForwardInput)
{
    login();
    auto runner = make_runner();

    EXPECT_TRUE(runner.run_step({"cmd", "hello"}));
    EXPECT_EQ(conn_->last_command(), "1001 chat 0 hello\t");

    conn_->clear_commands();
    EXPECT_TRUE(runner.run_step({"menuinput", "world"}));
    EXPECT_EQ(conn_->last_command(), "1001 chat 0 world\t");
}

TEST_F(ScenarioStepsTest, RunStepWaitHonorsTimeoutSuffix)
{
    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"wait", "always timeout=100"}));
}

TEST_F(ScenarioStepsTest, RunStepWaitUnsatisfiableTimesOut)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"wait", "battle timeout=40"}));
    EXPECT_NE(runner.fail_reason().find("timeout"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepWaitBadConditionFails)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"wait", "zzz"}));
    EXPECT_NE(runner.fail_reason().find("bad condition"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepSleepAcceptsMillisAndRejectsGarbage)
{
    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"sleep", "0"}));
    EXPECT_FALSE(runner.run_step({"sleep", "abc"}));
    EXPECT_NE(runner.fail_reason().find("sleep needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepSetStoresTrimmedValue)
{
    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"set", "name  测试 小侠 "}));
    EXPECT_EQ(runner.mutable_vars().at("name"), "测试 小侠");
}

TEST_F(ScenarioStepsTest, RunStepSetWithoutValueFails)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"set", "lonely"}));
    EXPECT_NE(runner.fail_reason().find("set needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepClearEmptiesOutputSnapshot)
{
    inject_text("旧的输出");
    ASSERT_FALSE(core_->recent_output().empty());

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"clear", ""}));
    EXPECT_TRUE(core_->recent_output().empty());
}

TEST_F(ScenarioStepsTest, RunStepUnknownVerbFails)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"bogus", ""}));
    EXPECT_NE(runner.fail_reason().find("unknown step verb"),
              std::string::npos);
}

// ==================== run_menu_step ====================

TEST_F(ScenarioStepsTest, RunMenuStepRequiresOpenMenu)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_menu_step("1"));
    EXPECT_NE(runner.fail_reason().find("no menu is open"), std::string::npos);

    EXPECT_FALSE(runner.run_menu_step(""));
    EXPECT_NE(runner.fail_reason().find("menu needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunMenuStepSelectsByIndexAndText)
{
    login();
    open_menu("$1 buy 1", "购买武器");

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_menu_step("1"));
    EXPECT_TRUE(conn_->last_command().find("buy 1") != std::string::npos);
    EXPECT_FALSE(core_->state().in_menu());

    conn_->clear_commands();
    open_menu("$1 buy 2", "购买防具");
    EXPECT_TRUE(runner.run_menu_step("防具")) << "按文本子串匹配";
    EXPECT_TRUE(conn_->last_command().find("buy 2") != std::string::npos);
}

TEST_F(ScenarioStepsTest, RunMenuStepOutOfRangeFails)
{
    open_menu();
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_menu_step("9"));
    EXPECT_NE(runner.fail_reason().find("menu item not found"),
              std::string::npos);
}

// ==================== run_capture_step ====================

TEST_F(ScenarioStepsTest, RunCaptureStepRequiresMarkerAndVar)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_capture_step("编号是"));
    EXPECT_NE(runner.fail_reason().find("capture needs"), std::string::npos);

    EXPECT_FALSE(runner.run_capture_step("编号是 as "));
    EXPECT_NE(runner.fail_reason().find("capture needs"), std::string::npos);
}

TEST_F(ScenarioStepsTest, RunCaptureStepExtractsDigitsAfterMarker)
{
    inject_text("你的编号是 12345。");

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_capture_step("编号是 as acct"));
    EXPECT_EQ(runner.mutable_vars().at("acct"), "12345");
}

TEST_F(ScenarioStepsTest, RunCaptureStepFailsWhenMarkerMissing)
{
    inject_text("无关的输出");

    auto runner = make_runner();
    EXPECT_FALSE(runner.run_capture_step("不存在的标记 as var"));
    EXPECT_NE(runner.fail_reason().find("capture marker not found"),
              std::string::npos);
}

// marker 按正则解释（design §7 契约）：含捕获组取组文本（非数字变量），
// 无组保持「匹配后取首段数字」（存量账号 capture 用法）。
TEST_F(ScenarioStepsTest, RunCaptureStepRegexGroupCapturesText)
{
    inject_text("  你看起来约十八岁。生得英俊潇洒，风流倜傥。武功出神入化。");

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_capture_step("(生得.*?。) as waimao"))
        << runner.fail_reason();
    EXPECT_EQ(runner.mutable_vars().at("waimao"), "生得英俊潇洒，风流倜傥。");
}

TEST_F(ScenarioStepsTest, RunCaptureStepInvalidRegexFails)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_capture_step("([ as var"));
    EXPECT_NE(runner.fail_reason().find("capture invalid regex"),
              std::string::npos);
}

// ==================== wait text … consume ====================
// 触发式服务链的地基：车道把要执行的命令逐条 tell 给巫师，巫师只跑一段
// `wait … consume → capture → /t → cmd` 的统一循环。三个不变量：
//   ① wait 只认**消费游标之后**最早的一条，命中即把游标推过该行 ——
//      同一批滞留缓冲的请求按到达顺序被逐条吃掉（存量 wait text 不消费，
//      循环里会永远撞回同一条）；
//   ② 行文本存给紧跟的 capture 读（命中行已被游标丢出缓冲）；
//   ③ capture 只读那一行，绝不退回全缓冲倒序扫去抓更晚的请求。

namespace {
CScenarioRunner::Step sce_step(const std::string& verb, const std::string& args)
{
    CScenarioRunner::Step s;
    s.verb = verb;
    s.args = args;
    return s;
}
}  // namespace

TEST_F(ScenarioStepsTest, WaitTextConsumeAdvancesPastEachHit)
{
    inject_text("#W# gongneng 1001 /call set level 45");
    inject_text("#W# gongneng 1002 /call set level 45");

    auto runner = make_runner();
    EXPECT_TRUE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=1000")))
        << runner.fail_reason();
    EXPECT_EQ(core_->output_cursor(), 1u);

    EXPECT_TRUE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=1000")))
        << runner.fail_reason();
    EXPECT_EQ(core_->output_cursor(), 2u);

    // 全吃干净了 → 第三次只能等到超时，而不是又抓回前两条
    EXPECT_FALSE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=60")));
    EXPECT_NE(runner.fail_reason().find("timeout"), std::string::npos);
}

TEST_F(ScenarioStepsTest, WaitTextConsumeFeedsCaptureFromTheHitLine)
{
    inject_text("#W# gongneng 1001 /call set level 45");
    inject_text("#W# gongneng 1002 /call set mp 10000");

    auto runner = make_runner();
    EXPECT_TRUE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=1000")))
        << runner.fail_reason();

    EXPECT_TRUE(runner.run_capture_step("#W# \\S+ (\\d+) as acct"));
    EXPECT_EQ(runner.mutable_vars().at("acct"), "1001");

    EXPECT_TRUE(runner.run_capture_step("#W# \\S+ \\d+ (.+) as wcmd"));
    EXPECT_EQ(runner.mutable_vars().at("wcmd"), "/call set level 45");
}

TEST_F(ScenarioStepsTest, CaptureAfterConsumeRefusesOtherLines)
{
    inject_text("#W# gongneng 1001 /call set level 45");
    inject_text("别处提到账号是 999");

    auto runner = make_runner();
    EXPECT_TRUE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=1000")))
        << runner.fail_reason();

    EXPECT_FALSE(runner.run_capture_step("账号是 as acct"));
    EXPECT_NE(runner.fail_reason().find("consumed line"), std::string::npos);
}

// 没带 consume 的存量剧本必须原样走「全缓冲倒序扫」那条路，零行为变化。
TEST_F(ScenarioStepsTest, CaptureWithoutConsumeKeepsBackwardScan)
{
    inject_text("先到的编号是 111。");
    inject_text("后到的编号是 222。");

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step(sce_step("wait", "text 编号是 timeout=1000")));
    EXPECT_TRUE(runner.run_capture_step("编号是 as acct"));
    EXPECT_EQ(runner.mutable_vars().at("acct"), "222");
}

TEST_F(ScenarioStepsTest, WaitConsumeOnlyValidOnText)
{
    inject_text("无关的输出");

    auto runner = make_runner();
    EXPECT_FALSE(
        runner.run_step(sce_step("wait", "notext 不存在 consume timeout=100")));
    EXPECT_NE(runner.fail_reason().find("not allowed on notext"),
              std::string::npos);

    EXPECT_FALSE(
        runner.run_step(sce_step("wait", "room 书房 consume timeout=100")));
    EXPECT_NE(runner.fail_reason().find("only supported on 'wait text'"),
              std::string::npos);
}

// consume 只摘独立单词：needle 里含 consumes 这类子串不能被误伤。
TEST_F(ScenarioStepsTest, StripTokenRemovesOnlyStandaloneWord)
{
    std::string tail = "text abc consume";
    EXPECT_TRUE(TestableScenarioRunner::strip_token(tail, "consume"));
    EXPECT_EQ(tail, "text abc");

    std::string head = "consume text abc";
    EXPECT_TRUE(TestableScenarioRunner::strip_token(head, "consume"));
    EXPECT_EQ(head, " text abc");  // run_step 随后会 trim

    std::string sub = "text consumes";
    EXPECT_FALSE(TestableScenarioRunner::strip_token(sub, "consume"));
    EXPECT_EQ(sub, "text consumes");
}

// clear 要连消费游标一起归到末尾：否则清空后游标还停在旧位置，
// recent_output 会算出 `start > size`、读越界。
TEST_F(ScenarioStepsTest, ClearOutputAlsoResetsConsumeCursor)
{
    inject_text("#W# gongneng 1001 /call set level 45");

    auto runner = make_runner();
    EXPECT_TRUE(
        runner.run_step(sce_step("wait", "text #W# consume timeout=1000")));
    EXPECT_EQ(core_->output_cursor(), 1u);
    EXPECT_TRUE(core_->recent_output().empty());

    EXPECT_TRUE(runner.run_step(sce_step("clear", "")));
    EXPECT_EQ(core_->output_cursor(), core_->output_seq());
    EXPECT_TRUE(core_->recent_output().empty());
}

// ==================== subst ====================

TEST_F(ScenarioStepsTest, SubstReplacesKnownVarsOnly)
{
    auto runner                = make_runner();
    runner.mutable_vars()["x"] = "7";
    EXPECT_EQ(runner.subst("a${x}b"), "a7b");
    EXPECT_EQ(runner.subst("a${missing}b"), "a${missing}b");
}

// ==================== filecopy / filedelete ====================
// 热重载剧本的文件准备动词。写入钉死在测试暂存区（test_tmp/ 或 tmp/）：
// 剧本一旦能把落点写进 lua-sample / lua-default 等教材或生产目录，就等于
// 给被测世界开了个污染口。

TEST_F(ScenarioStepsTest, RunStepFilecopyWritesIntoScratchArea)
{
    auto root = test_tmp_dir() / "jianghu_cli_filecopy_test";
    std::filesystem::create_directories(root);
    auto src = root / "variant_a.lua";
    auto dst = root / "scripts" / "npc" / "target.lua";
    {
        std::ofstream out(src);
        out << "return { a = 1 }";
    }

    auto runner = make_runner();
    // 目标经 $var 替换（E2E 剧本正是这样引用私有脚本副本）
    runner.mutable_vars()["dst_dir"] = (root / "scripts" / "npc").string();
    EXPECT_TRUE(
        runner.run_step({"filecopy", src.string() + " $dst_dir/target.lua"}))
        << runner.fail_reason();

    std::string body;
    {
        // 读完立即关闭句柄：Windows 下开着句柄 remove_all 会被文件锁拒绝
        std::ifstream in(dst);
        body.assign((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    }
    EXPECT_EQ(body, "return { a = 1 }");

    // 覆盖写：目标已存在时直接覆盖，不报错
    EXPECT_TRUE(
        runner.run_step({"filecopy", src.string() + " " + dst.string()}))
        << runner.fail_reason();

    std::filesystem::remove_all(root);
}

TEST_F(ScenarioStepsTest, RunStepFilecopyMissingSourceFails)
{
    auto root = test_tmp_dir() / "jianghu_cli_filecopy_test";
    std::filesystem::create_directories(root);

    auto runner = make_runner();
    EXPECT_FALSE(
        runner.run_step({"filecopy", (root / "nope.lua").string() + " "
                                         + (root / "dst.lua").string()}));
    EXPECT_NE(runner.fail_reason().find("source not found"), std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_F(ScenarioStepsTest, RunStepFileWriteOutsideScratchAreaRefused)
{
    auto runner = make_runner();
    EXPECT_FALSE(
        runner.run_step({"filecopy", "anywhere.lua lua-sample/pollute.lua"}));
    EXPECT_NE(runner.fail_reason().find("file write refused"),
              std::string::npos)
        << "教材目录必须被拒";

    EXPECT_FALSE(runner.run_step({"filedelete", "lua-default/command/x.lua"}));
    EXPECT_NE(runner.fail_reason().find("file write refused"),
              std::string::npos);

    EXPECT_FALSE(runner.run_step({"filedelete", "src/main.cpp"}));
    EXPECT_NE(runner.fail_reason().find("file write refused"),
              std::string::npos);
}

TEST_F(ScenarioStepsTest, RunStepFiledeleteRemovesAndRequiresExisting)
{
    auto root = test_tmp_dir() / "jianghu_cli_filedelete_test";
    std::filesystem::create_directories(root);
    auto victim = root / "victim.lua";
    {
        std::ofstream out(victim);
        out << "x";
    }

    auto runner = make_runner();
    EXPECT_TRUE(runner.run_step({"filedelete", victim.string()}))
        << runner.fail_reason();
    EXPECT_FALSE(std::filesystem::exists(victim));

    // 目标不存在即失败：路径笔误必须立刻暴露，不许静默通过
    EXPECT_FALSE(runner.run_step({"filedelete", victim.string()}));
    EXPECT_NE(runner.fail_reason().find("not found"), std::string::npos);

    std::filesystem::remove_all(root);
}
