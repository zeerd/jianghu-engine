// cli/tests/commands_test.cc — CommandHandler 命令解析与 wire 格式契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=CommandHandler*

#include "commands.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

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

class CommandHandlerTest : public ::testing::Test {
protected:
    MockConnection conn_;
    GameState state_;
    TerminalUI ui_;
    CommandHandler handler_;
    std::filesystem::path logs_dir_;

    CommandHandlerTest()
        : state_(conn_), handler_(state_, conn_, ui_)
    {
        logs_dir_ = std::filesystem::temp_directory_path()
                    / "jianghu_cli_commands_test_logs";
        ui_.set_logs_dir(logs_dir_.string());
    }

    ~CommandHandlerTest() override
    {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
    }

    void SetUp() override { login(); }

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
        conn_.clear_commands();
    }

    // 房间内增加一个已命名对象（NPC/玩家/地面物品）
    // iid = 对象英文 id（dbase"id"，协议 I= 字段）——/t 的合法参数
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
        m17.cmd             = 17;
        m17.params["O"]     = std::to_string(id);
        m17.params["N"]     = name;
        m17.params["a"]     = std::to_string(qty);
        if (!iid.empty()) m17.params["I"] = iid;
        state_.handle_message(m17);
    }

    // 背包中增加一个物品
    void add_inventory_item(ObjectID id, const std::string& name,
                            const std::string& iid = "", int qty = 1)
    {
        state_.handle_message(make_msg(
            11, {{"E", "1001"},
                 {"T", std::to_string(id)},
                 {"u", "0"},
                 {"i", "1"},
                 {"U", "0"}}));
        state_.handle_message(make_msg(
            17, {{"O", std::to_string(id)},
                 {"N", name},
                 {"I", iid},
                 {"a", std::to_string(qty)}}));
    }
};

// ==================== 入口与通用分发 ====================

TEST_F(CommandHandlerTest, EmptyInputReturnsFalse)
{
    EXPECT_FALSE(handler_.process(""));
}

TEST_F(CommandHandlerTest, GenericCommandWithoutLoginOmitsPlayerId)
{
    GameState fresh(conn_);  // 未登录的另一个状态
    CommandHandler fresh_handler(fresh, conn_, ui_);
    OutputCapture cap;
    EXPECT_TRUE(fresh_handler.process("look"));
    EXPECT_EQ(conn_.last_command(), "look 0\t");
}

TEST_F(CommandHandlerTest, GenericCommandPrependsPlayerId)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("look"));
    EXPECT_EQ(conn_.last_command(), "1001 look 0\t");
}

TEST_F(CommandHandlerTest, GenericCommandInsertsActiveTarget)
{
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("look"));
    EXPECT_EQ(conn_.last_command(), "1001 look 2002\t");
}

TEST_F(CommandHandlerTest, GenericCommandSplitsArgsAndInsertsNullTarget)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("say hi there"));
    EXPECT_EQ(conn_.last_command(), "1001 say 0 hi there\t");
}

TEST_F(CommandHandlerTest, GenericCommandWithTargetPrefixesArgs)
{
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("say hi"));
    EXPECT_EQ(conn_.last_command(), "1001 say 2002 hi\t");
}

// ==================== __target__ ====================

