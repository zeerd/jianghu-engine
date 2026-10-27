// cli/tests/types_test.cc — GameMessage 访问器契约 + 装备槽位名映射
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=GameMessage*:WieldPositionName*

#include "types.h"

#include <gtest/gtest.h>

TEST(GameMessage, GetReturnsValueOrDefault)
{
    GameMessage m;
    m.params["M"] = "hello";
    EXPECT_EQ(m.get("M"), "hello");
    EXPECT_EQ(m.get("X"), "");
    EXPECT_EQ(m.get("X", "fallback"), "fallback");
}

TEST(GameMessage, GetIntParsesAndFallsBack)
{
    GameMessage m;
    m.params["C"] = "42";
    m.params["bad"] = "abc";
    EXPECT_EQ(m.get_int("C"), 42);
    EXPECT_EQ(m.get_int("bad"), 0);
    EXPECT_EQ(m.get_int("bad", -5), -5);
    EXPECT_EQ(m.get_int("missing", 7), 7);
}

TEST(GameMessage, HasChecksPresence)
{
    GameMessage m;
    m.params["k"] = "";
    EXPECT_TRUE(m.has("k"));
    EXPECT_FALSE(m.has("absent"));
}

TEST(GameMessage, ToStringContainsCmdAndParams)
{
    GameMessage m;
    m.cmd = 19;
    m.params["M"] = "hi";
    std::string s = m.to_string();
    EXPECT_NE(s.find("cmd=19"), std::string::npos);
    EXPECT_NE(s.find("M=hi"), std::string::npos);
}

// 装备槽位名是「剧本 wield 条件」与「.info 装备渲染」共用的唯一映射：
// 两边对同一个槽位读出的字面量必须一致，否则剧本会永远等不到槽位匹配。
// 逐槽覆盖，未映射的槽位（0/越界）一律回「未知」。
TEST(WieldPositionName, MapsEverySlotAndUnknownFallback)
{
    EXPECT_EQ(wield_position_name(0), "未知");
    EXPECT_EQ(wield_position_name(1), "头部");
    EXPECT_EQ(wield_position_name(2), "左耳");
    EXPECT_EQ(wield_position_name(3), "右耳");
    EXPECT_EQ(wield_position_name(4), "耳环");
    EXPECT_EQ(wield_position_name(5), "颈部");
    EXPECT_EQ(wield_position_name(6), "左手");
    EXPECT_EQ(wield_position_name(7), "右手");
    EXPECT_EQ(wield_position_name(8), "手部");
    EXPECT_EQ(wield_position_name(9), "左手腕");
    EXPECT_EQ(wield_position_name(10), "右手腕");
    EXPECT_EQ(wield_position_name(11), "手腕");
    EXPECT_EQ(wield_position_name(12), "铠甲");
    EXPECT_EQ(wield_position_name(13), "腰带");
    EXPECT_EQ(wield_position_name(14), "鞋子");
    EXPECT_EQ(wield_position_name(15), "戒指");
    EXPECT_EQ(wield_position_name(16), "未知");
    EXPECT_EQ(wield_position_name(-1), "未知");
}
