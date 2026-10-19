// test_server_config.cpp — CServerConfig 玩法开关配置单元测试
//
// 契约：config.ini [server] 段的开关决定运行期玩法分支；
// 缺段/缺键时回落内置默认值（内功加血四项关闭、限流开启）。
#include "pch.h"

#include "IniParser.h"
#include "ServerConfig.h"

namespace {

class ServerConfigTest : public ::testing::Test {
protected:
    void SetUp() override { ResetToDefaults(); }

    void TearDown() override { ResetToDefaults(); }

    static void ResetToDefaults()
    {
        ini_load("tests/unit/test_config.ini");
        CServerConfig::init();
    }

    static void SetServerOption(const char* key, const char* value)
    {
        g_ini_data["server"][key] = value;
    }
};

}  // namespace

TEST_F(ServerConfigTest, Init_UsesBuiltInDefaultsWhenSectionMissing)
{
    EXPECT_FALSE(CServerConfig::force_add_hp());
    EXPECT_FALSE(CServerConfig::no_force_conflict());
    EXPECT_FALSE(CServerConfig::force_no_need_prepare());
    EXPECT_FALSE(CServerConfig::no_betrayer_valid());
    EXPECT_TRUE(CServerConfig::rate_limit()) << "限流默认开启";
}

TEST_F(ServerConfigTest, Init_ReadsConfiguredValues)
{
    SetServerOption("force_add_hp", "1");
    SetServerOption("no_force_conflict", "1");
    SetServerOption("force_no_need_prepare", "1");
    SetServerOption("no_betrayer_valid", "1");
    CServerConfig::init();

    EXPECT_TRUE(CServerConfig::force_add_hp());
    EXPECT_TRUE(CServerConfig::no_force_conflict());
    EXPECT_TRUE(CServerConfig::force_no_need_prepare());
    EXPECT_TRUE(CServerConfig::no_betrayer_valid());
}

TEST_F(ServerConfigTest, Init_AcceptsBooleanAliases)
{
    SetServerOption("no_force_conflict", "yes");
    CServerConfig::init();

    EXPECT_TRUE(CServerConfig::no_force_conflict());
}

// E2E 集成测试配置通过 rate_limit=0 关闭指令限流
TEST_F(ServerConfigTest, Init_ReadsRateLimitToggle)
{
    SetServerOption("rate_limit", "0");
    CServerConfig::init();

    EXPECT_FALSE(CServerConfig::rate_limit());
}
