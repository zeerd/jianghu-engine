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
//   - get_beast_combat_action：野兽攻击动作表属引擎默认数据（lua-default
//     自带）。verb 命中返回文案与伤害加成；表外动词判未命中并清空输出
//     （damage 归 0），damage = 0 是合法取值而非未命中。
//   - load_script 的 chunkname 是文件路径（既非复合 key 也非裸名）：
//     回溯据此定位文件，覆盖率据此把执行归因到具体语料文件。
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

// 契约：野兽攻击动作表属引擎默认数据（lua-default），数据集无需自带；
// verb 命中时返回文案与伤害加成。
TEST_F(LuaDataScriptTest, BeastCombatActionLoadsFromDefaultRoot)
{
    EXPECT_TRUE(
        std::filesystem::exists("lua-default/data/beast_combat_action.lua"))
        << "野兽动作表属引擎默认数据";

    std::string action;
    int damage = -1;
    ASSERT_TRUE(g_LuaEngine->get_beast_combat_action("bite", action, damage))
        << "默认根的野兽动作表必须可读";
    EXPECT_EQ("$N扑上来张嘴往$n的$l狠狠地一咬", action);
    EXPECT_EQ(20, damage);
}

// damage = 0 是合法取值（claw 不加成），不得与「未命中」混为一谈
TEST_F(LuaDataScriptTest, BeastCombatActionZeroDamageIsStillAHit)
{
    std::string action;
    int damage = -1;
    ASSERT_TRUE(g_LuaEngine->get_beast_combat_action("claw", action, damage));
    EXPECT_EQ("$N用爪子往$n的$l一抓", action);
    EXPECT_EQ(0, damage);
}

TEST_F(LuaDataScriptTest, BeastCombatActionUnknownVerbMisses)
{
    std::string action = "stale";
    int damage         = 7;
    EXPECT_FALSE(g_LuaEngine->get_beast_combat_action("bogus", action, damage))
        << "表外动词必须判为未命中";
    EXPECT_TRUE(action.empty()) << "未命中必须清空输出";
    EXPECT_EQ(0, damage);

    // 空动词直接拒绝，不进动作表查询
    action = "stale";
    damage = 7;
    EXPECT_FALSE(g_LuaEngine->get_beast_combat_action("", action, damage));
    EXPECT_TRUE(action.empty());
    EXPECT_EQ(0, damage);
}

// 契约：load_script 装出来的 chunk，其 source 就是脚本文件的相对路径。
// 复合 key（data/login）与裸名（login）都定位不到文件，回溯与覆盖率归因
// 会一起失效——两者消费的是同一个字符串。
TEST_F(LuaDataScriptTest, LoadScriptChunkNameIsFileRelativePath)
{
    lua_State* L = ensure_lua_env();
    ASSERT_NE(L, nullptr);

    const int ref = g_LuaEngine->load_script("login", LUA_SCRIPT_DATA);
    ASSERT_NE(ref, LUA_NOREF) << "夹具 data/login.lua 必须可加载";

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_setglobal(L, "__chunkname_probe");
    const std::string source = lua_str_expr(
        L, "debug.getinfo(__chunkname_probe.check_name, 'S').source");
    lua_pushnil(L);
    lua_setglobal(L, "__chunkname_probe");

    EXPECT_EQ(source, "tests/unit/lua-tests/data/login.lua");
}
