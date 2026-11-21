// test_log_monitor.cpp — 日志监控：配置读取、签名 TTL、快照与分析编排
// （缓冲上界与落盘失败分支见 test_log_monitor_limits.cpp）
//
// 构建/运行：仓库根执行 .\run-unit-tests.ps1（唯一认可方式）
#include "pch.h"

#include <chrono>
#include <string>

#include "StringUtil.h"
#include "test_log_hits.h"
#include "test_log_monitor_common.h"

using log_monitor_test::FakeRunner;
using log_monitor_test::FakeSender;
using log_monitor_test::LogMonitorTest;
using log_monitor_test::make_config;
using log_monitor_test::read_all;

// ---------------------------------------------------------------------
// 配置读取
// ---------------------------------------------------------------------

TEST_F(LogMonitorTest, ConfigReadsIniSection)
{
    g_ini_data["opencode"]["cli_path"]       = "D:\\oc.exe";
    g_ini_data["opencode"]["feishu_webhook"] = "https://hook/x";
    g_ini_data["opencode"]["model"]          = "prov/model";
    g_ini_data["opencode"]["work_dir"]       = "D:\\repo";
    g_ini_data["opencode"]["level"]          = "error";
    g_ini_data["opencode"]["cooldown"]       = "3600";
    g_ini_data["opencode"]["timeout"]        = "60";
    g_ini_data["opencode"]["flush_ms"]       = "200";
    g_ini_data["opencode"]["context_lines"]  = "50";

    const LogMonitorConfig config = load_log_monitor_config();
    EXPECT_EQ(config.cli_path, "D:\\oc.exe");
    EXPECT_EQ(config.feishu_webhook, "https://hook/x");
    EXPECT_EQ(config.model, "prov/model");
    EXPECT_EQ(config.work_dir, "D:\\repo");
    EXPECT_EQ(config.level, LOG_LVL_ERROR);
    EXPECT_EQ(config.cooldown_sec, 3600);
    EXPECT_EQ(config.timeout_sec, 60);
    EXPECT_EQ(config.flush_ms, 200);
    EXPECT_EQ(config.context_lines, 50);
}

TEST_F(LogMonitorTest, ConfigClampsOutOfRangeValues)
{
    g_ini_data["opencode"]["level"]         = "bogus";  // 未知级别 → warn
    g_ini_data["opencode"]["cooldown"]      = "0";
    g_ini_data["opencode"]["timeout"]       = "-5";
    g_ini_data["opencode"]["flush_ms"]      = "-1";
    g_ini_data["opencode"]["context_lines"] = "3";

    const LogMonitorConfig config = load_log_monitor_config();
    EXPECT_EQ(config.level, LOG_LVL_WARN);
    EXPECT_EQ(config.cooldown_sec, 1);
    EXPECT_EQ(config.timeout_sec, 1);
    EXPECT_EQ(config.flush_ms, 0);
    EXPECT_EQ(config.context_lines, 10);
}

TEST_F(LogMonitorTest, ConfigDefaultsWhenSectionAbsent)
{
    g_ini_data.erase("opencode");
    const LogMonitorConfig config = load_log_monitor_config();
    EXPECT_TRUE(config.cli_path.empty());
    EXPECT_TRUE(config.feishu_webhook.empty());
    EXPECT_EQ(config.level, LOG_LVL_WARN);
    EXPECT_EQ(config.cooldown_sec, 86400);
    EXPECT_EQ(config.timeout_sec, 300);
}

// ---------------------------------------------------------------------
// 签名与级别识别
// ---------------------------------------------------------------------

TEST(LogSignature, SameTemplateDifferentIdsShareSignature)
{
    const uint64_t a = log_signature(
        LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) 缓冲越界: 用户 12345");
    const uint64_t b = log_signature(
        LOG_LVL_ERROR, "[ERROR] (2026-11-21 11:22:33) 缓冲越界: 用户 67890");
    EXPECT_EQ(a, b);
}

