// cli/tests/input_test.cc — InputProcessor 输入分流契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=InputProcessor*

#include "input.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

#include "commands.h"
#include "game_state.h"
#include "mock_connection.h"
#include "terminal_ui.h"

namespace {

// 把 UI 输出重定向到内存流：断言可见文本，同时抑制测试输出噪声。
class OutputCapture {
public:
    OutputCapture() : saved_(std::cout.rdbuf(out_.rdbuf())) {}
    ~OutputCapture() { std::cout.rdbuf(saved_); }
    std::string str() const { return out_.str(); }

private:
    std::stringstream out_;
    std::streambuf* saved_;
};

}  // namespace

class InputProcessorTest : public ::testing::Test {
protected:
    MockConnection conn_;
    GameState state_;
    TerminalUI ui_;
    CommandHandler cmd_handler_;
    InputProcessor input_;
    std::filesystem::path logs_dir_;

    InputProcessorTest()
        : state_(conn_),
          cmd_handler_(state_, conn_, ui_),
          input_(cmd_handler_, state_, conn_, ui_)
    {
        logs_dir_ = std::filesystem::temp_directory_path()
                    / "jianghu_cli_input_test_logs";
        ui_.set_logs_dir(logs_dir_.string());
    }

    ~InputProcessorTest() override
    {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
    }

    void SetUp() override
    {
        login();
        conn_.clear_commands();
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
        state_.handle_message(make_msg(18, {{"U", "1001"},
                                            {"G", "0"},
                                            {"V", "1.0"},
                                            {"R", "0"},
                                            {"M", "srv"},
                                            {"l", "0"},
                                            {"Y", "0"}}));
    }

    // 进入 5001 房间
    void enter_room()
    {
        state_.handle_message(make_msg(
            10, {{"R", "5001"},
                 {"I", "map_test"},
                 {"N", "测试房"},
                 {"F", "0"},
                 {"D", ""}}));
    }

    // 房间内增加一个已命名对象（iid = 英文 id，协议 I= 字段，/t 的合法参数）
    void add_room_object(ObjectID id, const std::string& name,
                         bool is_user = false, bool is_item = false,
                         int qty = 1, const std::string& iid = "")
    {
        state_.handle_message(make_msg(
            11, {{"E", "5001"},
                 {"T", std::to_string(id)},
                 {"u", is_user ? "1" : "0"},
                 {"i", is_item ? "1" : "0"},
                 {"U", "0"}}));
        GameMessage m17;
        m17.cmd         = 17;
        m17.params["O"] = std::to_string(id);
        m17.params["N"] = name;
        m17.params["a"] = std::to_string(qty);
        if (!iid.empty()) m17.params["I"] = iid;
        state_.handle_message(m17);
    }

    // 打开一个单项菜单
    void open_menu(const std::string& cmd_text,
                   const std::string& item_text = "购买武器")
    {
        state_.handle_message(
            make_msg(27, {{"N", "200"}, {"s", item_text}, {"c", cmd_text}}));
        state_.handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));
    }

    // 打开无选项的对话
    void open_dialog()
    {
        state_.handle_message(make_msg(
            27, {{"N", "100"}, {"n", "铁匠"}, {"m", "需要什么？"}}));
    }
};

// ==================== CLI 命令 ====================

TEST_F(InputProcessorTest, EmptyInputIsIgnored)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line(""));
    EXPECT_EQ(conn_.command_count(), 0);
}

TEST_F(InputProcessorTest, CliQuitSetsFlagAndStopsUi)
{
    {
        OutputCapture cap;
        EXPECT_TRUE(input_.process_line(".quit"));
    }
    EXPECT_TRUE(input_.should_quit());
    EXPECT_FALSE(ui_.is_running());

    input_.clear_quit();
    EXPECT_FALSE(input_.should_quit());
}

TEST_F(InputProcessorTest, CliExitStopsUi)
{
    {
        OutputCapture cap;
        EXPECT_TRUE(input_.process_line(".exit"));
    }
    EXPECT_TRUE(input_.should_quit());
    EXPECT_FALSE(ui_.is_running());
}

TEST_F(InputProcessorTest, CliHelpListsCommandSections)
{
    OutputCapture cap;
    input_.process_line(".help");
    std::string out = cap.str();
    EXPECT_NE(out.find("=== Commands ==="), std::string::npos);
    EXPECT_NE(out.find("=== CLI Commands ==="), std::string::npos);
}

TEST_F(InputProcessorTest, CliStatusShowsStatusLine)
{
    enter_room();
    OutputCapture cap;
    input_.process_line(".status");
    std::string out = cap.str();
    EXPECT_NE(out.find("测试房"), std::string::npos);
    EXPECT_NE(out.find("HP:"), std::string::npos);
}

