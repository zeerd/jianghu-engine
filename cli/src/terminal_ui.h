#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

class TerminalUI {
public:
    TerminalUI() = default;
    ~TerminalUI();

    // Always succeeds — simple scrolling output
    bool init() { return true; }

    /// Set logs directory. If empty (default), uses  ~/.local/share/jianghu/logs.
    /// Must be called before any add_line() call.
    void set_logs_dir(const std::string &dir);

    // Print a line to stdout and append to daily log file
    void add_line(const std::string &line);

    // 写 stdout 的原始文本（调用方已含换行），不进会话日志。
    void post_raw(const std::string &text);

    // 等待输出队列排空（含最后一条写盘完成）；返回是否在超时前排空。
    bool flush_output();

    // No-op in simple mode
    void set_status(const std::string &) {}

    // Signal shutdown
    void shutdown() { running_ = false; }

    bool is_running() const { return running_; }

private:
    /// Ensure log file is open for today. Rotates if day changed.
    void ensure_log_file();

    /// Format a timestamp string [YYYY-MM-DD HH:MM:SS]
    static std::string timestamp();

    // 一条待写输出：out 是写往 stdout 的完整字节（含换行）；
    // logline 是写往会话日志的整行（含换行），空表示不入日志。
    struct OutItem {
        std::string out;
        std::string logline;
    };

    // 调用方持有 out_mutex_ 时启动写线程（首条输出时惰性启动）。
    void start_writer_locked();
    void writer_loop();

    std::atomic<bool> running_{true};

    std::string logs_dir_;
    std::ofstream log_file_;
    int current_log_day_{0};
    std::mutex log_mutex_;

    std::deque<OutItem> out_queue_;
    std::mutex out_mutex_;
    std::condition_variable out_cv_;
    std::thread out_thread_;
    bool out_stopping_{false};
    // 正在写 stdout 的瞬时标记：flush_output 用它区分「队列空」与「已写完」
    bool out_writing_{false};
};
