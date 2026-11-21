/**
 * @file LogMonitor.cpp
 * @brief 日志监控实现：配置读取、签名去重、环形缓冲与告警编排；
 *        级别名、提示词与飞书正文在 LogMonitorMessage.cpp。 */

#include "LogMonitor.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "IniParser.h"
#include "StringUtil.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <chrono>

// 签名表上限：TTL 表只记「问题身份 → 到期时刻」，有界才能常驻内存
static const size_t kMaxSeenEntries = 4096;

// ---------------------------------------------------------------------
// 配置
// ---------------------------------------------------------------------

LogMonitorConfig load_log_monitor_config()
{
    LogMonitorConfig config;
    config.cli_path       = ini_get("opencode", "cli_path", "");
    config.feishu_webhook = ini_get("opencode", "feishu_webhook", "");
    config.model          = ini_get("opencode", "model", "");
    config.work_dir       = ini_get("opencode", "work_dir", "");
    config.cooldown_sec   = ini_get_int("opencode", "cooldown", 86400);
    config.timeout_sec    = ini_get_int("opencode", "timeout", 300);
    config.flush_ms       = ini_get_int("opencode", "flush_ms", 500);
    config.context_lines  = ini_get_int("opencode", "context_lines", 100);

    // 级别：按名取值，未知值回落 warn——宁可少触发也不逐行分析
    config.level          = LOG_LVL_WARN;
    const char* level_str = ini_get("opencode", "level", "warn");
    if (level_str && level_str[0]) {
        for (int i = 0; i <= LOG_LVL_FATAL; i++) {
            if (_stricmp(level_str, level_lower_name(i)) == 0) {
                config.level = i;
                break;
            }
        }
    }

    // 值域钳制：TTL 与超时至少 1 秒，合流窗口非负，上下文至少 10 行
    if (config.cooldown_sec < 1) config.cooldown_sec = 1;
    if (config.timeout_sec < 1) config.timeout_sec = 1;
    if (config.flush_ms < 0) config.flush_ms = 0;
    if (config.context_lines < 10) config.context_lines = 10;
    return config;
}

// ---------------------------------------------------------------------
// 签名与级别识别
// ---------------------------------------------------------------------

uint64_t log_signature(LogLevel level, const std::string& line)
{
    // 剥掉行首 "[LEVEL ] (时间戳) " 前缀——只在行首识别，
    // 消息体里出现同形状文本也不受影响
    size_t msg_begin = 0;
    if (!line.empty() && line.front() == '[') {
        size_t close = line.find(") ");
        if (close != std::string::npos) msg_begin = close + 2;
    }

    // 数字串归一化为 '#'：用户 id、计数、时间的差异不改变问题身份
    std::string norm;
    norm.reserve(line.size());
    for (size_t i = msg_begin; i < line.size(); i++) {
        char c = line[i];
        if (c >= '0' && c <= '9') {
            if (norm.empty() || norm.back() != '#') norm += '#';
        }
        else {
            norm += c;
        }
    }

    // FNV-1a 64：级别参与哈希——同一句话在不同级别是不同问题
    uint64_t hash = 14695981039346656037ULL;
    auto mix      = [&hash](unsigned char k) {
        hash ^= k;
        hash *= 1099511628211ULL;
    };
    mix(static_cast<unsigned char>(level));
    for (unsigned char c : norm) mix(c);
    return hash;
}

int log_level_from_line(const std::string& line)
{
    if (line.size() < 3 || line.front() != '[') return -1;
    size_t close = line.find(']');
    if (close == std::string::npos) return -1;

    std::string name = line.substr(1, close - 1);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    for (int i = 0; i <= LOG_LVL_FATAL; i++) {
        if (_stricmp(name.c_str(), level_upper_name(i)) == 0) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------
// CLogMonitor
// ---------------------------------------------------------------------

CLogMonitor::CLogMonitor() = default;
CLogMonitor::~CLogMonitor() { stop(); }

void CLogMonitor::set_dependencies(std::unique_ptr<IFeishuSender> sender,
                                   std::unique_ptr<IOpencodeRunner> runner)
{
    m_sender = std::move(sender);
    m_runner = std::move(runner);
}

void CLogMonitor::configure(const LogMonitorConfig& config)
{
    m_config = config;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ring_cap = static_cast<size_t>(
        config.context_lines > 0 ? config.context_lines : 1);
    while (m_ring.size() > m_ring_cap) {
        m_ring.pop_front();
        ++m_dropped;
    }
}

void CLogMonitor::start()
{
    if (!enabled() || m_thread.joinable()) return;
    m_stop   = false;
    m_thread = std::thread(&CLogMonitor::thread_main, this);
}

void CLogMonitor::stop()
{
    if (!m_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    // 取消在 join 之前：关机不等满在途分析的超时
    if (m_runner) m_runner->cancel();
    m_thread.join();
}

void CLogMonitor::on_line(LogLevel level, const std::string& line)
{
    if (!enabled()) return;
    const bool trigger = level >= m_config.level;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ring.push_back(line);
        while (m_ring.size() > m_ring_cap) {
            m_ring.pop_front();
            ++m_dropped;
        }
        // 先入缓冲、后置标志：监控线程见到标志时，置标志的行必然已在
        // 缓冲中——快照不可能漏掉触发行
        if (trigger) m_pending = true;
    }
    if (trigger) m_cv.notify_one();
}

std::string CLogMonitor::snapshot_context()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string out;
    for (const std::string& line : m_ring) {
        out += line;
        out += '\n';
    }
    return out;
}

size_t CLogMonitor::dropped_lines() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dropped;
}