TEST_F(InputProcessorTest, CliInfoShowsPanels)
{
    OutputCapture cap;
    input_.process_line(".info");
    EXPECT_NE(cap.str().find("人物信息"), std::string::npos);
}

TEST_F(InputProcessorTest, CliInventoryShowsInventoryPanel)
{
    OutputCapture cap;
    input_.process_line(".inventory");
    EXPECT_NE(cap.str().find("携带物品"), std::string::npos);
}

TEST_F(InputProcessorTest, CliSkillsEmptyShowsHint)
{
    OutputCapture cap;
    input_.process_line(".skills");
    EXPECT_NE(cap.str().find("技能列表"), std::string::npos);
    EXPECT_NE(cap.str().find("(无技能)"), std::string::npos);
}

TEST_F(InputProcessorTest, CliSkillsListsLevelPotentialAndPrepare)
{
    state_.handle_message(make_msg(13, {{"T", "1"},
                                        {"S", "huashan_jianfa"},
                                        {"L", "5"},
                                        {"P", "2"},
                                        {"M", "sword"},
                                        {"K", "1"}}));
    state_.handle_message(make_msg(16, {{"S", "huashan_jianfa"},
                                        {"N", "华山剑法"},
                                        {"E", "sword"}}));
    conn_.clear_commands();

    OutputCapture cap;
    input_.process_line(".skills");
    std::string out = cap.str();
    EXPECT_NE(out.find("华山剑法"), std::string::npos);
    EXPECT_NE(out.find("Lv.5"), std::string::npos);
    EXPECT_NE(out.find("潜能:2"), std::string::npos);
    EXPECT_NE(out.find("[准备]"), std::string::npos);
    EXPECT_NE(out.find("-> sword"), std::string::npos);
}

TEST_F(InputProcessorTest, CliObjectsEmptyShowsPlaceholder)
{
    OutputCapture cap;
    input_.process_line(".objects");
    EXPECT_NE(cap.str().find("(******)"), std::string::npos);
}

TEST_F(InputProcessorTest, CliObjectsListsNpcsCharsAndItems)
{
    enter_room();
    add_room_object(2001, "店小二");  // 默认标志 = NPC
    add_room_object(2002, "过客甲", /*is_user=*/true);
    add_room_object(3001, "包子", /*is_user=*/false, /*is_item=*/true,
                    /*qty=*/3);

    OutputCapture cap;
    input_.process_line(".objects");
    std::string out = cap.str();
    EXPECT_NE(out.find("NPCs:"), std::string::npos);
    EXPECT_NE(out.find("Players:"), std::string::npos);
    EXPECT_NE(out.find("Items:"), std::string::npos);
    EXPECT_NE(out.find("店小二"), std::string::npos);
    EXPECT_NE(out.find("过客甲"), std::string::npos);
    EXPECT_NE(out.find("包子 x3"), std::string::npos);
}

TEST_F(InputProcessorTest, CliCharsEmptyShowsPlaceholder)
{
    OutputCapture cap;
    input_.process_line(".chars");
    EXPECT_NE(cap.str().find("(******)"), std::string::npos);
}

TEST_F(InputProcessorTest, CliCharsListsNpcsAndPlayers)
{
    enter_room();
    add_room_object(2001, "店小二", /*is_user=*/true);
    add_room_object(2002, "过客甲", /*is_user=*/true);

    OutputCapture cap;
    input_.process_line(".chars");
    std::string out = cap.str();
    EXPECT_NE(out.find("=== Characters ==="), std::string::npos);
    EXPECT_NE(out.find("店小二"), std::string::npos);
    EXPECT_NE(out.find("过客甲"), std::string::npos);
}

TEST_F(InputProcessorTest, CliDoorsEmptyShowsNone)
{
    enter_room();
    OutputCapture cap;
    input_.process_line(".doors");
    std::string out = cap.str();
    EXPECT_NE(out.find("测试房"), std::string::npos);
    EXPECT_NE(out.find("无"), std::string::npos);
}

TEST_F(InputProcessorTest, CliDoorsListsExits)
{
    enter_room();
    state_.handle_message(make_msg(24, {{"R", "5001"},
                                        {"D", "north"},
                                        {"A", "正北"},
                                        {"N", "武当山"},
                                        {"R2", "6001"}}));

    OutputCapture cap;
    input_.process_line(".doors");
    EXPECT_NE(cap.str().find("正北 -> 武当山"), std::string::npos);
}

TEST_F(InputProcessorTest, CliLookShowsEmptyScene)
{
    enter_room();
    OutputCapture cap;
    input_.process_line(".look");
    std::string out = cap.str();
    EXPECT_NE(out.find("=== 测试房 ==="), std::string::npos);
    EXPECT_NE(out.find("(无)"), std::string::npos);
}

