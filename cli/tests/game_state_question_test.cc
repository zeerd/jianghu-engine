#include <gtest/gtest.h>

#include "game_state.h"
#include "mock_connection.h"

// ==================== Test Fixture ====================
// Shares the same pattern as game_state_test.cc — SetUp logs in first.

class GameStateQuestionTest : public ::testing::Test {
protected:
    MockConnection mock_conn_;
    GameState state_;

    GameStateQuestionTest() : state_(mock_conn_) {}

    // Build a GameMessage with cmd and key=val params
    static GameMessage make_msg(
        int cmd,
        std::initializer_list<std::pair<std::string, std::string>> p = {})
    {
        GameMessage m;
        m.cmd = cmd;
        for (auto &[k, v] : p) m.params[k] = v;
        return m;
    }
};

// ==================== C=27 QUESTION — NPC Dialog ====================

TEST_F(GameStateQuestionTest, NpcDialog)
{
    auto out = state_.handle_message(make_msg(
        27, {{"N", "100"}, {"n", "铁匠"}, {"m", "你需要打造什么装备？"}}));

    EXPECT_TRUE(state_.in_dialog());
    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out[0].find("铁匠") != std::string::npos);
}

// ==================== C=27 QUESTION — Menu Building ====================

TEST_F(GameStateQuestionTest, BuildMenuThenEnd)
{
    // item 1
    state_.handle_message(make_msg(
        27, {{"N", "200"}, {"s", "购买武器"}, {"c", "$1 buy 1"}, {"b", "1"}}));
    EXPECT_FALSE(state_.in_menu());  // menu not complete yet

    // item 2
    state_.handle_message(
        make_msg(27, {{"N", "200"}, {"s", "购买防具"}, {"c", "$1 buy 2"}}));
    EXPECT_FALSE(state_.in_menu());

    // end marker
    auto out = state_.handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));

    EXPECT_TRUE(state_.in_menu());
    ASSERT_EQ(state_.menu_items().size(), 2u);
    EXPECT_EQ(state_.menu_items()[0].text, "购买武器");
    EXPECT_EQ(state_.menu_items()[1].text, "购买防具");
    ASSERT_FALSE(out.empty());
}

// ==================== Menu Actions ====================

TEST_F(GameStateQuestionTest, SelectMenuItemSendsCommand)
{
    state_.handle_message(
        make_msg(27, {{"N", "200"}, {"s", "购买武器"}, {"c", "$1 buy 1"}}));
    state_.handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));

    std::string cmd = state_.select_menu_item(0);
    EXPECT_EQ(cmd, " buy 1");  // $1 replaced with NPC ID 200
    EXPECT_FALSE(state_.in_menu());
}

TEST_F(GameStateQuestionTest, SelectInvalidIndexReturnsEmpty)
{
    state_.handle_message(
        make_msg(27, {{"N", "200"}, {"s", "购买武器"}, {"c", "$1 buy 1"}}));
    state_.handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));

    EXPECT_EQ(state_.select_menu_item(-1), "");
    EXPECT_EQ(state_.select_menu_item(5), "");
}

TEST_F(GameStateQuestionTest, SelectMenuItemTriggersPendingInput)
{
    // Menu item with $2 placeholder — needs user input
    state_.handle_message(
        make_msg(27, {{"N", "300"}, {"s", "交易物品"}, {"c", "$1 trade $2"}}));
    state_.handle_message(make_msg(27, {{"N", "300"}, {"e", "1"}}));

    // Selecting the item should not close the menu
    std::string cmd = state_.select_menu_item(0);
    EXPECT_EQ(cmd, "");  // waiting for more input
    EXPECT_TRUE(state_.is_waiting_for_menu_input());
    EXPECT_TRUE(state_.in_menu());
}

TEST_F(GameStateQuestionTest, CompletePendingCommand)
{
    state_.handle_message(
        make_msg(27, {{"N", "300"}, {"s", "交易"}, {"c", "$1 trade $2"}}));
    state_.handle_message(make_msg(27, {{"N", "300"}, {"e", "1"}}));
    state_.select_menu_item(0);

    std::string completed = state_.complete_pending_menu_command("sword");
    EXPECT_EQ(completed, " trade sword");
    EXPECT_FALSE(state_.in_menu());
    EXPECT_FALSE(state_.is_waiting_for_menu_input());
}

