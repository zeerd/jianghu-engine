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

    /// 最近 max_lines 行的输出快照（按时间先后排列，最早在前）。
    std::vector<std::string> recent_output(size_t max_lines = 400) const;

    /// 清空输出快照（脚本在关键步骤前可调用，避免匹配到陈旧文本）。
    void clear_output();

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
};
