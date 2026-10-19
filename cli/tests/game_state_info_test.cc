// cli/tests/game_state_info_test.cc — .info 面板渲染契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter="GameStateInfo*|InfoDetail*"

#include "game_state_info.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "game_state.h"
#include "mock_connection.h"

namespace {

// 把行列表拼成一个字符串便于 contains 断言
std::string join(const std::vector<std::string>& lines)
{
    std::string out;
    for (const auto& l : lines) out += l + "\n";
    return out;
}

}  // namespace

class GameStateInfoTest : public ::testing::Test {
protected:
    MockConnection conn_;
    GameState state_;

    GameStateInfoTest() : state_(conn_) {}

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

    // 增加背包物品（含元数据）
    void add_inventory_item(ObjectID id, const std::string& name,
                            const std::string& iid = "", int qty = 1,
                            int weight = 0, int wield = 0)
    {
        state_.handle_message(make_msg(
            11, {{"E", "1001"},
                 {"T", std::to_string(id)},
                 {"u", "0"},
                 {"i", "1"},
                 {"U", "0"}}));
        GameMessage m;
        m.cmd = 17;
        m.params = {{"O", std::to_string(id)},
                    {"N", name},
                    {"I", iid},
                    {"a", std::to_string(qty)},
                    {"W", std::to_string(weight)}};
        state_.handle_message(m);
        if (wield != 0) {
            // 14: RTNITEM 设置装备槽位
            state_.handle_message(make_msg(
                14, {{"I0", std::to_string(id)},
                     {"E0", std::to_string(wield)}}));
        }
    }
};

// ==================== detail 帮助函数 ====================

TEST(InfoDetail, MakeBarShapes)
{
    EXPECT_EQ(detail::make_bar(0), "[        ]");
    EXPECT_EQ(detail::make_bar(1), "[=       ]");    // 非零保底 1 格
    EXPECT_EQ(detail::make_bar(50), "[====    ]");   // 四舍五入
    EXPECT_EQ(detail::make_bar(100), "[========]");
    EXPECT_EQ(detail::make_bar(300), "[========]");  // 上限 8 格
}

TEST(InfoDetail, MakeLinePadsShortKey)
{
    // key 会被颜色码包裹，只校验冒号之后的对齐填充
    std::string short_line = detail::make_line("AB", "v");
    EXPECT_NE(short_line.find(":"), std::string::npos);
    EXPECT_EQ(short_line.substr(short_line.find(':')), ":      v");
    std::string long_line = detail::make_line("ABCDEFGH", "v");
    EXPECT_EQ(long_line.substr(long_line.find(':')), ":v");
}

// ==================== 全量面板 ====================

TEST_F(GameStateInfoTest, AllPanelsContainSectionHeaders)
{
    std::string all = join(player_info_panels(state_));
    EXPECT_NE(all.find("人物信息"), std::string::npos);
    EXPECT_NE(all.find("位置与状态"), std::string::npos);
    EXPECT_NE(all.find("四维属性"), std::string::npos);
    EXPECT_NE(all.find("气血真气"), std::string::npos);
    EXPECT_NE(all.find("攻守战力"), std::string::npos);
    EXPECT_NE(all.find("武功"), std::string::npos);
    EXPECT_NE(all.find("携带物品"), std::string::npos);
}

TEST_F(GameStateInfoTest, UnknownPanelNameReturnsEmpty)
{
    EXPECT_TRUE(player_info_panel(state_, "bogus").empty());
}

// ==================== 人物信息面板 ====================

TEST_F(GameStateInfoTest, CharacterPanelShowsOptionalFields)
{
    login();
    auto& a = state_.mutable_player_attr();
    a.name   = "测试小侠";
    a.gender = "男";
    a.race   = "人类";
    a.job    = "武当派";
    a.title  = "少侠";
    a.level  = 3;
    a.age    = 18;
    a.exp    = 100;
    a.next_exp = 200;
    a.idle   = true;
    a.pks    = 1;
    a.mks    = 2;

    std::string out = join(player_info_panel(state_, "character"));
    EXPECT_NE(out.find("测试小侠"), std::string::npos);
    EXPECT_NE(out.find("性  别"), std::string::npos);
    EXPECT_NE(out.find("种  族"), std::string::npos);
    EXPECT_NE(out.find("职  业"), std::string::npos);
    EXPECT_NE(out.find("头  衔"), std::string::npos);
    EXPECT_NE(out.find("100/200"), std::string::npos);
    EXPECT_NE(out.find("发呆中"), std::string::npos);
    EXPECT_NE(out.find("2个人（其中1个玩家）"), std::string::npos);
}

