#include "game_state.h"

#include <gtest/gtest.h>

#include "mock_connection.h"
#include "protocol.h"

// Provide the definition for the global debug flag (defined in main.cc).
std::atomic<bool> g_dbg_enabled{false};

// ==================== Test Fixture ====================

class GameStateTest : public ::testing::Test {
protected:
    MockConnection mock_conn_;
    GameState state_;

    GameStateTest() : state_(mock_conn_) {}

    void SetUp() override
    {
        // Login to set player_id_ (needed so map_changed() etc. send commands)
        state_.handle_message(make_msg(18, {{"U", "1001"},
                                            {"G", "1"},
                                            {"V", "1.0"},
                                            {"R", "12345"},
                                            {"M", "TestServer"},
                                            {"l", "10"},
                                            {"Y", "0"}}));
        mock_conn_.clear_commands();
    }

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

// ==================== C=10 GOTOROOM ====================

TEST_F(GameStateTest, EnterRoom)
{
    auto out = state_.handle_message(make_msg(10, {{"R", "5001"},
                                                   {"I", "map_wudang"},
                                                   {"N", "武当山"},
                                                   {"F", "0"},
                                                   {"D", "east"}}));

    EXPECT_EQ(state_.current_room(), 5001);
    EXPECT_EQ(state_.room_name(), "武当山");
    EXPECT_EQ(state_.room_map(), "map_wudang");
    EXPECT_FALSE(state_.in_battle());
    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out[0].find("武当山") != std::string::npos);
}

TEST_F(GameStateTest, EnterBattleRoom)
{
    state_.handle_message(make_msg(
        10, {{"R", "5002"}, {"I", ""}, {"N", "战场"}, {"F", "1"}, {"D", ""}}));

    EXPECT_TRUE(state_.in_battle());
}

TEST_F(GameStateTest, EnterRoomClearsObjects)
{
    // Add an object first
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "2001"}, {"u", "1"}, {"i", "0"}, {"U", "0"}}));
    // Change room — objects should be cleared
    state_.handle_message(make_msg(
        10,
        {{"R", "5002"}, {"I", ""}, {"N", "新房间"}, {"F", "0"}, {"D", ""}}));

    EXPECT_TRUE(state_.room_npcs().empty());
    EXPECT_TRUE(state_.room_chars().empty());
}

TEST_F(GameStateTest, EnterRoomSendsLocCommand)
{
    state_.handle_message(make_msg(10, {{"R", "5010"},
                                        {"I", "map_test"},
                                        {"N", "Test"},
                                        {"F", "0"},
                                        {"D", ""}}));

    // Should trigger map_changed() → "1001 loc 0 12 12\t"
    ASSERT_FALSE(mock_conn_.sent_commands.empty());
    EXPECT_EQ(mock_conn_.last_command(), "1001 loc 0 12 12\t");
}

// ==================== C=11 ADDOBJECT ====================

TEST_F(GameStateTest, AddNpcToRoom)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "3001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));

    auto npcs = state_.room_npcs();
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ(npcs[0].id, 3001);
}

TEST_F(GameStateTest, AddItemToInventory)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "1001"}, {"T", "4001"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));

    auto items = state_.inventory();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].id, 4001);
    EXPECT_TRUE(items[0].is_item);
}

TEST_F(GameStateTest, AddCharacterToRoom)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "3002"}, {"u", "1"}, {"i", "0"}, {"U", "0"}}));

    auto chars = state_.room_chars();
    ASSERT_EQ(chars.size(), 1u);
    EXPECT_EQ(chars[0].id, 3002);
}

// 改名重发（服务器 CBox::set_name → on_name_changed）会再次下发 ADDOBJECT：
// 同一容器内按 object id 幂等合并——不产生重复条目，也不冲掉已通过
// DOWNOBJECT 下载的名字。
TEST_F(GameStateTest, DuplicateAddObjectMergesInsteadOfDuplicating)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "3001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));
    state_.handle_message(make_msg(
        17, {{"O", "3001"}, {"N", "一个奇怪的树"}, {"I", "tree"}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "3001"}, {"u", "0"}, {"i", "0"}, {"U", "1"}}));

    auto npcs = state_.room_npcs();
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ("一个奇怪的树", npcs[0].name);
    EXPECT_EQ("tree", npcs[0].internal_id);
    EXPECT_TRUE(npcs[0].updated);  // 状态位按最新一个包刷新
}

