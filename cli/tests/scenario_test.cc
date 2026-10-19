// cli/tests/scenario_test.cc — 无头剧本「回显 + 立即 flush」契约
//
// 构建与运行（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=ScenarioRun*
//
// 契约：run_scenario 每执行一步都要把步骤回显写进 stdout 并立即 flush，
// 下一步的回显必须发生在上一步的 flush 之后。
// run-integration-tests.ps1 靠管道流式读取这些行显示进度；回显一旦不
// flush，stdout 走管道是全缓冲的，整段剧本（含长达数分钟的 wait）会在
// 控制台上毫无反馈，无法区分「正在跑」与「已卡死」。

#include "scenario.h"
#include "scenario_runner.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

#include "client_core.h"
#include "config.h"
#include "mock_connection.h"
#include "protocol.h"

namespace {

// 记录写入的全部字节与每次 flush 时的字节数，用位置断言先后顺序。
class SyncProbeBuf : public std::streambuf {
public:
    std::string text;
    std::vector<size_t> flush_positions;

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        text.append(s, static_cast<size_t>(n));
        return n;
    }

    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        text.push_back(traits_type::to_char_type(ch));
        return ch;
    }

    int sync() override
    {
        flush_positions.push_back(text.size());
        return 0;
    }
};

struct LineSpan {
    size_t start = 0;
    size_t end   = 0;
};

// 取包含 needle 的整行（含行尾 '\n'）在 text 中的位置。
bool find_line(const std::string& text, const std::string& needle,
               LineSpan& out)
{
    size_t pos = text.find(needle);
    if (pos == std::string::npos) return false;
    size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) return false;
    out.start = pos;
    out.end   = nl + 1;
    return true;
}

// [from, to] 区间内是否发生过 flush。回显行在 text 里首尾相接，
// 上一行的 end 就是下一行的 start，紧跟在行尾 '\n' 之后的 flush 恰好
// 落在这个端点上，因此两端都要闭合。
bool has_flush_in(const std::vector<size_t>& flushes, size_t from, size_t to)
{
    for (size_t f : flushes) {
        if (f >= from && f <= to) return true;
    }
    return false;
}

// from 之后（含恰好位于 from 的 flush）是否发生过 flush。
bool has_flush_after(const std::vector<size_t>& flushes, size_t from)
{
    for (size_t f : flushes) {
        if (f >= from) return true;
    }
    return false;
}

}  // namespace

class ScenarioRunTest : public ::testing::Test {
protected:
    std::filesystem::path work_dir_;
    std::filesystem::path script_path_;
    std::filesystem::path logs_dir_;

    void SetUp() override
    {
        work_dir_    = std::filesystem::temp_directory_path()
                       / "jianghu_cli_scenario_test";
        logs_dir_    = work_dir_ / "logs";
        script_path_ = work_dir_ / "echo.sce";
        std::filesystem::create_directories(work_dir_);

        // 行 1 注释、行 2 空行都不产生步骤，步骤落在行 3 与行 5。
        std::ofstream out(script_path_, std::ios::trunc);
        out << "# 注释与空行不产生步骤\n"
               "\n"
               "sleep 0\n"
               "\n"
               "sleep 0\n";
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(work_dir_, ec);
    }
};

namespace {

// 计数 needle 在 text 中出现的次数（needle 互不重叠）。
size_t count_occurrences(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    for (size_t p = text.find(needle); p != std::string::npos;
         p = text.find(needle, p + needle.size())) {
        ++count;
    }
    return count;
}

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

}  // namespace

struct RunResult {
    int rc = -1;
    std::string out;    // 回显到 stdout 的全部文本
    std::string report; // report 文件内容（未给 report_path 时为空）
};

// repeat/end 区块契约：区块在装载期展开为重复的步骤序列，行号保持为
// 区块体内步骤的原始行号（失败定位不能因为展开而失真）。
class ScenarioRepeatTest : public ::testing::Test {
protected:
    std::filesystem::path work_dir_;
    std::filesystem::path script_path_;
    std::filesystem::path report_path_;
    std::filesystem::path logs_dir_;