TEST_F(InputProcessorTest, CliLookShowsCharsItemsAndExits)
{
    enter_room();
    add_room_object(2001, "店小二", /*is_user=*/true);
    add_room_object(3001, "包子", /*is_item=*/true);
    state_.handle_message(make_msg(24, {{"R", "5001"},
                                        {"D", "north"},
                                        {"A", "正北"},
                                        {"N", "武当山"}}));

    OutputCapture cap;
    input_.process_line(".look");
    std::string out = cap.str();
    EXPECT_NE(out.find("店小二"), std::string::npos);
    EXPECT_NE(out.find("包子"), std::string::npos);
    EXPECT_NE(out.find("正北 -> 武当山"), std::string::npos);
}

TEST_F(InputProcessorTest, CliItemsEmptyShowsPlaceholder)
{
    OutputCapture cap;
    input_.process_line(".items");
    EXPECT_NE(cap.str().find("(******)"), std::string::npos);
}

TEST_F(InputProcessorTest, CliItemsListsGroundItems)
{
    enter_room();
    add_room_object(3001, "包子", /*is_user=*/false, /*is_item=*/true,
                    /*qty=*/3);

    OutputCapture cap;
    input_.process_line(".items");
    EXPECT_NE(cap.str().find("包子 x3"), std::string::npos);
}

TEST_F(InputProcessorTest, CliUnknownCommandShowsHint)
{
    OutputCapture cap;
    input_.process_line(".bogus");
    std::string out = cap.str();
    EXPECT_NE(out.find("Unknown CLI command: .bogus"), std::string::npos);
    EXPECT_NE(out.find("CLI commands start with '.'"), std::string::npos);
}

// ==================== /login /register ====================

TEST_F(InputProcessorTest, LoginWithCredentialsSetsAccount)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/login acc1 pwd1"));
    EXPECT_EQ(state_.account_id(), "acc1");
    EXPECT_NE(cap.str().find("Logging in..."), std::string::npos);
}

TEST_F(InputProcessorTest, LoginMissingPasswordIsIgnored)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/login acc1"));
    EXPECT_EQ(state_.account_id(), "");
    EXPECT_EQ(cap.str().find("Logging in..."), std::string::npos);
}

TEST_F(InputProcessorTest, RegisterWithoutAccountShowsUsage)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/register"));
    EXPECT_NE(cap.str().find("Usage: /register"), std::string::npos);
}

TEST_F(InputProcessorTest, RegisterWithAccountAttemptsSend)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/register acc1 pwd1 M 21 21 21 21"));
    // MockConnection 未连接，send_register 返回 false，不显示 Registering
    EXPECT_EQ(cap.str().find("Usage: /register"), std::string::npos);
}

// ============ /t /give /sell 委托与 .tune 本地切换 ============

TEST_F(InputProcessorTest, TargetDelegationShowsCurrent)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/t"));
    EXPECT_NE(cap.str().find("Current target: 0"), std::string::npos);
}

TEST_F(InputProcessorTest, TargetDelegationSetsById)
{
    // /t 只收 ID：房间对象按英文 id 定向（显示名不做参数）
    add_room_object(2001, "店小二", /*is_user=*/true, /*is_item=*/false, 1,
                    "xiaoer");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/t xiaoer"));
    EXPECT_EQ(state_.target_id(), 2001);

    // 显示名不是 /t 参数：报未找到且保留原目标
    EXPECT_TRUE(input_.process_line("/t 店小二"));
    EXPECT_EQ(state_.target_id(), 2001);
    EXPECT_NE(cap.str().find("Target not found"), std::string::npos);
}

TEST_F(InputProcessorTest, LocalTuneShowsChannels)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line(".tune"));
    EXPECT_NE(cap.str().find("Current channel: chat"), std::string::npos);
}

TEST_F(InputProcessorTest, LocalTuneSwitchesChannel)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line(".tune new"));
    EXPECT_EQ(state_.current_channel(), "new");
    // 本地切换不下发任何 wire（服务端 tune 是「关闭接收频道」的另一语义）
    EXPECT_EQ(conn_.command_count(), 0);
    EXPECT_NE(cap.str().find("Channel switched to: new"), std::string::npos);
}

// 前缀约定：`.`=本地命令、`/`=服务器命令。/tune 直发服务端 tune，
// 真正关闭该频道的接收（由服务端回执确认）。
TEST_F(InputProcessorTest, SlashTuneReachesServerTune)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/tune chat"));
    EXPECT_EQ(conn_.last_command(), "1001 tune 0 chat\t");
}