TEST_F(GameStateInfoTest, CharacterPanelExpWithoutNextShowsBare)
{
    auto& a   = state_.mutable_player_attr();
    a.exp     = 100;
    a.next_exp = 50;  // next <= exp 时不显示分母
    std::string out = join(player_info_panel(state_, "character"));
    EXPECT_NE(out.find("经  验"), std::string::npos);
    EXPECT_EQ(out.find("100/50"), std::string::npos);
}

TEST_F(GameStateInfoTest, CharacterPanelSkipsEmptyOptionalFields)
{
    std::string out = join(player_info_panel(state_, "character"));
    EXPECT_EQ(out.find("性  别"), std::string::npos);
    EXPECT_EQ(out.find("头  衔"), std::string::npos);
    EXPECT_EQ(out.find("发呆中"), std::string::npos);
}

// ==================== 位置与状态面板 ====================

TEST_F(GameStateInfoTest, LocationPanelShowsMapAndIdle)
{
    login();
    state_.handle_message(make_msg(
        10, {{"R", "5001"}, {"I", "map_x"}, {"N", "武当"}, {"F", "0"}}));

    std::string out = join(player_info_panel(state_, "location"));
    EXPECT_NE(out.find("地 图"), std::string::npos);
    EXPECT_NE(out.find("map_x"), std::string::npos);
    EXPECT_NE(out.find("武当"), std::string::npos);
    EXPECT_NE(out.find("空闲"), std::string::npos);
}

TEST_F(GameStateInfoTest, LocationPanelShowsBattleTag)
{
    login();
    state_.handle_message(make_msg(
        10, {{"R", "5002"}, {"I", ""}, {"N", "战场"}, {"F", "1"}}));

    std::string out = join(player_info_panel(state_, "location"));
    EXPECT_NE(out.find("战斗中"), std::string::npos);
    EXPECT_EQ(out.find("空闲"), std::string::npos);
}

TEST_F(GameStateInfoTest, LocationPanelSkipsMapWhenEmpty)
{
    std::string out = join(player_info_panel(state_, "location"));
    EXPECT_EQ(out.find("地 图"), std::string::npos);
}

// ==================== 气血真气面板 ====================

TEST_F(GameStateInfoTest, VitalsPanelShowsBarsAndBonus)
{
    auto& a  = state_.mutable_player_attr();
    a.hp     = 50;
    a.max_hp = 100;
    a.eff_hp = 10;
    a.mp     = 20;
    a.max_mp = 40;
    a.enforce = 5;

    std::string out = join(player_info_panel(state_, "vitals"));
    EXPECT_NE(out.find("50/100"), std::string::npos);
    EXPECT_NE(out.find("(10)"), std::string::npos);
    EXPECT_NE(out.find("20/40"), std::string::npos);
    EXPECT_NE(out.find("(+5)"), std::string::npos);
}

TEST_F(GameStateInfoTest, VitalsPanelZeroMaxShowsPlainValue)
{
    auto& a = state_.mutable_player_attr();
    a.hp    = 7;
    a.max_hp = 0;

    std::string out = join(player_info_panel(state_, "vitals"));
    EXPECT_NE(out.find("7"), std::string::npos);
    EXPECT_EQ(out.find("/0"), std::string::npos);
}

// ==================== 攻守战力面板 ====================

TEST_F(GameStateInfoTest, CombatPanelShowsAttackDefenseAndExp)
{
    auto& a       = state_.mutable_player_attr();
    a.attack      = 30;
    a.defense     = 12;
    a.combat_exp  = 123;

    std::string out = join(player_info_panel(state_, "combat"));
    EXPECT_NE(out.find("攻击"), std::string::npos);
    EXPECT_NE(out.find("防御"), std::string::npos);
    EXPECT_NE(out.find("战斗力:123"), std::string::npos);
}

// ==================== 武功面板 ====================

TEST_F(GameStateInfoTest, SkillsPanelEmptyShowsNotLearned)
{
    std::string out = join(player_info_panel(state_, "skills"));
    EXPECT_NE(out.find("未习得"), std::string::npos);
}