bool CLogMonitor::should_trigger(uint64_t signature, int64_t now_ms)
{
    const int64_t ttl_ms = static_cast<int64_t>(m_config.cooldown_sec) * 1000;
    auto it              = m_seen.find(signature);
    if (it != m_seen.end() && now_ms < it->second) return false;  // TTL 内

    m_seen[signature] = now_ms + ttl_ms;
    if (m_seen.size() > kMaxSeenEntries) {
        // 先剪过期项
        for (auto i = m_seen.begin(); i != m_seen.end();) {
            if (now_ms >= i->second) {
                i = m_seen.erase(i);
            }
            else {
                ++i;
            }
        }
        // 仍超限：淘汰最早到期的（保住最近的问题身份）
        while (m_seen.size() > kMaxSeenEntries) {
            auto oldest = m_seen.begin();
            for (auto i = m_seen.begin(); i != m_seen.end(); ++i) {
                if (i->second < oldest->second) oldest = i;
            }
            m_seen.erase(oldest);
        }
    }
    return true;
}

bool CLogMonitor::process_pending(int64_t now_ms)
{
    if (!m_pending.exchange(false)) return false;
    // 依赖未注入：不消费也不登记签名。直推形态没有 runner，只有 sender
    if (!m_runner && !m_sender) return false;

    const std::string snapshot = snapshot_context();

    // 快照内达阈值的行逐条认领：全部在 TTL 内则本轮跳过；
    // 至少一个新签名才开分析——一轮带出快照里的所有新问题
    std::string trigger_line;
    int trigger_level = -1;
    int fresh_count   = 0;
    size_t start      = 0;
    while (start < snapshot.size()) {
        size_t eol = snapshot.find('\n', start);
        if (eol == std::string::npos) eol = snapshot.size();
        std::string line = snapshot.substr(start, eol - start);
        start            = eol + 1;

        int level = log_level_from_line(line);
        if (level < 0 || level < m_config.level) continue;
        if (!should_trigger(log_signature(static_cast<LogLevel>(level), line),
                            now_ms)) {
            continue;
        }
        ++fresh_count;
        if (trigger_line.empty()) {
            trigger_line  = line;
            trigger_level = level;
        }
    }
    if (trigger_line.empty()) return false;

    // 直推模式：没有分析通道，快照本身就是告警内容——不写上下文附件、
    // 不落 plan，触发去重已在上面完成
    if (!m_runner) {
        push_snapshot(snapshot, trigger_line, trigger_level);
        return true;
    }

    return analyze_snapshot(snapshot, trigger_line, trigger_level, fresh_count);
}

bool CLogMonitor::analyze_snapshot(const std::string& snapshot,
                                   const std::string& trigger_line, int level,
                                   int fresh_count)
{
    const std::string context_path = write_context_file(snapshot);
    if (context_path.empty()) {
        LOG_INFO("log_monitor: context file write failed");
        return false;
    }

    // 分析开始的可观测点：「生成中」的那段时间（到 plan saved 为止）
    LOG_INFO("log_monitor: analyzing %d new signature(s), context=%s",
             fresh_count, context_path.c_str());

    const auto begin = std::chrono::steady_clock::now();
    std::string plan;
    if (!m_runner->analyze(build_analysis_prompt(m_config), context_path,
                           plan)) {
        // 失败原因由 runner 记 INFO；签名不回收——分析通道坏掉时，
        // 同一问题不该被反复拉起，故障本身以 INFO 留痕
        return false;
    }
    const long long elapsed_ms
        = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - begin)
              .count();

    const std::string plan_path = save_plan(plan, level);
    if (plan_path.empty()) {
        LOG_INFO("log_monitor: plan save failed");
        return false;
    }
    LOG_INFO("log_monitor: plan saved to %s (%lld ms)", plan_path.c_str(),
             elapsed_ms);

    if (m_sender) {
        push_message(
            build_feishu_message(plan, plan_path, trigger_line, level));
    }
    return true;
}

void CLogMonitor::push_snapshot(const std::string& snapshot,
                                const std::string& trigger_line, int level)
{
    // 直推形态下 process_pending 已保证 sender 在场
    LOG_INFO("log_monitor: direct push, snapshot without analysis");
    push_message(build_feishu_message(snapshot, "", trigger_line, level));
}

void CLogMonitor::push_message(const std::string& msg)
{
    if (!m_sender->send(msg)) {
        LOG_INFO("log_monitor: feishu push failed");
    }
    else {
        LOG_INFO("log_monitor: feishu push ok");
    }
}

