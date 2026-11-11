// cli/tests/terminal_ui_test.cc — TerminalUI 输出队列契约
//
// 输出走单写线程：stdout 与会话日志由同一线程写、保序；调用方不被控制台
// 或日志盘的速度拖住（协议与剧本等待因此不会被渲染阻塞）。
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=TerminalUi*

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>

#include "terminal_ui.h"
#include "test_tmp_dir.h"

namespace {

// 捕获 stdout：替换缓冲区，取文本前先等 UI 写线程排空。
class CoutCapture {
public:
    explicit CoutCapture(TerminalUI& ui)
        : ui_(ui), saved_(std::cout.rdbuf(out_.rdbuf()))
    {
    }
    ~CoutCapture()
    {
        ui_.flush_output();
        std::cout.rdbuf(saved_);
    }
    std::string str()
    {
        ui_.flush_output();
        return out_.str();
    }

private:
    TerminalUI& ui_;
    std::stringstream out_;
    std::streambuf* saved_;
};

// 会话日志目录里所有 .log 的拼接内容
std::string read_logs(const std::filesystem::path& dir)
{
    std::string all;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".log") continue;
        std::ifstream in(entry.path(), std::ios::binary);
        all.append(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
    }
    return all;
}

class TerminalUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        logs_dir_ = test_tmp_dir() / "jianghu_cli_terminal_ui_test";
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
        std::filesystem::create_directories(logs_dir_);
    }
    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
    }

    std::filesystem::path logs_dir_;
};

}  // namespace

TEST_F(TerminalUiTest, AddLineWritesStdoutAndSessionLog)
{
    TerminalUI ui;
    ui.set_logs_dir(logs_dir_.string());
    CoutCapture cap(ui);

    ui.add_line("hello line");
    ASSERT_TRUE(ui.flush_output());

    EXPECT_NE(cap.str().find("hello line"), std::string::npos);
    EXPECT_NE(cap.str().find("\033[0m"), std::string::npos)
        << "stdout 行尾必须带颜色复位";

    EXPECT_NE(read_logs(logs_dir_).find("] hello line"), std::string::npos)
        << "content=[" << read_logs(logs_dir_) << "]";
}

TEST_F(TerminalUiTest, PostRawGoesToStdoutOnly)
{
    TerminalUI ui;
    ui.set_logs_dir(logs_dir_.string());
    CoutCapture cap(ui);

    ui.post_raw("[scenario] raw line\n");
    ASSERT_TRUE(ui.flush_output());

    EXPECT_NE(cap.str().find("[scenario] raw line\n"), std::string::npos);
    EXPECT_EQ(read_logs(logs_dir_).find("raw line"), std::string::npos)
        << "post_raw 是原始 stdout 文本，不入会话日志";
}

TEST_F(TerminalUiTest, QueueKeepsEmitOrder)
{
    TerminalUI ui;
    ui.set_logs_dir(logs_dir_.string());
    CoutCapture cap(ui);

    ui.add_line("first");
    ui.post_raw("second\n");
    ui.add_line("third");
    ASSERT_TRUE(ui.flush_output());

    const std::string out = cap.str();
    const auto p_first    = out.find("first");
    const auto p_second   = out.find("second");
    const auto p_third    = out.find("third");
    ASSERT_NE(p_first, std::string::npos);
    ASSERT_NE(p_second, std::string::npos);
    ASSERT_NE(p_third, std::string::npos);
    EXPECT_LT(p_first, p_second);
    EXPECT_LT(p_second, p_third);

    const std::string log = read_logs(logs_dir_);
    EXPECT_LT(log.find("] first"), log.find("] third")) << "会话日志同样保序";
}
