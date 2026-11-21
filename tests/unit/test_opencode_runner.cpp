// test_opencode_runner.cpp — opencode CLI 分析：JSONL 输出解析、命令行拼装
// 与真实子进程执行（自有子进程、按句柄收口）
//
// 构建/运行：仓库根执行 .\run-unit-tests.ps1（唯一认可方式）
#include "pch.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>

#include "OpencodeRunner.h"
#include "test_tmp_dir.h"

namespace {

// 记录调用参数，回放预设输出（进程层假件：不创建真进程）
class FakeProcessRunner : public IProcessRunner {
public:
    ProcessResult run(const std::string& exe, const std::string& args,
                      const std::string& cwd, int timeout_ms,
                      std::string& out_output) override
    {
        last_exe        = exe;
        last_args       = args;
        last_cwd        = cwd;
        last_timeout_ms = timeout_ms;
        ++run_count;
        if (result != ProcessResult::Ok) return result;
        out_output = output;
        return result;
    }

    void cancel() override { ++cancel_count; }

    std::string last_exe;
    std::string last_args;
    std::string last_cwd;
    int last_timeout_ms  = 0;
    int run_count        = 0;
    int cancel_count     = 0;
    ProcessResult result = ProcessResult::Ok;
    std::string output;
};

const char* kTextEvent
    = "{\"type\":\"text\",\"part\":{\"type\":\"text\",\"text\":\"PONG\"}}\n";

}  // namespace

// ---------------------------------------------------------------------
// JSONL 解析
// ---------------------------------------------------------------------

TEST(ParseOpencodeOutput, ExtractsTextPartsInOrder)
{
    const std::string jsonl
        = "{\"type\":\"step_start\"}\n"
          "{\"type\":\"text\",\"part\":{\"type\":\"text\",\"text\":\"part "
          "one\"}}\n"
          "{\"type\":\"text\",\"part\":{\"type\":\"text\",\"text\":\"part "
          "two\"}}\n"
          "{\"type\":\"step_finish\",\"reason\":\"stop\"}\n";
    std::string text, err;
    EXPECT_TRUE(parse_opencode_output(jsonl, text, err));
    EXPECT_EQ(text, "part one\npart two");
    EXPECT_TRUE(err.empty());
}

TEST(ParseOpencodeOutput, CapturesErrorEventWithoutText)
{
    const std::string jsonl
        = "{\"type\":\"error\",\"error\":{\"type\":\"provider.quota\","
          "\"message\":\"no credits\"}}\n";
    std::string text, err;
    EXPECT_FALSE(parse_opencode_output(jsonl, text, err));
    EXPECT_EQ(err, "no credits");
    EXPECT_TRUE(text.empty());
}

TEST(ParseOpencodeOutput, SkipsGarbageAndBlankLines)
{
    const std::string jsonl
        = "opencode: starting\n"
          "\n"
          "{not json at all}\n"
          "{\"type\":\"text\",\"part\":{\"type\":\"text\",\"text\":\"ok\"}}\n";
    std::string text, err;
    EXPECT_TRUE(parse_opencode_output(jsonl, text, err));
    EXPECT_EQ(text, "ok");
}

TEST(ParseOpencodeOutput, EmptyOutputYieldsFailure)
{
    std::string text, err;
    EXPECT_FALSE(parse_opencode_output("", text, err));
    EXPECT_TRUE(text.empty());
    EXPECT_TRUE(err.empty());
}

TEST(ParseOpencodeOutput, TextEventWithoutPartTextYieldsFailure)
{
    // 畸形 text 事件（缺 part.text）不产出文本
    const std::string jsonl = "{\"type\":\"text\"}\n";
    std::string text, err;
    EXPECT_FALSE(parse_opencode_output(jsonl, text, err));
}

// ---------------------------------------------------------------------
// 分析编排（假进程注入）
// ---------------------------------------------------------------------

TEST(OpencodeRunner, BuildsCommandLineWithContextFile)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->output = kTextEvent;
    COpencodeRunner runner(std::move(process), "C:\\tools\\opencode-cli.exe",
                           "C:\\work dir", "prov/model", 30);

    std::string plan;
    ASSERT_TRUE(
        runner.analyze("analyze this log", "C:\\tmp\\ctx file.txt", plan));
    EXPECT_EQ(plan, "PONG");
    EXPECT_EQ(probe->last_exe, "C:\\tools\\opencode-cli.exe");
    EXPECT_EQ(probe->last_cwd, "C:\\work dir");
    EXPECT_EQ(probe->last_timeout_ms, 30000);

    // 事件流输出、附件与提示词齐备；含空格的路径被整体引用
    EXPECT_NE(probe->last_args.find("run --format json --auto"),
              std::string::npos);
    EXPECT_NE(probe->last_args.find("--model prov/model"), std::string::npos);
    EXPECT_NE(probe->last_args.find("--file \"C:\\tmp\\ctx file.txt\""),
              std::string::npos);
    EXPECT_NE(probe->last_args.find("\"analyze this log\""), std::string::npos);
}

