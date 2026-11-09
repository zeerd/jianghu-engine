#include "terminal_ui.h"

#include <chrono>
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
    {
        std::lock_guard<std::mutex> lock(out_mutex_);
        out_stopping_ = true;
    }
    out_cv_.notify_all();
    if (out_thread_.joinable()) out_thread_.join();

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

void TerminalUI::start_writer_locked()
{
    if (out_thread_.joinable() || out_stopping_) return;
    out_thread_ = std::thread(&TerminalUI::writer_loop, this);
}

// 输出写线程：stdout 与会话日志都只由本线程写，队列天然保序。
// 输出是旁路——屏幕与日志的快慢不得拖住协议处理与剧本等待：写 stdout
// 在管道/终端变慢时会阻塞，阻塞必须只落在本线程，不落在调用方。
void TerminalUI::writer_loop()
{
    for (;;) {
        OutItem item;
        {
            std::unique_lock<std::mutex> lock(out_mutex_);
            out_cv_.wait(lock,
                         [this] { return out_stopping_ || !out_queue_.empty(); });
            if (out_queue_.empty()) {
                if (out_stopping_) return;
                continue;
            }
            item        = std::move(out_queue_.front());
            out_writing_ = true;
            out_queue_.pop_front();
        }

        // std::endl 换成显式 flush：无头运行的 stdout 是管道（全缓冲），
        // 不逐条冲刷就看不到逐步进度。
        std::cout << item.out << std::flush;

        if (!item.logline.empty()) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            if (logs_dir_.empty()) {
                logs_dir_ = default_logs_dir();
            }
            ensure_log_file();
            if (log_file_.is_open()) {
                log_file_ << item.logline << std::flush;
            }
        }

        // 一条输出（stdout+日志）全部落定后才清写作中标志：
        // flush_output 以「队列空且不在写作中」为完成条件，提前清会让
        // 排空方抢在日志写完之前返回。
        bool drained = false;
        {
            std::lock_guard<std::mutex> lock(out_mutex_);
            out_writing_ = false;
            drained      = out_queue_.empty();
        }
        if (drained) out_cv_.notify_all();
    }
}

void TerminalUI::add_line(const std::string &line)
{
    OutItem item;
    item.out = line + "\033[0m\n";
    // 时间戳在入队时取：写线程排空延迟不会把会话日志时间戳推后。
    const std::string ignore = "要打开某个频道只要用该频道说话即可。";
    if (line != ignore) {
        item.logline = "[" + timestamp() + "] " + line + "\n";
    }

    {
        std::lock_guard<std::mutex> lock(out_mutex_);
        start_writer_locked();
        out_queue_.push_back(std::move(item));
    }
    out_cv_.notify_one();
}

void TerminalUI::post_raw(const std::string &text)
{
    if (text.empty()) return;

    OutItem item;
    item.out = text;
    {
        std::lock_guard<std::mutex> lock(out_mutex_);
        start_writer_locked();
        out_queue_.push_back(std::move(item));
    }
    out_cv_.notify_one();
}

bool TerminalUI::flush_output()
{
    std::unique_lock<std::mutex> lock(out_mutex_);
    return out_cv_.wait_for(lock, std::chrono::seconds(5), [this] {
        return out_queue_.empty() && !out_writing_;
    });
}