TEST_F(GameStateQuestionTest, CompletePendingWithMultipleParams)
{
    state_.handle_message(
        make_msg(27, {{"N", "400"}, {"s", "炼制"}, {"c", "$1 craft $2 $3"}}));
    state_.handle_message(make_msg(27, {{"N", "400"}, {"e", "1"}}));
    state_.select_menu_item(0);

    std::string completed
        = state_.complete_pending_menu_command("iron_sword 3");
    EXPECT_EQ(completed, " craft iron_sword 3");
}

TEST_F(GameStateQuestionTest, CancelMenu)
{
    state_.handle_message(
        make_msg(27, {{"N", "200"}, {"s", "选项1"}, {"c", "$1 cmd1"}}));
    state_.handle_message(make_msg(27, {{"N", "200"}, {"e", "1"}}));
    EXPECT_TRUE(state_.in_menu());

    state_.cancel_menu();
    EXPECT_FALSE(state_.in_menu());
    EXPECT_FALSE(state_.in_dialog());
    EXPECT_TRUE(state_.menu_items().empty());
}

TEST_F(GameStateQuestionTest, GetPendingPrompt)
{
    state_.handle_message(
        make_msg(27, {{"N", "300"}, {"s", "交易物品"}, {"c", "$1 trade $2"}}));
    state_.handle_message(make_msg(27, {{"N", "300"}, {"e", "1"}}));
    state_.select_menu_item(0);

    std::string prompt = state_.get_pending_menu_prompt();
    EXPECT_TRUE(prompt.find("交易物品") != std::string::npos);
    EXPECT_TRUE(prompt.find("参数1") != std::string::npos);
}

TEST_F(GameStateQuestionTest, EmptyMenuAutoCancels)
{
    // End marker with no items added
    state_.handle_message(make_msg(27, {{"N", "999"}, {"e", "1"}}));
    EXPECT_FALSE(state_.in_menu());
}

// ==================== find_target ====================

TEST_F(GameStateQuestionTest, FindTargetByNumericId)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "8001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));

    EXPECT_EQ(state_.find_target("8001"), 8001);
}

TEST_F(GameStateQuestionTest, FindTargetByName)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "8002"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));
    state_.handle_message(make_msg(
        17, {{"O", "8002"}, {"N", "店小二"}, {"I", "xiaoer"}, {"U", "0"}}));

    EXPECT_EQ(state_.find_target("店小二"), 8002);
}

TEST_F(GameStateQuestionTest, FindTargetByInternalId)
{
    state_.handle_message(make_msg(34, {{"U", "7001"},
                                        {"N", "李四"},
                                        {"I", "acc_lisi"},
                                        {"r", "5"},
                                        {"t", ""}}));

    EXPECT_EQ(state_.find_target("acc_lisi"), 7001);
}

TEST_F(GameStateQuestionTest, FindTargetPlayerSelf)
{
    // player_id is 1001 from fixture-free GameState
    MockConnection conn;
    GameState gs(conn);
    gs.handle_message(make_msg(18, {{"U", "1001"},
                                    {"G", "0"},
                                    {"V", "1"},
                                    {"R", "0"},
                                    {"M", "S"},
                                    {"l", "0"}}));

    auto out
        = gs.handle_message(make_msg(15, {{"T", "1001"}, {"p", "玩家张三"}}));

    EXPECT_EQ(gs.find_target("1001"), 1001);
    EXPECT_EQ(gs.find_target("玩家张三"), 1001);
}

TEST_F(GameStateQuestionTest, FindTargetNotFound)
{
    EXPECT_EQ(state_.find_target("nonexistent"), 0);
}

// ==================== Player info panels ====================

TEST_F(GameStateQuestionTest, PlayerInfoPanelsNonEmpty)
{
    auto panels = state_.player_info_panels();
    EXPECT_FALSE(panels.empty());
}

TEST_F(GameStateQuestionTest, PlayerInfoPanelByName)
{
    auto panel = state_.player_info_panel("character");
    EXPECT_FALSE(panel.empty()) << "character panel should return content";
}
