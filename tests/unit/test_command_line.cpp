// test_command_line.cpp — parse_config_path 的命令行解析单元测试
//
// 契约：服务器用 --config <path> / --config=<path>
// 指定配置文件，未提供/无法解析时 回落到默认
// data/config.ini。注入式启动器（OpenCppCoverage）会把每个参数重新加引号，
// 传入形如 "--config" "C:\path\config.ini" 的命令行，也必须解析正确。
//
// 构建/运行：仓库根目录执行 .\run-coverage.ps1
#include "pch.h"

#include "CommandLine.h"

namespace {

const char* const kDefault = "data/config.ini";

std::string Parse(const char* cmd_line)
{
    char buf[256];
    return parse_config_path(cmd_line, buf, sizeof(buf));
}

}  // namespace

TEST(CommandLineTest, MissingArgFallsBackToDefault)
{
    EXPECT_STREQ(kDefault, Parse(nullptr).c_str());
    EXPECT_STREQ(kDefault, Parse("").c_str());
    EXPECT_STREQ(kDefault, Parse("--check-lua").c_str());
}

TEST(CommandLineTest, SpaceSeparatedForm)
{
    EXPECT_STREQ("data/other.ini", Parse("--config data/other.ini").c_str());
    EXPECT_STREQ(
        "data/other.ini",
        Parse("--dbg --config data/other.ini --report out.txt").c_str());
}

TEST(CommandLineTest, EqualsForm)
{
    EXPECT_STREQ("data/other.ini", Parse("--config=data/other.ini").c_str());
    EXPECT_STREQ("data/other.ini",
                 Parse("--dbg --config=data/other.ini").c_str());
}

// OpenCppCoverage 会把每个参数重新加引号后再传给子进程
TEST(CommandLineTest, QuotedArgumentsFromLauncher)
{
    EXPECT_STREQ("C:\\cfg\\config.ini",
                 Parse("\"--config\" \"C:\\cfg\\config.ini\" ").c_str());
    EXPECT_STREQ("C:\\cfg dir\\config.ini",
                 Parse("\"--config\" \"C:\\cfg dir\\config.ini\" ").c_str());
    EXPECT_STREQ("C:\\cfg\\config.ini",
                 Parse("\"--config=C:\\cfg\\config.ini\"").c_str());
}

TEST(CommandLineTest, QuotedValueWithoutLauncherQuotes)
{
    EXPECT_STREQ("data/other.ini",
                 Parse("--config \"data/other.ini\"").c_str());
    EXPECT_STREQ("data/my dir/a.ini",
                 Parse("--config \"data/my dir/a.ini\"").c_str());
}

TEST(CommandLineTest, SimilarPrefixIsNotConfigOption)
{
    EXPECT_STREQ(kDefault, Parse("--configure foo").c_str());
    EXPECT_STREQ(kDefault, Parse("--configurable").c_str());
    // 首个 --config 非法时应继续向后匹配
    EXPECT_STREQ("x.ini", Parse("--configure foo --config x.ini").c_str());
}

TEST(CommandLineTest, MissingValueFallsBackToDefault)
{
    EXPECT_STREQ(kDefault, Parse("--config").c_str());
    EXPECT_STREQ(kDefault, Parse("--config ").c_str());
    EXPECT_STREQ(kDefault, Parse("--config=").c_str());
    EXPECT_STREQ(kDefault, Parse("--config \"\"").c_str());
}

TEST(CommandLineTest, FirstValidOptionWins)
{
    EXPECT_STREQ("a.ini", Parse("--config a.ini --config b.ini").c_str());
}

TEST(CommandLineTest, BufferTooSmallFallsBackToDefault)
{
    char buf[4];
    EXPECT_STREQ(kDefault,
                 parse_config_path("--config abcdef", buf, sizeof(buf)));
}

TEST(CommandLineTest, NullBufferFallsBackToDefault)
{
    EXPECT_STREQ(kDefault, parse_config_path("--config abc", nullptr, 16));
}
