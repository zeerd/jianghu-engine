#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "commands.h"
#include "config.h"
#include "connection.h"
#include "game_state.h"
#include "input.h"
#include "terminal_ui.h"

/// 共享的客户端运行时：拥有连接、游戏状态、终端 UI、命令处理，以及后台的
/// 心跳与网络线程。交互式 Application 与无头 ScenarioRunner 都基于它，
/// 从而两条路径复用完全相同的收发与解析逻辑。
class ClientCore {
public:
    /// 正常构造：内部创建 Connection。
    explicit ClientCore(const Config& cfg);

    /// 测试/扩展构造：注入外部 Connection（所有权转移）。
    /// 若提供，ClientCore 负责其生命周期；不提供则内部创建。
    ClientCore(const Config& cfg, std::unique_ptr<Connection> conn);

    ~ClientCore();

    /// 连接、自动登录（仅当配置里有账号+密码）、启动后台线程。
    /// 连接失败时抛出 std::runtime_error。
    void start();

    /// 停止后台线程并断开连接（幂等）。
    void stop();

    /// 处理一次收到的服务器消息。可从任意线程调用。
    void process_received();

    Connection& conn() { return *conn_; }
    GameState& state() { return state_; }
    CommandHandler& cmd_handler() { return cmd_handler_; }
    InputProcessor& input() { return input_processor_; }
    TerminalUI& ui() { return ui_; }
    const Config& cfg() const { return cfg_; }

    bool running() const { return running_.load(); }
    void request_stop() { running_ = false; }
    bool server_closed() const { return server_closed_.load(); }

    /// 文本匹配与输出缓冲的窗口行数。
    ///
    /// 触发式服务链里，巫师道要**囤住全部未处理请求**：21 道 × ~55 条
    /// ≈ 1200 条 tell，外加自己跑命令换回来的回执，峰值约 3000 行。窗口
    /// 取 2000 时会从头部截断、把最老的未消费请求悄悄丢掉（剧本侧表现为
    /// 「某道的请求永远等不到」），故抬到 8192 留足余量。
    static constexpr size_t kOutputWindow = 8192;

    /// 最近 max_lines 行的输出快照（按时间先后排列，最早在前）。
    ///
    /// 起点是**消费游标**（`advance_output_cursor` 之前的历史行不参与匹配）；
    /// 未消费行多于 max_lines 时才从头部截断。first_abs 非空时回传首行的
    /// 绝对序号——`consume` 靠它把游标推进到命中行之后。
    std::vector<std::string> recent_output(size_t max_lines  = 400,
                                           size_t* first_abs = nullptr) const;

    /// 清空输出快照（脚本在关键步骤前可调用，避免匹配到陈旧文本）。
    /// 连同消费游标一起归零到当前末尾：清空后所有行都算「未看过」。
    void clear_output();

    /// 输出缓冲的绝对行号上界（累计入队行数，只增不减）。
    size_t output_seq() const;

    /// 消费游标：该绝对行号之前的行不再参与 recent_output 与文本匹配。
    size_t output_cursor() const;

    /// 把消费游标推进到 to_abs（不含），只增不减；已消费的行随即释放。
    void advance_output_cursor(size_t to_abs);

    /// 取绝对行号 abs 的行文本（该行必须仍在缓冲内且未被消费）。
    bool output_line_at(size_t abs, std::string& out) const;

    /// 启动心跳保活线程（间隔取配置 heartbeat_interval，0=关闭）。
    void start_heartbeat();

    /// 心跳线程体：每 interval_sec 发一次保活，stop() 后立即退出。
    void run_heartbeat_loop(int interval_sec);

    /// 发送一次保活命令（服务端对任何入站命令都会重置发呆计时）。
    void send_heartbeat();

private:
    void init_connection();
    void init_auto_login();
    void start_threads();
    void cleanup_threads();

    Config cfg_;
    std::string host_;
    int port_ = 0;

    std::unique_ptr<Connection> conn_;
    GameState state_;
    TerminalUI ui_;
    CommandHandler cmd_handler_;
    InputProcessor input_processor_;

    std::thread heartbeat_thread_;
    std::mutex hb_mutex_;
    std::condition_variable hb_cv_;
    std::thread net_thread_;
    std::atomic<bool> running_{true};
    std::atomic<bool> server_closed_{false};

    mutable std::mutex output_mutex_;
    std::deque<std::string> output_lines_;
    // 输出缓冲的绝对行号坐标系（三个量都由 output_mutex_ 保护）：
    //   output_base_   output_lines_[0] 的绝对行号（出队时递增）
    //   output_seq_    累计入队行数（绝对行号的上界）
    //   output_cursor_ 消费游标；之前的行不参与文本匹配，且随游标推进释放
    // absolute index = output_base_ + 下标。
    size_t output_base_   = 0;
    size_t output_seq_    = 0;
    size_t output_cursor_ = 0;
};