// 背包物品同理：重复 ADDOBJECT 只留一条，名字保留。
TEST_F(GameStateTest, DuplicateAddObjectKeepsSingleInventoryEntry)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "1001"}, {"T", "4001"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    state_.handle_message(make_msg(
        17, {{"O", "4001"}, {"N", "金创药"}, {"I", "herb"}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "1001"}, {"T", "4001"}, {"u", "0"}, {"i", "1"}, {"U", "1"}}));

    auto items = state_.inventory();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ("金创药", items[0].name);
}

// ==================== C=12 REMOVEOBJECT ====================

TEST_F(GameStateTest, RemoveObjectFromRoom)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "3001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));
    state_.handle_message(make_msg(12, {{"E", "5001"}, {"T", "3001"}}));

    EXPECT_TRUE(state_.room_npcs().empty());
}

TEST_F(GameStateTest, RemoveObjectFromInventory)
{
    state_.handle_message(make_msg(
        11,
        {{"E", "1001"}, {"T", "4001"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    state_.handle_message(make_msg(12, {{"E", "1001"}, {"T", "4001"}}));

    EXPECT_TRUE(state_.inventory().empty());
}

// ==================== C=13 RTNSKILL ====================

TEST_F(GameStateTest, ReceiveSkillTriggersDownload)
{
    state_.handle_message(make_msg(13, {{"T", "5001"},
                                        {"S", "huashan_jianfa"},
                                        {"L", "100"},
                                        {"P", "50"},
                                        {"M", "sword"},
                                        {"K", "1"}}));

    ASSERT_EQ(state_.skills().size(), 1u);
    EXPECT_EQ(state_.skills()[0].base_name, "huashan_jianfa");
    EXPECT_EQ(state_.skills()[0].level, 100);
    EXPECT_TRUE(state_.skills()[0].prepared);

    // Should send "0 downloadskill 0 huashan_jianfa\t"
    EXPECT_EQ(mock_conn_.last_command(), "0 downloadskill 0 huashan_jianfa\t");
}

// 服务端每次技能变化都全量重推技能列表（CChar::flush_my_skill 遍历
// skills 映射逐条发 RTNSKILL），记录端必须按 base_name 幂等合并。
TEST_F(GameStateTest, SkillRepushUpdatesInsteadOfDuplicating)
{
    state_.handle_message(make_msg(
        13, {{"T", "5001"}, {"S", "force"}, {"L", "1"}, {"P", "5"}}));
    state_.handle_message(make_msg(
        13, {{"T", "5001"}, {"S", "force"}, {"L", "2"}, {"P", "9"}}));
    state_.handle_message(make_msg(
        13, {{"T", "5001"}, {"S", "sword"}, {"L", "1"}, {"P", "5"}}));

    ASSERT_EQ(state_.skills().size(), 2u);
    EXPECT_EQ(state_.skills()[0].base_name, "force");
    EXPECT_EQ(state_.skills()[0].level, 2);
    EXPECT_EQ(state_.skills()[0].potential, 9);
    EXPECT_EQ(state_.skills()[1].base_name, "sword");
}

TEST_F(GameStateTest, SkillRepushKeepsDownloadedMetadata)
{
    state_.handle_message(
        make_msg(13, {{"T", "5001"}, {"S", "force"}, {"L", "1"}}));
    // DOWNSKILL 回填显示名后，再次全量重推不得丢失元数据
    state_.handle_message(make_msg(16, {{"S", "force"},
                                        {"N", "基本内功"},
                                        {"E", "force"},
                                        {"P", "1"}}));
    state_.handle_message(
        make_msg(13, {{"T", "5001"}, {"S", "force"}, {"L", "2"}}));

    ASSERT_EQ(state_.skills().size(), 1u);
    EXPECT_EQ(state_.skills()[0].skill_name, "基本内功");
    EXPECT_EQ(state_.skills()[0].level, 2);
}

// DOWNMAGIC 同样随全量重推反复下发，按法术名幂等合并。
TEST_F(GameStateTest, MagicRepushUpdatesInsteadOfDuplicating)
{
    state_.handle_message(
        make_msg(13, {{"T", "5001"}, {"S", "force"}, {"L", "1"}}));
    state_.handle_message(make_msg(28, {{"S", "force"},
                                        {"M", "recover"},
                                        {"N", "复元术"},
                                        {"R", "20"},
                                        {"A", "100"},
                                        {"V", "3"},
                                        {"T", "1"}}));
    state_.handle_message(make_msg(28, {{"S", "force"},
                                        {"M", "recover"},
                                        {"N", "复元术"},
                                        {"R", "20"},
                                        {"A", "100"},
                                        {"V", "3"},
                                        {"T", "1"}}));

    ASSERT_EQ(state_.skills().size(), 1u);
    ASSERT_EQ(state_.skills()[0].magics.size(), 1u);
    EXPECT_EQ(state_.skills()[0].magics[0].name, "recover");
    EXPECT_EQ(state_.skills()[0].magics[0].display_name, "复元术");
    EXPECT_EQ(state_.skills()[0].magics[0].mp, 100);
}

// ==================== C=14 RTNITEM ====================

TEST_F(GameStateTest, ReceiveItemWieldPositions)
{
    // Add an item first
    state_.handle_message(make_msg(
        11,
        {{"E", "1001"}, {"T", "6001"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    mock_conn_.clear_commands();

    // Update wield position
    state_.handle_message(make_msg(14, {{"I0", "6001"}, {"E0", "2"}}));

    ASSERT_EQ(state_.inventory().size(), 1u);
    EXPECT_EQ(state_.inventory()[0].wield, 2);
}

// 服务端物品按每 10 件一批下发（CChar::flush_my_equip），批头 B= 为本批首条
// 的全局编号、条目键沿用全局编号——第二批（B=10 起）必须按偏移解析，
// 否则第 10 件及之后物品的装备槽位状态会整体丢失。
TEST_F(GameStateTest, ReceiveItemWieldPositionsHonorBatchOffset)
{
    // 第一批（无 B=，默认从 0 起）先落一件
    state_.handle_message(make_msg(14, {{"I0", "7000"}}));
    ASSERT_EQ(state_.inventory().size(), 1u);

    // 第二批：B=10，条目键从 I10/E10 开始
    state_.handle_message(
        make_msg(14, {{"B", "10"}, {"I10", "7010"}, {"E10", "2"}}));

    ASSERT_EQ(state_.inventory().size(), 2u);
    const ObjectInfo *sword = nullptr;
    for (const auto &item : state_.inventory()) {
        if (item.id == 7010) sword = &item;
    }
    ASSERT_NE(sword, nullptr) << "第二批条目不得被丢弃";
    EXPECT_EQ(sword->wield, 2);
}

// ==================== C=15 RTNINFO ====================

TEST_F(GameStateTest, ReceivePlayerInfo)
{
    state_.handle_message(make_msg(15, {{"T", "1001"},
                                        {"p", "张三"},
                                        {"P", "1"},
                                        {"q", "男性"},
                                        {"r", "剑客"},
                                        {"s", "人族"},
                                        {"t", "大侠"},
                                        {"e", "500"},
                                        {"g", "1000"},
                                        {"f", "200"},
                                        {"h", "500"},
                                        {"exp", "10000"},
                                        {"E", "20"},
                                        {"F", "18"},
                                        {"G", "15"},
                                        {"H", "22"}}));

    EXPECT_EQ(state_.player_attr().name, "张三");
    EXPECT_EQ(state_.player_attr().gender, "男性");
    EXPECT_EQ(state_.player_attr().job, "剑客");
    EXPECT_EQ(state_.player_attr().hp, 500);
    EXPECT_EQ(state_.player_attr().max_hp, 1000);
    EXPECT_EQ(state_.player_attr().exp, 10000);
    EXPECT_EQ(state_.player_attr().str, 20);
}

// 开着 --dbg 时 RTNINFO 照常解析：DBG 只是旁路打印，不得改变状态更新结果。
// 顺带覆盖 case 15 的 DBG 分支（默认关闭时该分支不执行）。
TEST_F(GameStateTest, ReceivePlayerInfoWithDbgLogging)
{
    g_dbg_enabled.store(true);
    state_.handle_message(make_msg(15, {{"T", "1001"},
                                        {"p", "张三"},
                                        {"e", "500"},
                                        {"g", "1000"}}));

    EXPECT_EQ(state_.player_attr().name, "张三");
    EXPECT_EQ(state_.player_attr().hp, 500);
    EXPECT_EQ(state_.player_attr().max_hp, 1000);
    g_dbg_enabled.store(false);
}

TEST_F(GameStateTest, ReceiveInfoForOtherPlayerIgnored)
{
    state_.handle_message(
        make_msg(15, {{"T", "9999"}, {"p", "NPC"}, {"e", "100"}}));

    EXPECT_NE(state_.player_attr().name, "NPC");
}

// ==================== C=18 LOGINUSER ====================

TEST_F(GameStateTest, SuccessfulLogin)
{
    MockConnection conn;
    GameState gs(conn);
    auto out = gs.handle_message(make_msg(18, {{"U", "2001"},
                                               {"G", "1"},
                                               {"V", "2.0"},
                                               {"R", "99999"},
                                               {"M", "MyServer"},
                                               {"l", "5"},
                                               {"Y", "1"}}));

    EXPECT_TRUE(gs.is_logged_in());
    EXPECT_EQ(gs.player_id(), 2001);
    EXPECT_FALSE(out.empty());
}

TEST_F(GameStateTest, LoginFailure)
{
    MockConnection conn;
    GameState gs(conn);
    auto out = gs.handle_message(make_msg(18, {{"F", "1"}, {"M", "密码错误"}}));

    EXPECT_FALSE(gs.is_logged_in());
    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out[0].find("密码错误") != std::string::npos);
}

// ==================== C=19 / 29 / 31 Notifications ====================

TEST_F(GameStateTest, NotifyMessage)
{
    auto out = state_.handle_message(make_msg(19, {{"M", "系统消息"}}));
    ASSERT_FALSE(out.empty());
}

TEST_F(GameStateTest, NotifyFail)
{
    auto out = state_.handle_message(make_msg(29, {{"M", "命令失败"}}));
    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out[0].find("命令失败") != std::string::npos);
}

// ==================== C=23 STATE ====================

TEST_F(GameStateTest, UpdatePlayerState)
{
    state_.handle_message(
        make_msg(15, {{"T", "1001"}, {"e", "800"}, {"g", "1000"}}));
    mock_conn_.clear_commands();

    state_.handle_message(make_msg(23, {{"T", "1001"},
                                        {"hp", "600"},
                                        {"mp", "150"},
                                        {"sta", "60"},
                                        {"idl", "0"}}));

    EXPECT_EQ(state_.player_attr().hp, 600);
    EXPECT_EQ(state_.player_attr().mp, 150);
    EXPECT_FALSE(state_.player_attr().idle);
}

// ==================== C=24 ADDDOOR ====================

TEST_F(GameStateTest, AddExitToRoom)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        24, {{"R", "5001"}, {"D", "north"}, {"A", "北方"}, {"N", "北大厅"}}));

    ASSERT_EQ(state_.exits().size(), 1u);
    EXPECT_EQ(state_.exits()[0].door_key, "north");
    EXPECT_EQ(state_.exits()[0].dir_label, "北方");
}

TEST_F(GameStateTest, IgnoreDoorForDifferentRoom)
{
    state_.handle_message(make_msg(
        24, {{"R", "9999"}, {"D", "south"}, {"A", "南方"}, {"N", "南院"}}));

    EXPECT_TRUE(state_.exits().empty());
}

// ==================== C=25 / 33 / 34 Online Users ====================

TEST_F(GameStateTest, UserListAndDetails)
{
    state_.handle_message(
        make_msg(33, {{"n", "2"}, {"i0", "7001"}, {"i1", "7002"}}));
    EXPECT_EQ(state_.online_users().size(), 2u);

    state_.handle_message(make_msg(34, {{"U", "7001"},
                                        {"N", "李四"},
                                        {"I", "acc_lisi"},
                                        {"r", "10"},
                                        {"t", "高手"}}));

    ASSERT_EQ(state_.online_users().size(), 2u);
    EXPECT_EQ(state_.online_users()[0].name, "李四");
    EXPECT_EQ(state_.online_users()[0].internal_id, "acc_lisi");
    EXPECT_EQ(state_.online_users()[0].level, 10);
}

// ==================== C=26 VENDOR_LIST ====================

TEST_F(GameStateTest, VendorListOutput)
{
    auto out = state_.handle_message(make_msg(26, {{"P", "0"},
                                                   {"B", "0"},
                                                   {"g0", "1"},
                                                   {"n0", "铁剑"},
                                                   {"b0", "100"},
                                                   {"a0", "5"}}));

    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out[0].find("Shop") != std::string::npos);
}

// ==================== C=17 DOWNOBJECT ====================

TEST_F(GameStateTest, ObjectDetailUpdate)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "8001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));

    state_.handle_message(make_msg(17, {{"O", "8001"},
                                        {"N", "练功木人"},
                                        {"I", "muren"},
                                        {"P", "3"},
                                        {"a", "1"},
                                        {"U", "0"},
                                        {"t", "靶子"}}));

    auto npcs = state_.room_npcs();
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ(npcs[0].name, "练功木人");
    EXPECT_EQ(npcs[0].internal_id, "muren");
    EXPECT_EQ(npcs[0].title, "靶子");
}

// ==================== Accessor tests ====================

TEST_F(GameStateTest, StatusLineFormat)
{
    state_.handle_message(make_msg(10, {{"R", "5001"},
                                        {"I", "map_test"},
                                        {"N", "测试房间"},
                                        {"F", "0"},
                                        {"D", ""}}));
    state_.handle_message(make_msg(15, {{"T", "1001"},
                                        {"e", "500"},
                                        {"g", "1000"},
                                        {"f", "200"},
                                        {"h", "500"},
                                        {"exp", "5000"}}));

    std::string st = state_.status_line();
    EXPECT_TRUE(st.find("map_test") != std::string::npos);
    EXPECT_TRUE(st.find("测试房间") != std::string::npos);
    EXPECT_TRUE(st.find("500") != std::string::npos);
}

TEST_F(GameStateTest, RoomFilterMethods)
{
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", ""}, {"N", "Room"}, {"F", "0"}, {"D", ""}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "9001"}, {"u", "0"}, {"i", "0"}, {"U", "0"}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "9002"}, {"u", "1"}, {"i", "0"}, {"U", "0"}}));
    state_.handle_message(make_msg(
        11,
        {{"E", "5001"}, {"T", "9003"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));

    EXPECT_EQ(state_.room_npcs().size(), 1u);   // 9001
    EXPECT_EQ(state_.room_chars().size(), 1u);  // 9002
    EXPECT_EQ(state_.room_items().size(), 1u);  // 9003
}

TEST_F(GameStateTest, AccountId)
{
    state_.set_account_id("my_account");
    EXPECT_EQ(state_.account_id(), "my_account");
}

TEST_F(GameStateTest, ChannelManagement)
{
    EXPECT_EQ(state_.current_channel(), "chat");
    state_.set_current_channel("new");
    EXPECT_EQ(state_.current_channel(), "new");
    EXPECT_EQ(state_.channels().size(), 8u);
}