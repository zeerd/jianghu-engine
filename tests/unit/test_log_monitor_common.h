// test_log_monitor_common.h — 日志监控用例共享夹具与假件
//
// 仅供 test_log_monitor.cpp 与 test_log_monitor_limits.cpp 使用：
// FakeSender / FakeRunner、配置构造、文本读取与日志目录隔离夹具。
#pragma once

#include <cstdio>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "IniParser.h"
#include "LogMonitor.h"
#include "test_tmp_dir.h"

namespace log_monitor_test {

// 记录推送正文；以 promise 通知「推送已发生」（事件驱动等待，非延时）
class FakeSender : public IFeishuSender {
public:
    bool send(const std::string& text) override
    {
        messages.push_back(text);
        if (!notified) {
            notified = true;
            notified_promise.set_value();
        }
        return result;
    }

    std::vector<std::string> messages;
    bool result   = true;
    bool notified = false;
    std::promise<void> notified_promise;
};

// 记录分析参数，回放预设 plan
class FakeRunner : public IOpencodeRunner {
public:
    bool analyze(const std::string& prompt, const std::string& context_file,
                 std::string& out_plan) override
    {
        ++call_count;
        last_prompt       = prompt;
        last_context_file = context_file;
        if (!succeed) return false;
        out_plan = plan;
        return true;
    }

    void cancel() override { ++cancel_count; }

    int call_count   = 0;
    int cancel_count = 0;
    bool succeed     = true;
    std::string plan = "## 修复计划\n1. 缓冲区边界检查";
    std::string last_prompt;
    std::string last_context_file;
};

inline std::string read_all(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return "";
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
    fclose(fp);
    return out;
}

inline LogMonitorConfig make_config()
{
    LogMonitorConfig config;
    config.cli_path       = "C:\\tools\\opencode-cli.exe";
    config.feishu_webhook = "https://hook.example/x";
    config.level          = LOG_LVL_WARN;
    config.cooldown_sec   = 86400;
    config.timeout_sec    = 5;
    config.flush_ms       = 0;
    config.context_lines  = 100;
    return config;
}

// 日志目录指到测试临时目录，避免污染真实 log/
class LogMonitorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_saved_log_path = logger_get_path();
        m_log_dir        = (test_tmp_dir() / "log_monitor").string();
        std::error_code ec;
        std::filesystem::remove_all(m_log_dir, ec);
        logger_set_path(m_log_dir.c_str());
    }

    void TearDown() override
    {
        logger_set_path(m_saved_log_path.c_str());
        g_ini_data.erase("opencode");
    }

    std::string m_saved_log_path;
    std::string m_log_dir;
};

}  // namespace log_monitor_test
