/**
 * @file LogMonitor.h
 * @brief 日志监控：挂钩日志输出，发现新问题推飞书——配了 cli_path 走
 *        opencode 分析出 plan 再推，只配 webhook 则把日志快照直接推上去。
 *
 * 触发模型（为什么这样设计见 docs/design/log-monitor.md）：
 * - 挂钩侧把日志行写进有界环形缓冲，达阈值的行只置触发标志——
 *   触发是信号不是逐条队列，没有「消费掉某一行」的路径；
 * - 监控线程唤醒后经合流窗口再快照最近若干行，并发同时落盘的
 *   多条日志一并进入同一轮分析；
 * - 快照内按签名做 TTL 去重：同一处问题（数字归一化后同签名）
 *   在 TTL 内只告警一次，不同签名互不阻塞。
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "FeishuClient.h"
#include "Logger.h"
#include "OpencodeRunner.h"

// [opencode] 配置段。cli_path 与 feishu_webhook 均为空表示不启用监控
// （不启动线程）。各键含义与默认值以 config.ini 注释为源头，此处不复制清单。
struct LogMonitorConfig {
    std::string cli_path;        // opencode CLI 可执行文件路径（空 = 不做分析）
    std::string feishu_webhook;  // 飞书自定义机器人 webhook（空 = 只落盘）
    std::string model;           // 模型（provider/model），空 = CLI 默认
    std::string work_dir;        // opencode 工作目录，空 = 继承服务器目录
    int level         = LOG_LVL_WARN;  // 触发分析的最低日志级别
    int cooldown_sec  = 86400;         // 同签名问题的触发 TTL（秒）
    int timeout_sec   = 300;           // 单次 opencode 分析超时（秒）
    int flush_ms      = 500;           // 唤醒后的合流窗口（毫秒）
    int context_lines = 100;           // 快照携带的最近日志行数
};

// 从已加载的 ini 读取 [opencode] 段并钳制到合法值域。
LogMonitorConfig load_log_monitor_config();

// 日志签名：剥掉 "[级别] (时间戳) " 前缀、数字串归一化后 FNV-1a 哈希。
// 同一处代码抛出的问题（仅 id/计数不同）得到同一签名——签名即问题身份。
uint64_t log_signature(LogLevel level, const std::string& line);

// 从日志行解析级别（"[ERROR] (…)" → LOG_LVL_ERROR）；识别失败返回 -1。
int log_level_from_line(const std::string& line);

// 级别名（与 LogLevel 枚举同序）：小写形供配置解析与 plan 文件名，
// 大写形供提示词与推送正文。
const char* level_lower_name(int level);
const char* level_upper_name(int level);

// 分析提示词：上下文经 --file 附件携带，提示词只描述任务与产出格式。
std::string build_analysis_prompt(const LogMonitorConfig& config);

// 飞书推送正文：级别 + 触发行 + 正文（plan 全文或日志快照）；
// body_path 非空时附落盘路径，为空（直推模式）时省略路径尾巴。
// 超过 28KB 在 UTF-8 边界截断，有落盘路径的截断注记里带上路径。
std::string build_feishu_message(const std::string& body,
                                 const std::string& body_path,
                                 const std::string& trigger_line, int level);

class CLogMonitor {
public:
    CLogMonitor();
    ~CLogMonitor();

    CLogMonitor(const CLogMonitor&)            = delete;
    CLogMonitor& operator=(const CLogMonitor&) = delete;

    // 注入依赖（start 前注入一次；停线程后才可再次注入）
    void set_dependencies(std::unique_ptr<IFeishuSender> sender,
                          std::unique_ptr<IOpencodeRunner> runner);
    // 配置（start 前调用；配置写定后不再变更，挂钩侧无锁读取）
    void configure(const LogMonitorConfig& config);

    // 启动监控线程（cli_path 与 feishu_webhook 均为空时不启动）
    void start();
    // 停止：置停止位、取消在途分析并等待线程退出；可重复调用
    void stop();

    // 是否启用：由 cli_path 或 feishu_webhook 决定（挂钩侧据此做零成本早退）
    bool enabled() const
    {
        return !m_config.cli_path.empty() || !m_config.feishu_webhook.empty();
    }

    // 日志入口：写环形缓冲；达到阈值的行置触发标志并唤醒监控线程。
    // 未启用时直接返回。
    void on_line(LogLevel level, const std::string& line);

    // 一轮处理：触发判定 → 快照 → 签名去重 → 分析 → 落盘 → 推送；
    // 未配 cli_path 时跳过分析，快照直推飞书。
    // 由监控线程调用；now_ms 由调用方给出（TTL 判定的时间源）。
    // 返回是否执行了分析或直推。
    bool process_pending(int64_t now_ms);

    // 快照环形缓冲最近若干行（'\n' 连接）——分析消费的对象是快照，
    // 不是触发信号的那一条日志。
    std::string snapshot_context();

    // 环形缓冲溢出累计丢弃行数（诊断用：缓冲有界、绝不阻塞日志线程）
    size_t dropped_lines() const;

private:
    void thread_main();
    // 签名 TTL 判定并登记（仅监控线程/单测直接调用的串行上下文）
    bool should_trigger(uint64_t signature, int64_t now_ms);
    // 上下文落盘，返回文件路径；失败返回空串
    std::string write_context_file(const std::string& snapshot);
    // plan 落盘，返回文件路径；失败返回空串
    std::string save_plan(const std::string& plan, int level);
    // 直推模式（无 opencode）：把快照正文推飞书，推送结果记 INFO
    void push_snapshot(const std::string& snapshot,
                       const std::string& trigger_line, int level);
    // 分析通道一轮：上下文落盘 → opencode 分析 → plan 落盘 → 推送
    bool analyze_snapshot(const std::string& snapshot,
                          const std::string& trigger_line, int level,
                          int fresh_count);
    // 推送并记 INFO：失败只留痕，不判轮次失败
    void push_message(const std::string& msg);

    LogMonitorConfig m_config;
    std::unique_ptr<IFeishuSender> m_sender;
    std::unique_ptr<IOpencodeRunner> m_runner;

    std::atomic<bool> m_stop{false};
    // 触发标志：达阈值的日志行只置位，不排队——消费的对象是环形缓冲快照
    std::atomic<bool> m_pending{false};
    std::thread m_thread;

    mutable std::mutex m_mutex;  // 保护环形缓冲与「缓冲+触发」的唤醒协议
    std::condition_variable m_cv;
    std::deque<std::string> m_ring;
    size_t m_ring_cap = 100;
    size_t m_dropped  = 0;

    // 签名 → 到期时刻；仅监控线程（或未启动时的串行调用方）访问
    std::unordered_map<uint64_t, int64_t> m_seen;
};

// 全局实例与生命周期入口 ----------------------------------------------

// 全局监控实例（进程单例）
CLogMonitor& log_monitor();

// Logger 挂钩入口：未启用时一次布尔判断即返回
void log_monitor_on_line(LogLevel level, const char* line);

// 启动/停止（配置来自已加载的 ini；main 在初始化成功后启动、
// 窗口销毁时停止）
void log_monitor_start();
void log_monitor_stop();