TEST_F(GameStateInfoTest, SkillsPanelMarksMappedAndPrepared)
{
    login();
    state_.handle_message(make_msg(13, {{"T", "1"},
                                        {"S", "taiji_quan"},
                                        {"L", "3"},
                                        {"P", "1"},
                                        {"K", "1"}}));
    state_.handle_message(make_msg(13, {{"T", "2"},
                                        {"S", "huashan_jianfa"},
                                        {"L", "5"},
                                        {"P", "2"},
                                        {"M", "taiji_quan"}}));
    state_.handle_message(make_msg(16, {{"S", "taiji_quan"},
                                        {"N", "太极拳"},
                                        {"E", "拳法"}}));
    state_.handle_message(make_msg(16, {{"S", "huashan_jianfa"},
                                        {"N", "华山剑法"},
                                        {"E", "剑法"}}));
    conn_.clear_commands();

    std::string out = join(player_info_panel(state_, "skills"));
    // taiji_quan 被 huashan_jianfa 映射 → 打 [*]；已准备 → [Y]
    EXPECT_NE(out.find("[*]太极拳"), std::string::npos);
    EXPECT_NE(out.find("[ ]华山剑法"), std::string::npos);
    EXPECT_NE(out.find("[Y]"), std::string::npos);
    EXPECT_NE(out.find("[3/1]"), std::string::npos);
}

// ==================== 携带物品面板 ====================

TEST_F(GameStateInfoTest, InventoryPanelEmptyShowsEmpty)
{
    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_NE(out.find("(空)"), std::string::npos);
}

TEST_F(GameStateInfoTest, InventoryPanelShowsMetaAndSlot)
{
    login();
    add_inventory_item(500, "铁剑", "sword_01", /*qty=*/3, /*weight=*/12);
    add_inventory_item(600, "布衣", "cloth_01", /*qty=*/1, /*weight=*/5,
                       /*wield=*/12);
    auto& a        = state_.mutable_player_attr();
    a.max_carry    = 1000;

    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_NE(out.find("铁剑(sword_01) x3"), std::string::npos);
    EXPECT_NE(out.find("布衣(cloth_01)[铠甲]"), std::string::npos);
    EXPECT_NE(out.find("负重"), std::string::npos);
    EXPECT_NE(out.find("未占满"), std::string::npos);
}

TEST_F(GameStateInfoTest, InventoryPanelUnknownSlotOmitsName)
{
    login();
    add_inventory_item(500, "神秘装备", "myst_01", /*qty=*/1, /*weight=*/0,
                       /*wield=*/99);
    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_NE(out.find("神秘装备(myst_01)"), std::string::npos);
    EXPECT_EQ(out.find("[未知]"), std::string::npos);
}

TEST_F(GameStateInfoTest, InventoryPanelSkipsItemsWithoutMeta)
{
    login();
    // 无名、无内部 id、未装备的条目应被过滤
    state_.handle_message(make_msg(
        11, {{"E", "1001"}, {"T", "700"}, {"u", "0"}, {"i", "1"}, {"U", "0"}}));
    add_inventory_item(500, "铁剑", "sword_01");

    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_EQ(out.find("(700)"), std::string::npos);
    EXPECT_NE(out.find("铁剑"), std::string::npos);
}

TEST_F(GameStateInfoTest, InventoryPanelCrowdedWhenOverEightyPercent)
{
    login();
    add_inventory_item(500, "铁锭", "ingot_01", /*qty=*/1, /*weight=*/90);
    auto& a     = state_.mutable_player_attr();
    a.max_carry = 100;

    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_NE(out.find("已拥挤"), std::string::npos);
    EXPECT_EQ(out.find("未占满"), std::string::npos);
}

TEST_F(GameStateInfoTest, InventoryPanelZeroMaxCarryShowsPlainWeight)
{
    login();
    add_inventory_item(500, "铁锭", "ingot_01", /*qty=*/1, /*weight=*/90);
    // max_carry = 0：负重行退化为纯数字；pct 按 0 算 → 仍显示未占满
    std::string out = join(player_info_panel(state_, "inventory"));
    EXPECT_EQ(out.find("90/0"), std::string::npos);
    EXPECT_NE(out.find("未占满"), std::string::npos);
}
