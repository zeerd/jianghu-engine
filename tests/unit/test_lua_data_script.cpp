// test_lua_data_script.cpp — CLuaEngine 统一数据脚本接口（DATA 类型）
//
// 契约：
//   - get_data_array：必需数据脚本。文件缺失 → 返回 false，并写出
//     "Failed to load data script" 错误行（同时走 load_script 的
//     BACKTRACE 回溯路径）。
//   - get_optional_data_array：可选数据脚本。文件缺失 = 数据集未配置
//     该表，out 清空返回 true，不写 ERROR、不写 BACKTRACE；文件存在
//     但加载失败（语法错等）仍按错误路径返回 false。
//   - get_ban_words：聊天禁词表 chanbanword 属可选配置，缺失时按空词表
//     处理（禁词过滤不生效），不作为启动错误。
//   - get_channel_config：扩展频道表 channel_config 属可选配置，缺失时
//     out 清空返回 true（扩展频道不注册），不写 ERROR、不写 BACKTRACE；
//     存在时按 频道名 -> { field -> 值 } 读出，值恒为字符串（Lua 数字
//     归一为 "1" 这类文本，布尔归一为 "1"/"0"）。文件属数据集内容：
//     夹具在 tests/unit/lua-tests/data/，引擎默认 lua-default 不带。
//
#include "pch.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "LuaEngine.h"
#include "test_log_hits.h"
#include "test_lua_env.h"

namespace {

const char* kRequiredAbsent = "data_required_absent_zzz";
const char* kOptionalAbsent = "data_optional_absent_zzz";

const char* kBanWordsFixture = "tests/unit/lua-tests/data/chanbanword.lua";
const char* kBanWordsFixtureOff
    = "tests/unit/lua-tests/data/chanbanword.lua.disabled";

const char* kChannelConfigFixture
    = "tests/unit/lua-tests/data/channel_config.lua";
const char* kChannelConfigFixtureOff
    = "tests/unit/lua-tests/data/channel_config.lua.disabled";

}  // namespace

class LuaDataScriptTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

TEST_F(LuaDataScriptTest, RequiredScriptMissingFailsLoudly)
{
    const char* needle
        = "GetDataArray: Failed to load data script "
          "'data_required_absent_zzz'";
    const int before = count_log_hits(needle);

    std::vector<std::string> out;
    EXPECT_FALSE(g_LuaEngine->get_data_array(kRequiredAbsent, "items", out));
    EXPECT_TRUE(out.empty()) << "失败路径必须清空输出";
    EXPECT_EQ(count_log_hits(needle), before + 1)
        << "必需数据脚本缺失必须报 ERROR";
}

TEST_F(LuaDataScriptTest, OptionalScriptMissingTreatedAsUnconfigured)
{
    const char* err_needle
        = "GetDataArray: Failed to load data script "
          "'data_optional_absent_zzz'";
    const char* bt_needle = "BACKTRACE  context: [data_optional_absent_zzz]";
    const int err_before  = count_log_hits(err_needle);
    const int bt_before   = count_log_hits(bt_needle);

    std::vector<std::string> out;
    out.push_back("stale");
    EXPECT_TRUE(
        g_LuaEngine->get_optional_data_array(kOptionalAbsent, "items", out))
        << "可选脚本缺失属于未配置，不是失败";
    EXPECT_TRUE(out.empty()) << "未配置的可选脚本必须清空输出";
    EXPECT_EQ(count_log_hits(err_needle), err_before)
        << "可选脚本缺失不应写 ERROR";
    EXPECT_EQ(count_log_hits(bt_needle), bt_before)
        << "可选脚本缺失不应写 BACKTRACE";
}

TEST_F(LuaDataScriptTest, OptionalScriptMissingRejectsEmptyName)
{
    std::vector<std::string> out;
    EXPECT_FALSE(g_LuaEngine->get_optional_data_array("", "items", out));
    EXPECT_FALSE(g_LuaEngine->get_optional_data_array("chanbanword", "", out));
}

TEST_F(LuaDataScriptTest, OptionalScriptPresentLoadsFixtureData)
{
    std::vector<std::string> out;
    ASSERT_TRUE(g_LuaEngine->get_optional_data_array("chanbanword",
                                                     "banned_words", out))
        << "夹具 data/chanbanword.lua 必须可读";
    EXPECT_GE(out.size(), 3u) << "禁词表条目: " << out.size();
}