TEST_F(CommandHandlerTest, TargetNoArgsShowsCurrentAndUsage)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__"));
    std::string out = cap.str();
    EXPECT_NE(out.find("Current target: 0"), std::string::npos);
    EXPECT_NE(out.find("/t 0 or /t clear"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetZeroClearsTarget)
{
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__0"));
    EXPECT_EQ(state_.target_id(), 0);
    EXPECT_NE(cap.str().find("Target cleared"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetClearKeywordClearsTarget)
{
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__clear"));
    EXPECT_EQ(state_.target_id(), 0);
}

TEST_F(CommandHandlerTest, TargetAccountIdResolvesToPlayerId)
{
    state_.set_account_id("acc100");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__acc100"));
    EXPECT_EQ(state_.target_id(), 1001);
}

TEST_F(CommandHandlerTest, TargetEnglishIdFindsRoomObject)
{
    // /t 只收 ID：房间 NPC 按英文 id（dbase"id" / 协议 I= 字段）解析
    add_room_object(3001, "铁匠", /*is_user=*/true, /*is_item=*/false, 1,
                    "tie jiang");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__tie jiang"));
    EXPECT_EQ(state_.target_id(), 3001);
    EXPECT_NE(cap.str().find("Target set to 3001"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetDisplayNameRejected)
{
    // 显示名不是 /t 参数（全服允许重名）：按名定向必须失败
    add_room_object(3001, "铁匠", /*is_user=*/true, /*is_item=*/false, 1,
                    "tie jiang");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__铁匠"));
    EXPECT_EQ(state_.target_id(), 0);
    EXPECT_NE(cap.str().find("Target not found"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetPlayerAccountIdResolvesCrossRoom)
{
    // 玩家数字账号 ID 全服有效：在线表条目（DOWNUSER 回填）即可解析，
    // 即使该玩家不在当前房间
    state_.handle_message(make_msg(34, {{"U", "7001"},
                                        {"N", "路人甲"},
                                        {"I", "1396"},
                                        {"r", "1"},
                                        {"t", ""}}));
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__1396"));
    EXPECT_EQ(state_.target_id(), 7001);
}

TEST_F(CommandHandlerTest, TargetResolvedAfterOfflineRemoved)
{
    // SENDUSER O=0（下线广播）按 UID 从在线表移除 → 该账号 ID 不再可解析
    state_.handle_message(make_msg(34, {{"U", "7001"},
                                        {"N", "路人甲"},
                                        {"I", "1396"},
                                        {"r", "1"},
                                        {"t", ""}}));
    EXPECT_EQ(state_.find_target("1396"), 7001);

    state_.handle_message(make_msg(25, {{"O", "0"}, {"U", "7001"}}));
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__1396"));
    EXPECT_EQ(state_.target_id(), 0);
    EXPECT_NE(cap.str().find("Target not found"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetNotFoundKeepsZero)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__不存在的人"));
    EXPECT_EQ(state_.target_id(), 0);
    EXPECT_NE(cap.str().find("Target not found"), std::string::npos);
}

TEST_F(CommandHandlerTest, TargetArgAfterSpaceIsJoined)
{
    // 含空格的英文 id（/t 后整串都是参数）
    add_room_object(3001, "铁匠 大锤", /*is_user=*/true, /*is_item=*/false, 1,
                    "tie jiang da chui");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__target__tie jiang da chui"));
    EXPECT_EQ(state_.target_id(), 3001);
}

// ==================== __tune__ ====================

TEST_F(CommandHandlerTest, TuneNoArgsShowsCurrentChannel)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__tune__"));
    std::string out = cap.str();
    EXPECT_NE(out.find("Current channel: chat"), std::string::npos);
    EXPECT_NE(out.find("chat, new, ad"), std::string::npos);
}

// 切频道是纯客户端行为：不下发 wire "tune"——服务端 tune 语义是
// 「关闭接收频道」，下发会误关本端接收而 CLI 却显示"已切换"。
TEST_F(CommandHandlerTest, TuneSwitchUpdatesChannelWithoutWire)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__tune__new"));
    EXPECT_EQ(state_.current_channel(), "new");
    EXPECT_EQ(conn_.command_count(), 0);
    EXPECT_NE(cap.str().find("Channel switched to: new"), std::string::npos);
}

// ==================== __give__ ====================

TEST_F(CommandHandlerTest, GiveNoArgsShowsUsage)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("__give__"));
    EXPECT_NE(cap.str().find("Usage: /give"), std::string::npos);
}

TEST_F(CommandHandlerTest, GiveRequiresLogin)
{
    GameState fresh(conn_);  // 未登录的另一个状态
    CommandHandler fresh_handler(fresh, conn_, ui_);
    OutputCapture cap;
    EXPECT_TRUE(fresh_handler.process_give("sword"));
    EXPECT_NE(cap.str().find("Not logged in"), std::string::npos);
}

TEST_F(CommandHandlerTest, GiveRequiresTarget)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("sword"));
    EXPECT_NE(cap.str().find("No target set"), std::string::npos);
}

TEST_F(CommandHandlerTest, GiveItemMustBeInInventory)
{
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("火焰刀"));
    EXPECT_NE(cap.str().find("not found in inventory"), std::string::npos);
}

TEST_F(CommandHandlerTest, GiveSendsWireWithExplicitAmount)
{
    add_inventory_item(500, "sword");
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("sword 3"));
    EXPECT_EQ(conn_.last_command(), "1001 give 500 to 2002 3\t");
}