TEST_F(InputProcessorTest, GiveDelegationShowsUsage)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/give"));
    EXPECT_NE(cap.str().find("Usage: /give"), std::string::npos);
}

TEST_F(InputProcessorTest, GiveDelegationSendsWire)
{
    state_.handle_message(make_msg(
        11, {{"E", "1001"}, {"T", "500"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    state_.handle_message(
        make_msg(17, {{"O", "500"}, {"N", "sword"}, {"a", "1"}}));
    state_.set_target_id(2002);

    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/give sword"));
    EXPECT_EQ(conn_.last_command(), "1001 give 500 to 2002\t");
}

TEST_F(InputProcessorTest, SellDelegationSendsWire)
{
    state_.handle_message(make_msg(
        11, {{"E", "1001"}, {"T", "500"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    state_.handle_message(
        make_msg(17, {{"O", "500"}, {"N", "sword"}, {"a", "1"}}));
    state_.set_target_id(2002);

    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/sell sword"));
    EXPECT_EQ(conn_.last_command(), "1001 sell 2002 500\t");
}

// ==================== 其他服务器命令与聊天 ====================

TEST_F(InputProcessorTest, OtherServerCommandForwarded)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("/look"));
    EXPECT_EQ(conn_.last_command(), "1001 look 0\t");
}

TEST_F(InputProcessorTest, PlainTextChatsOnCurrentChannel)
{
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("hello world"));
    EXPECT_EQ(conn_.last_command(), "1001 chat 0 hello world\t");
}

TEST_F(InputProcessorTest, PlainTextUsesSwitchedChannel)
{
    state_.set_current_channel("tell");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("hello"));
    EXPECT_EQ(conn_.last_command(), "1001 tell 0 hello\t");
}

// ==================== 菜单/对话交互 ====================

TEST_F(InputProcessorTest, MenuChoiceSendsResolvedCommand)
{
    open_menu("$1 buy 1");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("1"));
    EXPECT_TRUE(conn_.last_command().find("buy 1") != std::string::npos);
    EXPECT_FALSE(state_.in_menu());
}

TEST_F(InputProcessorTest, MenuChoiceZeroCancels)
{
    open_menu("$1 buy 1");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("0"));
    EXPECT_FALSE(state_.in_menu());
    EXPECT_EQ(conn_.command_count(), 0);
    EXPECT_NE(cap.str().find("Interaction cancelled"), std::string::npos);
}

TEST_F(InputProcessorTest, MenuInvalidChoiceReports)
{
    open_menu("$1 buy 1");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("9"));
    EXPECT_NE(cap.str().find("Invalid choice"), std::string::npos);
    EXPECT_TRUE(state_.in_menu()) << "非法选择后菜单保持打开";
}

TEST_F(InputProcessorTest, MenuNonNumericFallsThroughToChat)
{
    open_menu("$1 buy 1");
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("你好"));
    EXPECT_FALSE(state_.in_menu());
    EXPECT_TRUE(conn_.last_command().find("chat 0 你好") != std::string::npos);
}

TEST_F(InputProcessorTest, DialogNumericChoiceFallsThroughToChat)
{
    open_dialog();
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("5"));
    EXPECT_FALSE(state_.in_dialog());
    EXPECT_TRUE(conn_.last_command().find("chat 0 5") != std::string::npos);
}

TEST_F(InputProcessorTest, DialogNonNumericCancelsAndChats)
{
    open_dialog();
    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("你好"));
    EXPECT_FALSE(state_.in_dialog());
    EXPECT_TRUE(conn_.last_command().find("chat 0 你好") != std::string::npos);
}

TEST_F(InputProcessorTest, PendingMenuInputCompletesAndSends)
{
    open_menu("$1 trade $2");
    {
        OutputCapture cap;
        input_.process_line("1");
    }
    EXPECT_TRUE(state_.is_waiting_for_menu_input());

    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("sword"));
    EXPECT_FALSE(state_.is_waiting_for_menu_input());
    EXPECT_TRUE(conn_.last_command().find("trade sword") != std::string::npos);
}

TEST_F(InputProcessorTest, PendingMenuInputStillWaitingShowsPrompt)
{
    open_menu("$1 craft $2 $3");
    {
        OutputCapture cap;
        input_.process_line("1");
    }
    ASSERT_TRUE(state_.is_waiting_for_menu_input());

    OutputCapture cap;
    EXPECT_TRUE(input_.process_line("part_a"));
    // 还有占位符未填：继续等待，不发送任何命令
    EXPECT_TRUE(state_.is_waiting_for_menu_input());
    EXPECT_EQ(conn_.command_count(), 0);
    EXPECT_FALSE(cap.str().empty()) << "应重显提示语";
}