TEST(LogSignature, DifferentMessagesDiffer)
{
    const uint64_t a
        = log_signature(LOG_LVL_ERROR, "[ERROR] (2026) 缓冲越界: 用户 1");
    const uint64_t b
        = log_signature(LOG_LVL_ERROR, "[ERROR] (2026) 连接超时: 用户 1");
    EXPECT_NE(a, b);
}

TEST(LogSignature, LevelParticipates)
{
    EXPECT_NE(log_signature(LOG_LVL_ERROR, "[ERROR] (2026) boom"),
              log_signature(LOG_LVL_WARN, "[WARN  ] (2026) boom"));
}

TEST(LogSignature, TimestampPrefixIrrelevant)
{
    // 快照行（带前缀）与裸消息同签名：剥前缀只在行首做一次
    const uint64_t a = log_signature(
        LOG_LVL_WARN, "[WARN  ] (2026-11-21 10:00:00) reset id=7");
    const uint64_t b = log_signature(LOG_LVL_WARN, "reset id=7");
    EXPECT_EQ(a, b);
}

TEST(LogLevelFromLine, RecognizesLevelNames)
{
    EXPECT_EQ(log_level_from_line("[ERROR] (2026-01-01 00:00:00) x"),
              LOG_LVL_ERROR);
    EXPECT_EQ(log_level_from_line("[WARN  ] (2026-01-01 00:00:00) x"),
              LOG_LVL_WARN);
    EXPECT_EQ(log_level_from_line("[INFO  ] (2026-01-01 00:00:00) x"),
              LOG_LVL_INFO);
    EXPECT_EQ(log_level_from_line("plain message"), -1);
    EXPECT_EQ(log_level_from_line("[BOGUS] x"), -1);
}

// ---------------------------------------------------------------------
// 提示词与推送正文
// ---------------------------------------------------------------------

TEST(BuildAnalysisPrompt, NamesFocusLevelAndOutputFormat)
{
    LogMonitorConfig config  = make_config();
    const std::string prompt = build_analysis_prompt(config);
    EXPECT_FALSE(prompt.empty());
    EXPECT_NE(prompt.find("WARN"), std::string::npos);
    EXPECT_NE(prompt.find("Markdown"), std::string::npos);

    config.level = LOG_LVL_ERROR;
    EXPECT_NE(build_analysis_prompt(config).find("ERROR"), std::string::npos);
}

TEST(BuildFeishuMessage, CarriesPlanTriggerAndPath)
{
    const std::string msg = build_feishu_message(
        "## plan 内容", "log/plans/1-1-000000-error.md",
        "[ERROR] (2026-11-21 10:00:00) boom", LOG_LVL_ERROR);
    EXPECT_NE(msg.find("## plan 内容"), std::string::npos);
    EXPECT_NE(msg.find("log/plans/1-1-000000-error.md"), std::string::npos);
    EXPECT_NE(msg.find("[ERROR] (2026-11-21 10:00:00) boom"),
              std::string::npos);
    EXPECT_NE(msg.find("ERROR"), std::string::npos);
}

TEST(BuildFeishuMessage, TruncatesOversizedPlanKeepingPath)
{
    std::string plan;
    while (plan.size() < 40 * 1024) plan += "错误边界错误边界错误边界\n";

    const std::string msg
        = build_feishu_message(plan, "log/plans/p.md", "t", LOG_LVL_WARN);
    EXPECT_LE(msg.size(), 28u * 1024u);
    // 截断后路径仍可溯源（注记里带路径）
    EXPECT_NE(msg.find("log/plans/p.md"), std::string::npos);
    EXPECT_NE(msg.find("已截断"), std::string::npos);
}

// ---------------------------------------------------------------------
// 监控行为
// ---------------------------------------------------------------------

TEST_F(LogMonitorTest, DisabledMonitorIgnoresLines)
{
    CLogMonitor monitor;  // 未配置 cli_path = 不启用
    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 00:00:00) boom");
    EXPECT_FALSE(monitor.enabled());
    EXPECT_FALSE(monitor.process_pending(1000));
}