TEST(OpencodeRunner, OmitsModelFlagWhenModelEmpty)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->output = kTextEvent;
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    std::string plan;
    ASSERT_TRUE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_EQ(probe->last_args.find("--model"), std::string::npos);
    EXPECT_EQ(probe->last_cwd, "");  // 空 cwd = 继承服务器工作目录
}

TEST(OpencodeRunner, ProcessFailureYieldsFalse)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->result = ProcessResult::Failed;
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_TRUE(plan.empty());
    EXPECT_EQ(probe->run_count, 1);
}

TEST(OpencodeRunner, TimedOutResultYieldsFalse)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->result = ProcessResult::TimedOut;
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_TRUE(plan.empty());
    EXPECT_EQ(probe->run_count, 1);
}

TEST(OpencodeRunner, CanceledResultYieldsFalse)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->result = ProcessResult::Canceled;
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_TRUE(plan.empty());
    EXPECT_EQ(probe->run_count, 1);
}

TEST(OpencodeRunner, OutputWithoutTextYieldsFalse)
{
    auto process = std::make_unique<FakeProcessRunner>();
    process->output
        = "{\"type\":\"error\",\"error\":{\"message\":\"quota\"}}\n";
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_TRUE(plan.empty());
}

TEST(OpencodeRunner, EmptyCliPathSkipsProcess)
{
    auto process = std::make_unique<FakeProcessRunner>();
    auto* probe  = process.get();
    COpencodeRunner runner(std::move(process), "", "", "", 5);

    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_EQ(probe->run_count, 0);
}

TEST(OpencodeRunner, CancelDelegatesToProcess)
{
    auto process = std::make_unique<FakeProcessRunner>();
    auto* probe  = process.get();
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    runner.cancel();
    EXPECT_EQ(probe->cancel_count, 1);
}

TEST(OpencodeRunner, NullProcessYieldsFalse)
{
    COpencodeRunner runner(nullptr, "oc.exe", "", "", 5);
    std::string plan;
    EXPECT_FALSE(runner.analyze("x y", "ctx.txt", plan));
}

TEST(OpencodeRunner, ResolvesContextFileToAbsolutePath)
{
    // 上下文文件按服务器自己的工作目录写入，交给子进程前必须解析成
    // 绝对路径：opencode 在 work_dir 下运行，相对路径会在那里找文件
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->output = kTextEvent;
    COpencodeRunner runner(std::move(process), "oc.exe", "", "", 5);

    const std::string relative = "log\\plan-context.txt";
    std::string plan;
    ASSERT_TRUE(runner.analyze("x y", relative, plan));

    char expected[MAX_PATH * 4] = {0};
    ASSERT_NE(GetFullPathNameA(relative.c_str(), (DWORD)sizeof(expected),
                               expected, NULL),
              0u);
    EXPECT_NE(probe->last_args.find(expected), std::string::npos);
}

TEST(OpencodeRunner, QuotesSpecialCharactersInModel)
{
    auto process  = std::make_unique<FakeProcessRunner>();
    auto* probe   = process.get();
    probe->output = kTextEvent;
    // 空格、内嵌引号与结尾反斜杠三种引用规则各命中一次
    COpencodeRunner runner(std::move(process), "oc.exe", "", "a \"b\" \\", 5);

    std::string plan;
    ASSERT_TRUE(runner.analyze("x y", "ctx.txt", plan));
    EXPECT_NE(probe->last_args.find("--model \"a \\\"b\\\" \\\\\""),
              std::string::npos);
}

// ---------------------------------------------------------------------
// 真实子进程（CWinProcessRunner）：只动自己启动的进程，按句柄收口
// ---------------------------------------------------------------------

TEST(WinProcessRunner, CapturesStdoutAndRespectsCwd)
{
    CWinProcessRunner runner;
    const std::string cwd = (test_tmp_dir() / "proc_runner").string();
    std::error_code ec;
    std::filesystem::create_directories(cwd, ec);

    std::string out;
    ASSERT_EQ(runner.run("cmd.exe", "/c cd", cwd, 5000, out),
              ProcessResult::Ok);
    // /c cd 打印工作目录：cwd 参数真实生效
    EXPECT_NE(out.find("proc_runner"), std::string::npos);
}

TEST(WinProcessRunner, MissingExecutableReturnsFailed)
{
    CWinProcessRunner runner;
    std::string out;
    EXPECT_EQ(
        runner.run("C:\\no-such-dir\\missing-tool-xyz.exe", "", "", 1000, out),
        ProcessResult::Failed);
}

TEST(WinProcessRunner, TimeoutTerminatesChild)
{
    CWinProcessRunner runner;
    std::string out;
    // 长命子进程 + 短超时：到点按自有句柄终止（不等子进程自然退出），
    // 结果分流为 TimedOut——与「进程没跑起来」的 Failed 区分开
    EXPECT_EQ(runner.run("cmd.exe", "/c ping -n 30 127.0.0.1", "", 300, out),
              ProcessResult::TimedOut);
}

TEST(WinProcessRunner, CancelBeforeRunSkipsSpawn)
{
    CWinProcessRunner runner;
    runner.cancel();  // 粘性取消
    std::string out;
    EXPECT_EQ(runner.run("cmd.exe", "/c echo never", "", 1000, out),
              ProcessResult::Canceled);
}