    void SetUp() override
    {
        work_dir_     = std::filesystem::temp_directory_path()
                        / "jianghu_cli_scenario_repeat_test";
        logs_dir_     = work_dir_ / "logs";
        script_path_  = work_dir_ / "loop.sce";
        report_path_  = work_dir_ / "report.txt";
        std::filesystem::create_directories(work_dir_);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(work_dir_, ec);
    }

    // 写入剧本（与可选变量文件）并以报告模式运行，返回运行结果。
    RunResult run_script(const std::string& script,
                         const std::string& vars = "")
    {
        std::ofstream out(script_path_, std::ios::trunc);
        out << script;
        out.close();

        std::filesystem::path vars_path = work_dir_ / "vars.txt";
        std::error_code ec;
        std::filesystem::remove(vars_path, ec);
        if (!vars.empty()) {
            std::ofstream v(vars_path, std::ios::trunc);
            v << vars;
            v.close();
        }

        Config cfg;
        cfg.logs_dir = logs_dir_.string();
        ClientCore core(cfg);

        ScenarioOptions opts;
        opts.script_path        = script_path_.string();
        opts.report_path        = report_path_.string();
        opts.vars_path          = vars_path.string();
        opts.default_timeout_ms = 5000;

        RunResult r;
        std::cout.flush();
        SyncProbeBuf probe;
        std::streambuf* saved = std::cout.rdbuf(&probe);
        r.rc                   = run_scenario(core, opts);
        std::cout.rdbuf(saved);

        r.out    = probe.text;
        r.report = read_text_file(report_path_);
        return r;
    }
};

TEST_F(ScenarioRepeatTest, RepeatsBodyGivenTimes)
{
    RunResult r = run_script("repeat 3\n"
                             "sleep 0\n"
                             "end\n");

    EXPECT_EQ(r.rc, 0);
    // 区块体执行 3 次，行号仍是区块体内步骤的行号 2
    EXPECT_EQ(count_occurrences(r.out, "[script:2] sleep 0"), 3u);
}

TEST_F(ScenarioRepeatTest, NestedRepeatsMultiplyIterations)
{
    RunResult r = run_script("repeat 2\n"
                             "repeat 2\n"
                             "sleep 0\n"
                             "end\n"
                             "end\n");

    EXPECT_EQ(r.rc, 0);
    EXPECT_EQ(count_occurrences(r.out, "[script:3] sleep 0"), 4u);
}

TEST_F(ScenarioRepeatTest, ZeroCountSkipsBody)
{
    RunResult r = run_script("repeat 0\n"
                             "sleep 0\n"
                             "end\n"
                             "sleep 0\n");

    EXPECT_EQ(r.rc, 0);
    EXPECT_EQ(count_occurrences(r.out, "[script:2] sleep 0"), 0u);
    // 区块之后的步骤照常执行
    EXPECT_EQ(count_occurrences(r.out, "[script:4] sleep 0"), 1u);
}

TEST_F(ScenarioRepeatTest, IndexVariableExposesIterationNumber)
{
    RunResult r = run_script("repeat 3 as i\n"
                             "set last ${i}\n"
                             "end\n");

    EXPECT_EQ(r.rc, 0);
    // 最后一轮迭代 i=3，set 把它写进变量并随报告落盘
    EXPECT_NE(r.report.find("var.last=3"), std::string::npos) << r.report;
    EXPECT_NE(r.report.find("var.i=3"), std::string::npos) << r.report;
}

TEST_F(ScenarioRepeatTest, RepeatCountSubstitutesLoadedVars)
{
    RunResult r = run_script("repeat ${n}\n"
                             "sleep 0\n"
                             "end\n",
                             "n=2\n");

    EXPECT_EQ(r.rc, 0);
    EXPECT_EQ(count_occurrences(r.out, "[script:2] sleep 0"), 2u);
}

TEST_F(ScenarioRepeatTest, RepeatWithoutEndFailsAtLoad)
{
    RunResult r = run_script("repeat 2\n"
                             "sleep 0\n");

    EXPECT_EQ(r.rc, 2);
    EXPECT_NE(r.report.find("status=fail"), std::string::npos) << r.report;
    EXPECT_NE(r.report.find("repeat without end"), std::string::npos)
        << r.report;
}