TEST_F(LogMonitorTest, BelowThresholdLineDoesNotTrigger)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());  // level = warn

    monitor.on_line(LOG_LVL_INFO, "[INFO  ] (2026-11-21 00:00:00) fine");
    EXPECT_FALSE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 0);
}

TEST_F(LogMonitorTest, MissingRunnerConsumesTrigger)
{
    CLogMonitor monitor;
    monitor.configure(make_config());
    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 00:00:00) boom");
    EXPECT_FALSE(monitor.process_pending(1000));
    EXPECT_EQ(monitor.dropped_lines(), 0u);
}

TEST_F(LogMonitorTest, ErrorTriggersAnalysisAndPush)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    auto sender = std::make_unique<FakeSender>();
    auto* sp    = sender.get();
    monitor.set_dependencies(std::move(sender), std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:00:00) 缓冲越界: 用户 12345");
    ASSERT_TRUE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);

    // 上下文文件携带整段快照（分析的输入是快照，不是触发单行），
    // 且出生即绝对路径——子进程在 work_dir 下也能按原样找到它
    ASSERT_FALSE(rp->last_context_file.empty());
    EXPECT_TRUE(std::filesystem::path(rp->last_context_file).is_absolute());
    const std::string context = read_all(rp->last_context_file);
    EXPECT_NE(context.find("缓冲越界: 用户 12345"), std::string::npos);

    // 推送正文含 plan，且报告的落盘路径是绝对的（可直接打开）
    ASSERT_EQ(sp->messages.size(), 1u);
    EXPECT_NE(sp->messages[0].find(rp->plan), std::string::npos);
    EXPECT_NE(sp->messages[0].find(absolute_path(m_log_dir) + "/plans"),
              std::string::npos);

    // plan 落盘在 <log>/plans 下且内容一致
    bool found = false;
    const std::filesystem::path plan_dir
        = std::filesystem::path(m_log_dir) / "plans";
    ASSERT_TRUE(std::filesystem::exists(plan_dir));
    for (const auto& entry : std::filesystem::directory_iterator(plan_dir)) {
        if (read_all(entry.path().string()) == rp->plan) found = true;
    }
    EXPECT_TRUE(found);

    // 生命周期日志齐备：分析开始（生成中可见）、完成（耗时 + plan 路径）、
    // 推送结果——三段都能从日志直读
    const std::string log = read_log_content();
    EXPECT_NE(log.find("log_monitor: analyzing 1 new signature(s)"),
              std::string::npos);
    EXPECT_NE(log.find("log_monitor: plan saved to "), std::string::npos);
    EXPECT_NE(log.find("log_monitor: feishu push ok"), std::string::npos);
}

TEST_F(LogMonitorTest, DuplicateSignatureSuppressedWithinTtl)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:00:00) 缓冲越界: 用户 1");
    ASSERT_TRUE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);

    // 同签名（仅 id 不同）在 TTL 内：被压制，不再分析
    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:05:00) 缓冲越界: 用户 2");
    EXPECT_FALSE(monitor.process_pending(2000));
    EXPECT_EQ(rp->call_count, 1);

    // 越过 TTL（默认一天）：同一问题再次分析
    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-22 10:00:00) 缓冲越界: 用户 3");
    const int64_t after_ttl = 1000 + static_cast<int64_t>(86400) * 1000;
    ASSERT_TRUE(monitor.process_pending(after_ttl));
    EXPECT_EQ(rp->call_count, 2);
}

TEST_F(LogMonitorTest, DistinctSignaturesMergeIntoOneRound)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    // 两条不同问题一并到达（并发落盘的形态）：一轮分析一起带走
    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) 缓冲越界");
    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:00:00) 网络发送失败");
    ASSERT_TRUE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);

    ASSERT_FALSE(rp->last_context_file.empty());
    const std::string context = read_all(rp->last_context_file);
    EXPECT_NE(context.find("缓冲越界"), std::string::npos);
    EXPECT_NE(context.find("网络发送失败"), std::string::npos);
}

