// test_command_line.cpp — parse_config_path 的命令行解析单元测试
//
// 契约：服务器用 --config <path> / --config=<path>
// 指定配置文件，未提供/无法解析时 回落到默认
// data/config.ini。注入式启动器（OpenCppCoverage）会把每个参数重新加引号，
// 传入形如 "--config" "C:\path\config.ini" 的命令行，也必须解析正确。
//
// 构建/运行：仓库根目录执行 .\run-coverage.ps1
#include "pch.h"

#include <cstdio>
#include <string>

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
    char buf[16] = "untouched";
    EXPECT_STREQ(kDefault, parse_config_path("--config abc", buf, 0));
    EXPECT_STREQ("untouched", buf);  // 未取到值，缓冲不被改动
}

// ---- parse_option_value：任意 `--<name> <value>` / `--<name>=<value>` 选项
// ----

namespace {

std::string Option(const char* cmd_line, const char* name,
                   const char* initial = "untouched")
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", initial);
    return parse_option_value(cmd_line, name, buf, sizeof(buf))
               ? std::string(buf)
               : std::string("<miss>") + "|" + buf;
}

}  // namespace

TEST(CommandLineOptionTest, BothFormsAndPrefixIsolation)
{
    EXPECT_EQ(Option("--log-run-id abc", "log-run-id"), "abc");
    EXPECT_EQ(Option("--log-run-id=abc", "log-run-id"), "abc");
    EXPECT_EQ(Option("--dbg --log-run-id abc --report out.txt", "log-run-id"),
              "abc");
    // 同前缀参数不是本选项，且其后仍能找到真选项
    EXPECT_EQ(Option("--log-run-identity x --log-run-id abc", "log-run-id"),
              "abc");
    // 选项名自身被引号包裹（注入式启动器）
    EXPECT_EQ(Option("\"--log-run-id\" \"abc\"", "log-run-id"), "abc");
    EXPECT_EQ(Option("--log-run-id \"a b\"", "log-run-id"), "a b");
}

TEST(CommandLineOptionTest, MissLeavesBufferUntouched)
{
    EXPECT_EQ(Option(nullptr, "log-run-id"), "<miss>|untouched");
    EXPECT_EQ(Option("", "log-run-id"), "<miss>|untouched");
    EXPECT_EQ(Option("--config data/config.ini", "log-run-id"),
              "<miss>|untouched");
    // 无值 / 空值 / 缓冲不足都不算取到
    EXPECT_EQ(Option("--log-run-id", "log-run-id"), "<miss>|untouched");
    EXPECT_EQ(Option("--log-run-id ", "log-run-id"), "<miss>|untouched");
    EXPECT_EQ(Option("--log-run-id=\"\"", "log-run-id"), "<miss>|untouched");

    char small[4];
    snprintf(small, sizeof(small), "kee");  // 4 字节缓冲只容得下 3 字符 + NUL
    EXPECT_FALSE(parse_option_value("--log-run-id abcdef", "log-run-id", small,
                                    sizeof(small)));
    EXPECT_STREQ("kee", small);
    EXPECT_FALSE(
        parse_option_value("--log-run-id abc", "", small, sizeof(small)));
}
