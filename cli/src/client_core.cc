#include "client_core.h"

#include <chrono>
#include <stdexcept>

#include "color.h"
#include "dbg.h"
#include "protocol.h"

ClientCore::ClientCore(const Config& cfg)
    : ClientCore(cfg, std::make_unique<Connection>())
{
}

ClientCore::ClientCore(const Config& cfg, std::unique_ptr<Connection> conn)
    : cfg_(cfg),
      conn_(std::move(conn)),
      state_(*conn_),
      ui_(),
      cmd_handler_(state_, *conn_, ui_),
      input_processor_(cmd_handler_, state_, *conn_, ui_, cfg.map_file,
                       cfg.map_step_timeout_ms)
{
    host_ = cfg.host;
    port_ = cfg.port;
    ui_.set_logs_dir(cfg.logs_dir);
    // 通讯密钥随连接配置下发：收发两侧共用 protocol 的密钥态，
    // 留空即回退内置默认（服务端 comm_key 为空时同为默认）
    protocol::set_comm_key(cfg.comm_key);
}

ClientCore::~ClientCore() { stop(); }

void ClientCore::start()
{
    init_connection();
    init_auto_login();
    start_threads();
}

void ClientCore::init_connection()
{
    ui_.add_line(color::convert("$HIG【系统】正在连接服务器 " + host_ + ":"
                                + std::to_string(port_) + "...$COM"));

    if (!conn_->connect(host_, port_)) {
        ui_.add_line(color::convert("$RED【系统】连接失败！$COM"));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        throw std::runtime_error("Connection failed");
    }
    ui_.add_line(color::convert("$GRN【系统】连接成功.$COM"));
}

void ClientCore::init_auto_login()
{
    if (!cfg_.account.empty() && !cfg_.password.empty()) {
        conn_->send_login(cfg_.account, cfg_.password);
        ui_.add_line("Auto-login as " + cfg_.account + "...");
        state_.set_account_id(cfg_.account);
    }
    else {
        ui_.add_line("Please enter login credentials:");
        ui_.add_line("  Enter /login <account> <password>");
    }
}

void ClientCore::start_threads()
{
    start_heartbeat();

    // 网络线程
    net_thread_ = std::thread([this]() {
        while (running_) {
            process_received();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
}

void ClientCore::start_heartbeat()
{
    if (cfg_.heartbeat_interval <= 0) return;

    int interval_sec  = cfg_.heartbeat_interval;
    heartbeat_thread_ = std::thread(
        [this, interval_sec]() { run_heartbeat_loop(interval_sec); });
}

void ClientCore::run_heartbeat_loop(int interval_sec)
{
    std::unique_lock<std::mutex> lock(hb_mutex_);
    do {
        if (hb_cv_.wait_for(lock, std::chrono::seconds(interval_sec),
                            [this] { return !running_; })) {
            return;
        }
        send_heartbeat();
    } while (running_);
}

void ClientCore::send_heartbeat()
{
    conn_->send_command_utf8(std::to_string(state_.player_id())
                             + " tune 0 reducedrop\t");
}

void ClientCore::process_received()
{
    auto messages = conn_->drain_received();
    for (const auto& raw : messages) {
        auto decoded = protocol::decode_str(raw);
        auto utf8    = protocol::gbk_to_utf8(decoded);
        auto msg     = protocol::parse_message(utf8);
        if (msg) {
            auto lines = state_.handle_message(*msg);
            for (const auto& line : lines) {
                {
                    std::lock_guard<std::mutex> lock(output_mutex_);
                    output_lines_.push_back(line);
                    ++output_seq_;
                    while (output_lines_.size() > kOutputWindow) {
                        output_lines_.pop_front();
                        ++output_base_;
                    }
                }
                ui_.add_line(line);
            }
        }

        // 服务端关闭连接
        if (conn_->is_eof()) {
            server_closed_ = true;
            ui_.add_line(
                color::convert("$REDServer closed the connection.$COM"));
            running_ = false;
        }
    }
}

std::vector<std::string> ClientCore::recent_output(size_t max_lines,
                                                   size_t* first_abs) const
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    // 起点：消费游标之后才算「还没看过」，且不能早于 deque 里现存的首行。
    size_t start = output_base_;
    if (output_cursor_ > start) start = output_cursor_;
    // 未消费行超过窗口才从头部截断（正常情况下窗口足够大，走不到这里）。
    if (output_seq_ - start > max_lines) start = output_seq_ - max_lines;
    if (first_abs) *first_abs = start;
    return std::vector<std::string>(
        output_lines_.begin() + (start - output_base_), output_lines_.end());
}

void ClientCore::clear_output()
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    output_lines_.clear();
    output_base_   = output_seq_;
    output_cursor_ = output_seq_;
}

size_t ClientCore::output_seq() const
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    return output_seq_;
}

size_t ClientCore::output_cursor() const
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    return output_cursor_;
}

void ClientCore::advance_output_cursor(size_t to_abs)
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (to_abs <= output_cursor_) return;
    output_cursor_ = to_abs;
    // 已消费的行没有再被读到的可能，直接释放——否则服务道跑满一轮后
    // 缓冲里会堆着上千行没人看的旧请求。
    while (!output_lines_.empty() && output_base_ < output_cursor_) {
        output_lines_.pop_front();
        ++output_base_;
    }
}

bool ClientCore::output_line_at(size_t abs, std::string& out) const
{
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (abs < output_cursor_ || abs < output_base_ || abs >= output_seq_)
        return false;
    out = output_lines_[abs - output_base_];
    return true;
}

void ClientCore::cleanup_threads()
{
    running_ = false;
    hb_cv_.notify_all();
    conn_->disconnect();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (net_thread_.joinable()) net_thread_.join();
}

void ClientCore::stop() { cleanup_threads(); }