TEST_F(LogMonitorTest, FailedAnalysisKeepsSignatureQuiet)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    rp->succeed = false;
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) boom");
    EXPECT_FALSE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);

    // 分析失败也已认领签名：TTL 内不反复拉起（通道故障以 INFO 留痕）
    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:01:00) boom");
    EXPECT_FALSE(monitor.process_pending(2000));
    EXPECT_EQ(rp->call_count, 1);
}

TEST_F(LogMonitorTest, PushFailureDoesNotFailRound)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto sender = std::make_unique<FakeSender>();
    auto* sp    = sender.get();
    sp->result  = false;
    monitor.set_dependencies(std::move(sender), std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) boom");
    // 推送失败只是留痕：plan 已落盘，本轮结论仍是成功
    EXPECT_TRUE(monitor.process_pending(1000));
    EXPECT_EQ(sp->messages.size(), 1u);
}

TEST_F(LogMonitorTest, NoSenderStillSavesPlan)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    // webhook 为空的装配形态：不注入 sender，plan 照常落盘
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) boom");
    EXPECT_TRUE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);
}

TEST_F(LogMonitorTest, ThreadDrivesPipelineAndStopsCleanly)
{
    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    auto sender = std::make_unique<FakeSender>();
    auto* sp    = sender.get();
    monitor.set_dependencies(std::move(sender), std::move(runner));
    LogMonitorConfig config = make_config();
    config.flush_ms         = 1;
    monitor.configure(config);

    ASSERT_TRUE(monitor.enabled());
    monitor.start();

    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:00:00) 线程联动 boom");

    // 以「推送已发生」为完成信号（事件驱动等待，非延时）
    const std::future<void> future = sp->notified_promise.get_future();
    ASSERT_EQ(future.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
    EXPECT_EQ(rp->call_count, 1);
    EXPECT_NE(sp->messages[0].find("线程联动 boom"), std::string::npos);

    monitor.stop();
    // 关机取消已下达（在途分析的取消路径）
    EXPECT_EQ(rp->cancel_count, 1);
    // 重复停止安全（析构里还会再走一次 stop）
    monitor.stop();
}

// ---------------------------------------------------------------------
// 全局入口（挂钩与生命周期）
// ---------------------------------------------------------------------

TEST_F(LogMonitorTest, HookIsNoopWhenDisabled)
{
    EXPECT_FALSE(log_monitor().enabled());
    // 未配置时挂钩是空操作：不写缓冲、不置触发
    log_monitor_on_line(LOG_LVL_ERROR,
                        "[ERROR] (2026-11-21 00:00:00) disabled hook");
    EXPECT_FALSE(log_monitor().process_pending(1000));
}

TEST_F(LogMonitorTest, GlobalStartStopFollowsIni)
{
    // 未配置 cli_path：启动入口是空操作
    g_ini_data.erase("opencode");
    log_monitor_start();
    EXPECT_FALSE(log_monitor().enabled());

    // 配置后启动（webhook 为空的装配分支）
    g_ini_data["opencode"]["cli_path"] = "C:\\no-such-dir\\opencode-cli.exe";
    log_monitor_start();
    EXPECT_TRUE(log_monitor().enabled());
    log_monitor_stop();

    // 带 webhook 的装配分支；挂钩入口在启用态可安全写入
    g_ini_data["opencode"]["feishu_webhook"] = "https://127.0.0.1:1/hook";
    log_monitor_start();
    EXPECT_TRUE(log_monitor().enabled());
    // 启动横幅入日志：监控是否武装、参数与 plan 目录一眼可读
    EXPECT_NE(read_log_content().find("log_monitor: enabled"),
              std::string::npos);
    log_monitor_on_line(LOG_LVL_ERROR,
                        "[ERROR] (2026-11-21 00:00:00) global hook");
    log_monitor_stop();

    // 复位为停用：后续用例的日志不再进入监控
    g_ini_data.erase("opencode");
    log_monitor_start();
    EXPECT_FALSE(log_monitor().enabled());
}