TEST_F(ScenarioRepeatTest, EndWithoutRepeatFailsAtLoad)
{
    RunResult r = run_script("sleep 0\n"
                             "end\n");

    EXPECT_EQ(r.rc, 2);
    EXPECT_NE(r.report.find("status=fail"), std::string::npos) << r.report;
    EXPECT_NE(r.report.find("end without repeat"), std::string::npos)
        << r.report;
}

TEST_F(ScenarioRepeatTest, RepeatCountMustBeInRange)
{
    const char* kBadCounts[] = {"abc", "-1", "10001"};
    for (const char* bad : kBadCounts) {
        SCOPED_TRACE(bad);
        RunResult r = run_script(std::string("repeat ") + bad + "\n"
                                                                    "sleep 0\n"
                                                                    "end\n");
        EXPECT_EQ(r.rc, 2);
        EXPECT_NE(r.report.find("status=fail"), std::string::npos) << r.report;
        EXPECT_NE(r.report.find("repeat "), std::string::npos) << r.report;
    }
}

TEST_F(ScenarioRepeatTest, FailureInsideRepeatReportsBodyLine)
{
    RunResult r = run_script("repeat 2\n"
                             "bogus verb\n"
                             "end\n");

    EXPECT_EQ(r.rc, 1);
    EXPECT_NE(r.report.find("line=2"), std::string::npos) << r.report;
    EXPECT_NE(r.report.find("unknown step verb: bogus"), std::string::npos)
        << r.report;
}

TEST_F(ScenarioRunTest, EchoesEachStepAndFlushesBeforeNextStep)
{
    Config cfg;
    cfg.logs_dir = logs_dir_.string();

    ClientCore core(cfg);

    ScenarioOptions opts;
    opts.script_path        = script_path_.string();
    opts.default_timeout_ms = 5000;

    std::cout.flush();
    SyncProbeBuf probe;
    std::streambuf* saved = std::cout.rdbuf(&probe);
    int rc                = run_scenario(core, opts);
    std::cout.rdbuf(saved);

    EXPECT_EQ(rc, 0);

    LineSpan running;
    LineSpan first;
    LineSpan second;
    ASSERT_TRUE(find_line(probe.text, "[scenario] running", running));
    ASSERT_TRUE(find_line(probe.text, "[script:3] sleep 0", first));
    ASSERT_TRUE(find_line(probe.text, "[script:5] sleep 0", second));

    // 每条回显写完后立刻 flush，且早于下一条回显
    EXPECT_TRUE(has_flush_in(probe.flush_positions, running.end, first.start))
        << "startup line was not flushed";
    EXPECT_TRUE(has_flush_in(probe.flush_positions, first.end, second.start))
        << "step echo at line 3 was not flushed immediately";
    EXPECT_TRUE(has_flush_after(probe.flush_positions, second.end))
        << "step echo at line 5 was not flushed";
}

// ==================== Wait Text Condition Tests ====================

/// 测试用子类：暴露 protected 的 eval_cond / wait_cond
class TestScenarioRunner : public CScenarioRunner {
public:
    using CScenarioRunner::CScenarioRunner;
    using CScenarioRunner::eval_cond;
    using CScenarioRunner::wait_cond;
};

class WaitTextConditionTest : public ::testing::Test {
protected:
    Config cfg_;
    std::filesystem::path logs_dir_;
    std::unique_ptr<ClientCore> core_;
    ScenarioOptions opts_;
    MockConnection* mock_conn_ = nullptr;  // 由 unique_ptr 管理，测试用裸指针访问

    WaitTextConditionTest() {
        logs_dir_ = std::filesystem::temp_directory_path() / "jianghu_cli_wait_text_test";
        std::filesystem::create_directories(logs_dir_);
        cfg_.logs_dir = logs_dir_.string();
        auto mock = std::make_unique<MockConnection>();
        mock_conn_ = mock.get();
        core_ = std::make_unique<ClientCore>(cfg_, std::move(mock));
        opts_.default_timeout_ms = 5000;
    }