std::string CLogMonitor::write_context_file(const std::string& snapshot)
{
    // 运行期产物出生即绝对：交给子进程的路径无须读者反推服务器工作目录
    const std::string log_dir = absolute_path(logger_get_path());
    CreateDirectoryA(log_dir.c_str(), NULL);

    // 单实例轮转：同一时刻至多一轮分析，固定文件名即可
    const std::string path = log_dir + "/plan-context.txt";
    FILE* fp = fopen(path.c_str(), "wb");  // 内容已是 UTF-8，不写 BOM
    if (!fp) return "";
    fwrite(snapshot.data(), 1, snapshot.size(), fp);
    fclose(fp);
    return path;
}

std::string CLogMonitor::save_plan(const std::string& plan, int level)
{
    // 运行期产物出生即绝对：飞书与日志里报告的路径可直接打开，
    // 不依赖读者的工作目录
    const std::string log_dir = absolute_path(logger_get_path());
    CreateDirectoryA(log_dir.c_str(), NULL);
    const std::string plan_dir = log_dir + "/plans";
    CreateDirectoryA(plan_dir.c_str(), NULL);

    time_t t         = time(NULL);
    struct tm* local = localtime(&t);
    char base[64];
    // 月-日-时分秒-级别：月日不补零，与日志文件命名一致
    snprintf(base, sizeof(base), "%d-%d-%02d%02d%02d-%s", local->tm_mon + 1,
             local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec,
             level_lower_name(level));

    // "wbx" 独占创建：同秒内多份 plan 追加序号，不覆盖
    std::string path;
    FILE* fp = NULL;
    for (int n = 1; n <= 100 && !fp; n++) {
        char suffix[16] = "";
        if (n > 1) snprintf(suffix, sizeof(suffix), "-%d", n);
        path = plan_dir + "/" + base + suffix + ".md";
        fp   = fopen(path.c_str(), "wbx");
    }
    if (!fp) return "";
    fwrite(plan.data(), 1, plan.size(), fp);
    fclose(fp);
    return path;
}

void CLogMonitor::thread_main()
{
    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_stop || m_pending; });
        if (m_stop) break;

        // 合流窗口：并发同时落盘的日志聚齐后一并快照，一次分析带走一批
        if (m_config.flush_ms > 0) {
            m_cv.wait_for(lock, std::chrono::milliseconds(m_config.flush_ms),
                          [this] { return m_stop.load(); });
            if (m_stop) break;
        }
        lock.unlock();

        const int64_t now
            = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count();
        process_pending(now);
    }
}

// ---------------------------------------------------------------------
// 全局实例与入口
// ---------------------------------------------------------------------

CLogMonitor& log_monitor()
{
    static CLogMonitor monitor;
    return monitor;
}

void log_monitor_on_line(LogLevel level, const char* line)
{
    if (!line) return;
    CLogMonitor& monitor = log_monitor();
    if (!monitor.enabled()) return;

    std::string text(line);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    monitor.on_line(level, text);
}

void log_monitor_start()
{
    LogMonitorConfig config = load_log_monitor_config();
    // 配置允许相对路径（工程整体可挪动）：装配时按服务器工作目录定准，
    // 此后子进程的可执行文件与工作目录都与 cwd 脱钩
    config.cli_path = absolute_path(config.cli_path);
    config.work_dir = absolute_path(config.work_dir);

    CLogMonitor& monitor = log_monitor();
    // 配置始终写定：cli_path 与 feishu_webhook 均为空即整体停用
    // （挂钩随之成为空操作）
    monitor.configure(config);
    if (config.cli_path.empty() && config.feishu_webhook.empty()) return;

    std::unique_ptr<IFeishuSender> sender;
    if (!config.feishu_webhook.empty()) {
        sender = std::make_unique<CFeishuClient>(
            std::make_unique<CWinHttpTransport>(), config.feishu_webhook);
    }
    // 分析通道只在配了 cli_path 时装配：只配 webhook 的直推形态没有 runner
    std::unique_ptr<IOpencodeRunner> runner;
    if (!config.cli_path.empty()) {
        runner = std::make_unique<COpencodeRunner>(
            std::make_unique<CWinProcessRunner>(), config.cli_path,
            config.work_dir, config.model, config.timeout_sec);
    }

    monitor.set_dependencies(std::move(sender), std::move(runner));
    monitor.start();

    // 启动横幅：模式（分析 / 直推）、参数与 plan 目录一眼可读
    const bool direct          = config.cli_path.empty();
    const std::string plan_dir = absolute_path(logger_get_path()) + "/plans";
    LOG_INFO(
        "log_monitor: enabled (mode=%s, cli=%s, work_dir=%s, model=%s, "
        "level=%s, cooldown=%ds, timeout=%ds, plan_dir=%s)",
        direct ? "direct" : "analyze",
        direct ? "(none)" : config.cli_path.c_str(),
        config.work_dir.empty() ? "(inherit)" : config.work_dir.c_str(),
        config.model.empty() ? "(default)" : config.model.c_str(),
        level_lower_name(config.level), config.cooldown_sec, config.timeout_sec,
        plan_dir.c_str());
}

void log_monitor_stop() { log_monitor().stop(); }
