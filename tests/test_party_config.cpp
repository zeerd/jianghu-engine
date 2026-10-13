// test_party_config.cpp — PartyConfig 组队配置初始化单元测试
//
// Init() 从 config.ini 的 [party] 段读取参数并钳制到合法区间，
// 缺段/缺键时回落到内置默认值。
#include "pch.h"

#include <string>

#include "IniParser.h"
#include "PartyConfig.h"

namespace {

class PartyConfigTest : public ::testing::Test {
protected:
    void SetUp() override { ResetToDefaults(); }

    void TearDown() override { ResetToDefaults(); }

    static void ResetToDefaults()
    {
        g_ini_data["party"].clear();
        CPartyConfig::init();
    }

    static void SetPartyOption(const char* key, const char* value)
    {
        g_ini_data["party"][key] = value;
    }
};

}  // namespace

TEST_F(PartyConfigTest, Init_UsesBuiltInDefaultsWhenSectionMissing)
{
    EXPECT_EQ(CPartyConfig::max_members(), 4);
    EXPECT_TRUE(CPartyConfig::share_exp());
    EXPECT_TRUE(CPartyConfig::auto_follow_fight());
    EXPECT_EQ(CPartyConfig::couple_level_diff(), 10);
}

TEST_F(PartyConfigTest, Init_ReadsConfiguredValues)
{
    SetPartyOption("max_members", "6");
    SetPartyOption("share_exp", "no");
    SetPartyOption("auto_follow_fight", "off");
    SetPartyOption("couple_level_diff", "20");
    CPartyConfig::init();

    EXPECT_EQ(CPartyConfig::max_members(), 6);
    EXPECT_FALSE(CPartyConfig::share_exp());
    EXPECT_FALSE(CPartyConfig::auto_follow_fight());
    EXPECT_EQ(CPartyConfig::couple_level_diff(), 20);
}

TEST_F(PartyConfigTest, Init_ClampsMaxMembersToPlayableRange)
{
    SetPartyOption("max_members", "1");
    CPartyConfig::init();
    EXPECT_EQ(CPartyConfig::max_members(), 2);

    SetPartyOption("max_members", "99");
    CPartyConfig::init();
    EXPECT_EQ(CPartyConfig::max_members(), 10);
}

TEST_F(PartyConfigTest, Init_ClampsCoupleLevelDiffToNonNegative)
{
    SetPartyOption("couple_level_diff", "-5");
    CPartyConfig::init();
    EXPECT_EQ(CPartyConfig::couple_level_diff(), 0);

    SetPartyOption("couple_level_diff", "999");
    CPartyConfig::init();
    EXPECT_EQ(CPartyConfig::couple_level_diff(), 50);
}