TEST_F(LuaDataScriptTest, BanWordsLoadFromFixtureScript)
{
    std::vector<std::string> words;
    ASSERT_TRUE(g_LuaEngine->get_ban_words(words)) << "禁词表可读";
    EXPECT_GE(words.size(), 3u);
}

TEST_F(LuaDataScriptTest, BanWordsWithoutConfigScriptYieldsEmptyTable)
{
    const char* err_needle
        = "GetDataArray: Failed to load data script 'chanbanword'";
    const char* bt_needle = "BACKTRACE  context: [chanbanword]";

    ASSERT_EQ(rename(kBanWordsFixture, kBanWordsFixtureOff), 0)
        << "夹具改名失败（文件不存在？）";

    const int err_before = count_log_hits(err_needle);
    const int bt_before  = count_log_hits(bt_needle);

    std::vector<std::string> words;
    const bool ok    = g_LuaEngine->get_ban_words(words);
    const bool empty = words.empty();

    // 夹具必须复原：断言放在复原之后，避免失败路径污染仓库
    EXPECT_EQ(rename(kBanWordsFixtureOff, kBanWordsFixture), 0)
        << "夹具必须复原";

    EXPECT_TRUE(ok) << "禁词表缺失 = 未配置，不是加载失败";
    EXPECT_TRUE(empty) << "未配置禁词表 → 空词表";
    EXPECT_EQ(count_log_hits(err_needle), err_before)
        << "禁词表缺失不应写 ERROR";
    EXPECT_EQ(count_log_hits(bt_needle), bt_before)
        << "禁词表缺失不应写 BACKTRACE";
}

TEST_F(LuaDataScriptTest, ChannelConfigPresentLoadsExtChannels)
{
    std::map<std::string, std::map<std::string, std::string>> cfg;
    ASSERT_TRUE(g_LuaEngine->get_channel_config(cfg))
        << "数据集夹具 channel_config 必须可读";

    // f1-f4 + answer 5 个扩展频道，键齐全
    EXPECT_EQ(cfg.size(), 5u);
    for (const char* name : {"f1", "f2", "f3", "f4", "answer"})
        EXPECT_EQ(cfg.count(name), 1u) << "缺少扩展频道: " << name;

    // 字段值原样搬运；数值字段在此层归一为字符串（整数化在 ChatChannel）
    EXPECT_EQ(cfg["answer"]["msg_speak"], "$YEL【考试】%s: %s");
    EXPECT_EQ(cfg["f1"]["newbie"], "1");
    EXPECT_EQ(cfg["f1"]["channel"], "f1");
}

TEST_F(LuaDataScriptTest, ChannelConfigMissingYieldsEmptyNoError)
{
    const char* err_needle
        = "GetChannelConfig: Failed to load data script 'channel_config'";
    const char* bt_needle = "BACKTRACE  context: [channel_config]";

    ASSERT_EQ(rename(kChannelConfigFixture, kChannelConfigFixtureOff), 0)
        << "夹具改名失败（文件不存在？）";

    const int err_before = count_log_hits(err_needle);
    const int bt_before  = count_log_hits(bt_needle);

    std::map<std::string, std::map<std::string, std::string>> cfg;
    cfg["stale"]["msg_speak"] = "脏数据";
    const bool ok             = g_LuaEngine->get_channel_config(cfg);
    const bool empty          = cfg.empty();
    const int err_after       = count_log_hits(err_needle);
    const int bt_after        = count_log_hits(bt_needle);

    // 夹具必须复原：断言放在复原之后，避免失败路径污染仓库
    EXPECT_EQ(rename(kChannelConfigFixtureOff, kChannelConfigFixture), 0)
        << "夹具必须复原";

    EXPECT_TRUE(ok) << "扩展频道缺失 = 未配置，不是加载失败";
    EXPECT_TRUE(empty) << "未配置时必须清空输出";
    EXPECT_EQ(err_after, err_before) << "channel_config 缺失不应写 ERROR";
    EXPECT_EQ(bt_after, bt_before) << "channel_config 缺失不应写 BACKTRACE";
}

// 契约：扩展频道属数据集内容——引擎默认（lua-default）不带
// channel_config.lua，裸装引擎只有 10 个机制频道；扩展频道由各数据集
// 自带本文件提供（缺失分支见 ChannelConfigMissingYieldsEmptyNoError）
TEST_F(LuaDataScriptTest, DefaultScriptRootHasNoChannelConfig)
{
    EXPECT_FALSE(std::filesystem::exists("lua-default/data/channel_config.lua"))
        << "扩展频道属数据集，引擎默认不得自带 channel_config.lua";
}
