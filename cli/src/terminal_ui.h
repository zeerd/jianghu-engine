#pragma once

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>

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

    std::atomic<bool> running_{true};

    std::string logs_dir_;
    std::ofstream log_file_;
    int current_log_day_{0};
    std::mutex log_mutex_;
};