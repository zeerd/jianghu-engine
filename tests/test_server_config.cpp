// test_server_config.cpp — CServerConfig 玩法开关配置单元测试
//
// 契约：config.ini [server] 段的 5 个开关决定运行期玩法分支；
// 缺段/缺键时回落内置默认值（内功加血等四项关闭、转世冲突检查开启）。
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
        ini_load("tests/test_config.ini");
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
    EXPECT_TRUE(CServerConfig::newlife_no_conflict());
}

TEST_F(ServerConfigTest, Init_ReadsConfiguredValues)
{
    SetServerOption("force_add_hp", "1");
    SetServerOption("no_force_conflict", "1");
    SetServerOption("force_no_need_prepare", "1");
    SetServerOption("no_betrayer_valid", "1");
    SetServerOption("newlife_no_conflict", "0");
    CServerConfig::init();

    EXPECT_TRUE(CServerConfig::force_add_hp());
    EXPECT_TRUE(CServerConfig::no_force_conflict());
    EXPECT_TRUE(CServerConfig::force_no_need_prepare());
    EXPECT_TRUE(CServerConfig::no_betrayer_valid());
    EXPECT_FALSE(CServerConfig::newlife_no_conflict());
}

TEST_F(ServerConfigTest, Init_AcceptsBooleanAliases)
{
    SetServerOption("no_force_conflict", "yes");
    SetServerOption("newlife_no_conflict", "off");
    CServerConfig::init();

    EXPECT_TRUE(CServerConfig::no_force_conflict());
    EXPECT_FALSE(CServerConfig::newlife_no_conflict());
}
