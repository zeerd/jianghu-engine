#include "terminal_ui.h"

#include <ctime>
#include <iostream>

#include "color.h"
#include "compat.h"

static std::string default_logs_dir()
{
    std::string home = home_dir();
    if (home.empty()) return "./logs";
    return home + "/.jianghu/logs";
}

TerminalUI::~TerminalUI()
{
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void TerminalUI::set_logs_dir(const std::string &dir)
{
    logs_dir_ = dir.empty() ? default_logs_dir() : dir;

    // Create directory if it does not exist
    make_dirs(logs_dir_);
}

std::string TerminalUI::timestamp()
{
    time_t now    = time(nullptr);
    struct tm *tm = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buf);
}

void TerminalUI::ensure_log_file()
{
    time_t now    = time(nullptr);
    struct tm *tm = localtime(&now);
    int today     = tm->tm_yday;  // day of year (0-365)

    if (log_file_.is_open() && today == current_log_day_) {
        return;  // Already on today's file
    }

    // Close old file
    if (log_file_.is_open()) {
        log_file_.close();
    }

    current_log_day_ = today;

    // Build filename: logs_dir/YYYY-MM-DD.log
    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm);
    std::string path = logs_dir_ + "/" + date_buf + ".log";

    log_file_.open(path, std::ios::app);
}

void TerminalUI::add_line(const std::string &line)
{
    std::cout << line << "\033[0m" << std::endl;

    // Write to log file with timestamp
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (logs_dir_.empty()) {
            logs_dir_ = default_logs_dir();
        }
        const std::string ignore = "要打开某个频道只要用该频道说话即可。";
        if (line != ignore) {
            ensure_log_file();
            if (log_file_.is_open()) {
                log_file_ << "[" << timestamp() << "] " << line << std::endl;
            }
        }
    }
}
