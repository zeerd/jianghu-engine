// 构建/运行（仓库根）：.\run-coverage.ps1
#include "gtest/gtest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "compat.h"
#include "config.h"

namespace {

std::filesystem::path test_dir()
{
    auto dir
        = std::filesystem::temp_directory_path() / "jianghu_cli_config_test";
    std::filesystem::create_directories(dir);
    return dir;
}

// 写入临时配置文件（body 原样落盘）并加载。
Config load_conf(const std::string &body)
{
    auto path = test_dir() / "cli.conf";
    std::ofstream out(path, std::ios::trunc);
    out << body;
    out.close();
    return load_config(path.string());
}

// 指向临时目录的 logs_dir 行，避免落盘时创建用户目录。
std::string logs_line()
{
    return "logs_dir = " + test_dir().string() + "\n";
}

}  // namespace

// 心跳配置契约：缺省保活开启（15s）；heartbeat_interval 可改周期、0 关闭。
// 集成测试的 tests/integration/cli.conf 即以 0 关闭，避免心跳回执
// 每 15s 刷屏污染剧本的 wait/notext 断言。
TEST(ConfigHeartbeat, ParsesIntervalAndKeepsDefaultEnabled)
{
    Config c = load_conf(logs_line() + "host = 127.0.0.1\n");
    EXPECT_EQ(c.heartbeat_interval, 15);
    c = load_conf(logs_line() + "heartbeat_interval = 30\n");
    EXPECT_EQ(c.heartbeat_interval, 30);
    c = load_conf(logs_line() + "heartbeat_interval = 0\n");
    EXPECT_EQ(c.heartbeat_interval, 0);
}

// 连接配置契约：port/account/password 三个键正确解析。
// 故意不写 logs_dir，顺带验证缺省回落（不影响断言值）。
TEST(ConfigParse, AccountPortAndPassword)
{
    Config c = load_conf("port = 6601\naccount = acc\npassword = pwd\n");
    EXPECT_EQ(c.port, 6601);
    EXPECT_EQ(c.account, "acc");
    EXPECT_EQ(c.password, "pwd");
}

// 配置文件不存在时返回全部缺省值，而不是空壳或异常。
TEST(ConfigParse, MissingFileReturnsDefaults)
{
    Config c = load_config((test_dir() / "no_such.conf").string());
    EXPECT_EQ(c.host, "127.0.0.1");
    EXPECT_EQ(c.port, 6600);
    EXPECT_EQ(c.heartbeat_interval, 15);
}

// logs_dir 里的 ~ 展开为主目录，否则日志会写进相对路径。
TEST(ConfigParse, ExpandsTildeInLogsDir)
{
    Config c          = load_conf("logs_dir = ~/AppData\n");
    const std::string home = home_dir();
    EXPECT_EQ(c.logs_dir, home.empty() ? "~/AppData" : home + "/AppData");
}

// 未配置 logs_dir 时，缺省值必须同样展开为主目录下的
// ~/.local/share/jianghu/logs（会话日志是 E2E 排障的权威记录，落错
// 位置等于没有日志）。副作用：确保缺省目录存在，属 CLI 本人生命周期行为。
TEST(ConfigParse, DefaultLogsDirExpandsHome)
{
    Config c             = load_conf("port = 6601\n");
    const std::string home = home_dir();
    EXPECT_EQ(c.logs_dir,
              home.empty() ? "~/.local/share/jianghu/logs"
                           : home + "/.local/share/jianghu/logs");
}

// 未设 CC_JIANGHU_CONFIG 时，配置文件固定取 ~/.jianghu/config。
TEST(ConfigPath, DefaultPathUsesHomeConfigLocation)
{
    const char *prev = getenv("CC_JIANGHU_CONFIG");
    std::string prev_val;
    if (prev) prev_val = prev;
    _putenv_s("CC_JIANGHU_CONFIG", "");

    const std::string home = home_dir();
    EXPECT_EQ(config_path(),
              home.empty() ? "./jianghu.conf" : home + "/.jianghu/config");

    if (prev) _putenv_s("CC_JIANGHU_CONFIG", prev_val.c_str());
}