TEST_F(CommandHandlerTest, GiveStackableItemDefaultsToOne)
{
    add_inventory_item(500, "stone", "", /*qty=*/5);
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("stone"));
    EXPECT_EQ(conn_.last_command(), "1001 give 500 to 2002 1\t");
}

TEST_F(CommandHandlerTest, GiveCoinStackOmitsDefaultAmount)
{
    add_inventory_item(600, "coin", "", /*qty=*/9);
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("coin"));
    EXPECT_EQ(conn_.last_command(), "1001 give 600 to 2002\t");
}

TEST_F(CommandHandlerTest, GiveNonNumericSecondTokenBecomesItemName)
{
    add_inventory_item(500, "stone");
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("stone abc"));
    // 第二个 token 无法转数字时改写 item_type，按 "abc" 查背包失败
    EXPECT_NE(cap.str().find("'abc' not found"), std::string::npos);
}

TEST_F(CommandHandlerTest, GiveMatchesInternalId)
{
    add_inventory_item(500, "铁剑", "sword_01");
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_give("sword_01 1"));
    EXPECT_EQ(conn_.last_command(), "1001 give 500 to 2002 1\t");
}

// ==================== __sell__ ====================

TEST_F(CommandHandlerTest, SellItemNotInInventoryReturnsFalse)
{
    EXPECT_FALSE(handler_.process_sell("火焰刀"));
}

TEST_F(CommandHandlerTest, SellRequiresVendorTarget)
{
    add_inventory_item(500, "sword");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_sell("sword"));
    EXPECT_NE(cap.str().find("No vendor target"), std::string::npos);
}

TEST_F(CommandHandlerTest, SellSendsWireToVendor)
{
    add_inventory_item(500, "sword");
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_sell("sword"));
    EXPECT_EQ(conn_.last_command(), "1001 sell 2002 500\t");
}

TEST_F(CommandHandlerTest, SellMatchesInternalId)
{
    add_inventory_item(500, "铁剑", "sword_01");
    state_.set_target_id(2002);
    OutputCapture cap;
    EXPECT_TRUE(handler_.process_sell("sword_01"));
    EXPECT_EQ(conn_.last_command(), "1001 sell 2002 500\t");
}

// ==================== get ... from ... ====================

TEST_F(CommandHandlerTest, GetFromBuildsWireFormat)
{
    // get 助手经 find_target 解析（/t 契约：只收 ID）——物品/容器用英文 id
    add_room_object(500, "铁剑", /*is_user=*/false, /*is_item=*/true, 1,
                    "sample_sword_basic");
    add_room_object(600, "木箱", /*is_user=*/false, /*is_item=*/true, 1,
                    "mu xiang");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("get sample_sword_basic from mu xiang"));
    EXPECT_EQ(conn_.last_command(), "1001 get 500 from 600\t");
}

TEST_F(CommandHandlerTest, GetFromItemNotFound)
{
    add_room_object(600, "木箱", /*is_user=*/false, /*is_item=*/true, 1,
                    "mu xiang");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("get 火焰刀 from mu xiang"));
    EXPECT_NE(cap.str().find("Item not found: 火焰刀"), std::string::npos);
}

TEST_F(CommandHandlerTest, GetFromContainerNotFound)
{
    add_room_object(500, "铁剑", /*is_user=*/false, /*is_item=*/true, 1,
                    "sample_sword_basic");
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("get sample_sword_basic from 保险柜"));
    EXPECT_NE(cap.str().find("Container not found"), std::string::npos);
}

TEST_F(CommandHandlerTest, GetWithoutFromFallsThroughToGeneric)
{
    OutputCapture cap;
    EXPECT_TRUE(handler_.process("get sword"));
    EXPECT_EQ(conn_.last_command(), "1001 get 0 sword\t");
}
