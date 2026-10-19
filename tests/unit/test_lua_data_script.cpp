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
//
#include "pch.h"

#include <cstdio>
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