    ~WaitTextConditionTest() override {
        std::error_code ec;
        std::filesystem::remove_all(logs_dir_, ec);
    }

    // 通过 MockConnection 注入原始消息，触发 process_received 产生输出行
    void inject_output(const std::string &raw_msg) {
        mock_conn_->inject_received(raw_msg);
        core_->process_received();
    }

    TestScenarioRunner make_runner() {
        return TestScenarioRunner(*core_, opts_);
    }
};

// 协议编码辅助：构造正确的 wire format 消息
// 服务端发送文本格式：&C=19&M=文本&\t  (NOTIFY)
// 经 GBK 编码 -> XOR 加密 -> 发送
// 测试注入的是原始 socket 数据（XOR 后的 GBK 字节 + \t）
static std::string make_text_msg(const std::string &text) {
    // 1. 构造 UTF-8 格式的消息体
    std::string msg_body = "&C=19&M=" + text + "&";
    // 2. UTF-8 -> GBK
    std::string gbk = protocol::utf8_to_gbk(msg_body);
    // 3. XOR 加密
    std::string encoded = protocol::encode_str(gbk);
    // 4. 添加 \t 终止符（不参与 XOR）
    encoded.push_back('\t');
    return encoded;
}

// 使用 wait_cond 测试（短超时，条件已满足则立即返回 true；不满足则超时返回 false）
TEST_F(WaitTextConditionTest, SinglePatternMatches) {
    inject_output(make_text_msg("你的基本内功进步了！"));

    auto runner = make_runner();
    // 10ms 超时：条件满足会在第 1 次循环立即返回 true
    EXPECT_TRUE(runner.wait_cond("text 进步了", 10));
}

TEST_F(WaitTextConditionTest, SinglePatternNotMatches) {
    inject_output(make_text_msg("你的基本内功退步了！"));

    auto runner = make_runner();
    // 条件不满足会等待直到超时（10ms）返回 false
    EXPECT_FALSE(runner.wait_cond("text 进步了", 10));
}

TEST_F(WaitTextConditionTest, MultiPatternFirstMatches) {
    inject_output(make_text_msg("你的基本内功进步了！"));

    auto runner = make_runner();
    EXPECT_TRUE(runner.wait_cond("text 进步了|退步了|没有学到", 10));
}

TEST_F(WaitTextConditionTest, MultiPatternSecondMatches) {
    inject_output(make_text_msg("你的基本内功退步了！"));

    auto runner = make_runner();
    EXPECT_TRUE(runner.wait_cond("text 进步了|退步了|没有学到", 10));
}

TEST_F(WaitTextConditionTest, MultiPatternThirdMatches) {
    inject_output(make_text_msg("这项技能你今天没有学到。"));

    auto runner = make_runner();
    EXPECT_TRUE(runner.wait_cond("text 进步了|退步了|没有学到", 10));
}

TEST_F(WaitTextConditionTest, MultiPatternNoneMatches) {
    inject_output(make_text_msg("师傅今天太累了。"));

    auto runner = make_runner();
    EXPECT_FALSE(runner.wait_cond("text 进步了|退步了|没有学到", 10));
}

TEST_F(WaitTextConditionTest, MultiPatternWithSpacesAroundPipe) {
    inject_output(make_text_msg("你的基本内功进步了！"));

    auto runner = make_runner();
    // 模式间的空格会被 trim 掉
    EXPECT_TRUE(runner.wait_cond("text 进步了 | 退步了 | 没有学到", 10));
}

TEST_F(WaitTextConditionTest, EmptyPatternAfterTrimIsIgnored) {
    inject_output(make_text_msg("进步了！"));

    auto runner = make_runner();
    // 连续的 || 会产生空模式，应被忽略
    EXPECT_TRUE(runner.wait_cond("text 进步了||退步了", 10));
}

TEST_F(WaitTextConditionTest, AllEmptyPatternsFails) {
    inject_output(make_text_msg("任何文本"));

    auto runner = make_runner();
    // 全空模式会被视为无效条件，wait_cond 会立即因 bad condition 返回 false
    EXPECT_FALSE(runner.wait_cond("text |||", 10));
}
