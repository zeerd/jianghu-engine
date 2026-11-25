#pragma once

#include <string>
#include <vector>

#include "room_map.h"

// Forward declarations
class CommandHandler;
class GameState;
class Connection;
class TerminalUI;

/** Encapsulates all stdin line processing: menus, CLI commands, server
 * commands, chat. */
class InputProcessor {
public:
    InputProcessor(CommandHandler& cmd_handler, GameState& state,
                   Connection& conn, TerminalUI& ui,
                   const std::string& map_file = "cli/sample.dat",
                   int map_step_timeout_ms     = 5000);

    /** Process a single line of user input. Returns false if client should
     * quit. */
    bool process_line(const std::string& input);

    /** True if the user requested to quit (via .quit / .exit). Clear after
     * checking. */
    bool should_quit() { return should_quit_; }
    void clear_quit() { should_quit_ = false; }

    /** Show CLI help text. */
    void show_help_text();

    /** Get the GameState reference. */
    GameState& game_state() { return state_; }
    const GameState& game_state() const { return state_; }

    /** 到房判定：服务端当前房间的注册名（I=）与该步目标房的注册名
     *  （地图 Name1）相等即到达。两者同源于 base_name、全服唯一，显示名
     *  （N= / Name2）允许重名、不参与判定。抽成谓词便于单测覆盖。 */
    static bool arrived_at(const std::string& map_key,
                           const std::string& arrival_name);

private:
    CommandHandler& cmd_handler_;
    GameState& state_;
    Connection& conn_;
    TerminalUI& ui_;
    bool should_quit_ = false;

    std::string map_file_;
    int map_step_timeout_ms_;
    RoomMap room_map_;
    bool map_loaded_ = false;

    void process_cli_cmd(const std::string& input);
    void process_server_cmd(const std::string& input);
    void process_go(const std::string& args);

    // 等待服务端把房间切到 arrival_name；超时或**中途被动进战斗**返回 false
    // （战斗精灵容器无出口，本跳 /go 已被吞，干等满超时没有意义——立即返回
    // 让 .go 走「逃跑→恢复→续走」）。map_step_timeout_ms_ <= 0 时视为立即到达。
    bool wait_for_room(const std::string& arrival_name) const;

    // 主动逃跑直到脱离战斗（.go 寻路恢复用）。flee 在 busy（「你没空逃跑！」）
    // 或 AP<50 时被引擎拒收，故轮询重试；脱战信号 = GOTOROOM 的 F= 翻 0
    // （GameState::in_battle 变假）。15s 内未脱战返回 false。
    bool flee_battle();
};
