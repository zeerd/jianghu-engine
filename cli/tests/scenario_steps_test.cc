// cli/tests/scenario_steps_test.cc — 无头剧本步骤执行器契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=ScenarioSteps*
//
// 覆盖 scenario_steps.cc：run_step 全部 verb、条件求值（eval_cond /
// eval_attr_cond）、菜单选择（run_menu_step）与文本捕获（run_capture_step）。

#include "scenario_runner.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "client_core.h"
#include "config.h"
#include "game_state.h"
#include "mock_connection.h"
#include "protocol.h"

namespace {

// 测试用子类：暴露 protected 的执行入口与内部状态。
class TestableScenarioRunner : public CScenarioRunner {
public:
    using CScenarioRunner::CScenarioRunner;
    using CScenarioRunner::attr_snapshot;
    using CScenarioRunner::attr_value;
    using CScenarioRunner::eval_attr_cond;
    using CScenarioRunner::eval_cond;
    using CScenarioRunner::run_capture_step;
    using CScenarioRunner::run_menu_step;
    using CScenarioRunner::run_step;
    using CScenarioRunner::subst;

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
        logs_dir_ = std::filesystem::temp_directory_path()
                    / "jianghu_cli_scenario_steps_test_logs";
        std::filesystem::create_directories(logs_dir_);
        cfg_.logs_dir = logs_dir_.string();

        auto mock = std::make_unique<MockConnection>();
        conn_     = mock.get();
        core_     = std::make_unique<ClientCore>(cfg_, std::move(mock));

        opts_.echo              = false;
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
    void open_menu(const std::string& cmd_text = "$1 buy 1",
                   const std::string& item_text = "购买武器")
    {
        core_->state().handle_message(make_msg(
            27, {{"N", "200"}, {"s", item_text}, {"c", cmd_text}}));
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

TEST_F(ScenarioStepsTest, EvalCondMenuAndDialog)
{
    auto runner = make_runner();
    std::string detail;

    open_menu();
    EXPECT_TRUE(runner.eval_cond("menu", detail));
    core_->state().cancel_menu();

    core_->state().handle_message(make_msg(
        27, {{"N", "100"}, {"n", "铁匠"}, {"m", "要什么？"}}));
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
    core_->state().handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "客栈"}, {"F", "0"}}));
    core_->state().handle_message(make_msg(
        11, {{"E", "5001"}, {"T", "2001"}, {"u", "1"}, {"i", "0"}, {"U", "0"}}));
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

    EXPECT_TRUE(runner.eval_cond("wield 铁剑 右手", detail)) << "物品名子串匹配";
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
    auto& a   = core_->state().mutable_player_attr();
    a.exp     = 100;

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
    a.combat_exp = 1; a.hp = 2; a.max_hp = 3;
    a.mp = 4; a.max_mp = 5; a.level = 6; a.shen = 7; a.eff_hp = 8;
    EXPECT_TRUE(runner.eval_attr_cond("combat_exp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("max_hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("eff_hp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("mp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("max_mp >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("level >= 0", detail));
    EXPECT_TRUE(runner.eval_attr_cond("shen >= 0", detail));
}

// eff_hp（有效气血上限）与 max_hp 是两回事：升级只抬 max_hp，eff_hp 会滞后，
// 「金创药把 eff_hp 补回 max_hp」这条测试点就靠它判定。
TEST_F(ScenarioStepsTest, EvalAttrCondEffHpTracksEffectiveCap)
{
    auto &a = core_->state().mutable_player_attr();
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
    auto runner = make_runner();
    std::string detail;
    EXPECT_TRUE(runner.eval_cond("attr exp >= 50", detail));
    EXPECT_FALSE(runner.eval_cond("attr exp > 50", detail));
}

// 属性断言超时必须带回**当前值**：只回显条件的话，失败时分不清是
//「状态还没到位」还是「状态变了却没同步到客户端」——后者才是真 bug。
TEST_F(ScenarioStepsTest, WaitAttrTimeoutReportsCurrentValue)
{
    core_->state().mutable_player_attr().hp = 489;
    auto runner = make_runner();

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
    auto runner = make_runner();

    EXPECT_EQ(runner.attr_snapshot("attr eff_hp < 489"), "eff_hp=363");
    EXPECT_EQ(runner.attr_snapshot("text 你好"), "")
        << "非属性条件不附加快照";
    EXPECT_EQ(runner.attr_snapshot("attr eff_hp"), "")
        << "缺运算符与值时不猜";
    EXPECT_EQ(runner.attr_snapshot("attr nosuch >= 1"), "")
        << "未知属性名给不出快照";
}

// 非属性断言超时不附加「（当前 …）」，失败原因保持原样可被既有断言匹配
TEST_F(ScenarioStepsTest, WaitTextTimeoutKeepsPlainReason)
{
    auto runner = make_runner();
    EXPECT_FALSE(runner.run_step({"wait", "text 根本不存在 timeout=80"}));
    EXPECT_NE(runner.fail_reason().find("timeout waiting for 'text 根本不存在'"),
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
    EXPECT_NE(runner.fail_reason().find("bad condition"),
              std::string::npos);
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
    EXPECT_NE(runner.fail_reason().find("no menu is open"),
              std::string::npos);

    EXPECT_FALSE(runner.run_menu_step(""));
    EXPECT_NE(runner.fail_reason().find("menu needs"),
              std::string::npos);
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
    EXPECT_NE(runner.fail_reason().find("capture needs"),
              std::string::npos);

    EXPECT_FALSE(runner.run_capture_step("编号是 as "));
    EXPECT_NE(runner.fail_reason().find("capture needs"),
              std::string::npos);
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

// ==================== subst ====================

TEST_F(ScenarioStepsTest, SubstReplacesKnownVarsOnly)
{
    auto runner                = make_runner();
    runner.mutable_vars()["x"] = "7";
    EXPECT_EQ(runner.subst("a${x}b"), "a7b");
    EXPECT_EQ(runner.subst("a${missing}b"), "a${missing}b");
}
