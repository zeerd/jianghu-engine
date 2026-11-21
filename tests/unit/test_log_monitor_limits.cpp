// test_log_monitor_limits.cpp — 日志监控的有界性与落盘失败分支
//
// 覆盖：环形缓冲上界与缩容裁剪、签名 TTL 表上限收束、
// 上下文/plan 落盘失败时的中止路径。
//
// 构建/运行：仓库根执行 .\run-unit-tests.ps1（唯一认可方式）
#include "pch.h"

#include <memory>
#include <string>

#include "test_log_monitor_common.h"

using log_monitor_test::FakeRunner;
using log_monitor_test::LogMonitorTest;
using log_monitor_test::make_config;

TEST_F(LogMonitorTest, RingOverflowDropsOldestAndKeepsTail)
{
    CLogMonitor monitor;
    LogMonitorConfig config = make_config();
    config.context_lines    = 10;
    monitor.configure(config);

    for (int i = 0; i < 30; i++) {
        monitor.on_line(LOG_LVL_INFO, "[INFO  ] (2026-11-21 10:00:00) line "
                                          + std::to_string(i));
    }
    // 有界缓冲：丢最旧并计数，绝不为告警积压阻塞日志线程
    EXPECT_EQ(monitor.dropped_lines(), 20u);

    const std::string snapshot = monitor.snapshot_context();
    EXPECT_EQ(snapshot.find("line 19"), std::string::npos);
    EXPECT_EQ(snapshot.find("line 30"), std::string::npos);
    EXPECT_NE(snapshot.find("line 20"), std::string::npos);
    EXPECT_NE(snapshot.find("line 29"), std::string::npos);

    // 缩容重配：超上限的存量行同样被裁掉并计入丢弃
    config.context_lines = 5;
    monitor.configure(config);
    EXPECT_EQ(monitor.dropped_lines(), 25u);
    EXPECT_EQ(monitor.snapshot_context().find("line 24"), std::string::npos);
    EXPECT_NE(monitor.snapshot_context().find("line 29"), std::string::npos);
}

TEST_F(LogMonitorTest, SignatureTableStaysBoundedUnderFlood)
{
    // 单轮快照容量封顶为 context_lines：每轮贡献 100 个新签名。
    // id 用字母编码：纯数字会被签名归一化折叠成同一个签名（那是同
    // 一个问题），字面差异必须落在非数字位置才是 4500 个不同问题。
    auto letter_id = [](int n) {
        std::string id;
        do {
            id += static_cast<char>('a' + n % 26);
            n = n / 26 - 1;
        } while (n >= 0);
        return id;
    };

    CLogMonitor monitor;
    LogMonitorConfig config = make_config();
    config.context_lines    = 100;
    auto runner             = std::make_unique<FakeRunner>();
    auto* rp                = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(config);

    int64_t now = 1000;
    // 4500 个互不相同的签名越过 4096 上限：剪除逻辑（过期剪 + 最旧淘汰）
    // 必须收束，否则本用例死循环；淘汰后系统仍能认领新问题
    for (int round = 0; round < 45; round++) {
        for (int i = 0; i < 100; i++) {
            monitor.on_line(LOG_LVL_ERROR,
                            "[ERROR] (2026-11-21 10:00:00) unique problem "
                                + letter_id(round * 100 + i));
        }
        ASSERT_TRUE(monitor.process_pending(now));
        now += 1000;
    }
    EXPECT_EQ(rp->call_count, 45);

    // 表有界之后，新签名照常触发（身份未因淘汰而错乱）
    monitor.on_line(LOG_LVL_ERROR,
                    "[ERROR] (2026-11-21 10:00:00) post-flood problem x");
    ASSERT_TRUE(monitor.process_pending(now));
    EXPECT_EQ(rp->call_count, 46);
}

TEST_F(LogMonitorTest, ContextWriteFailureAbortsRound)
{
    // 用同名文件占住日志根路径：上下文文件落不了盘，本轮中止，
    // 分析通道不被无效调用
    const std::string blocked = m_log_dir + "-blocked";
    FILE* probe               = fopen(blocked.c_str(), "wb");
    ASSERT_NE(probe, nullptr);
    fclose(probe);
    logger_set_path(blocked.c_str());

    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) boom");
    EXPECT_FALSE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 0);

    std::error_code ec;
    std::filesystem::remove(blocked, ec);
}

TEST_F(LogMonitorTest, PlanSaveFailureAbortsRound)
{
    // plans 目录位被同名文件占住：分析已完成但 plan 无处落盘 → 本轮判失败
    std::error_code ec;
    std::filesystem::create_directories(m_log_dir, ec);
    const std::string blocked_dir = m_log_dir + "/plans";
    {
        FILE* fp = fopen(blocked_dir.c_str(), "wb");
        ASSERT_NE(fp, nullptr);
        fclose(fp);
    }

    CLogMonitor monitor;
    auto runner = std::make_unique<FakeRunner>();
    auto* rp    = runner.get();
    monitor.set_dependencies(nullptr, std::move(runner));
    monitor.configure(make_config());

    monitor.on_line(LOG_LVL_ERROR, "[ERROR] (2026-11-21 10:00:00) boom");
    EXPECT_FALSE(monitor.process_pending(1000));
    EXPECT_EQ(rp->call_count, 1);  // 分析先于落盘：分析已发生
}
