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
                   const std::string& map_file = "sample.dat",
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

    // 等待服务端把房间切换到 names 中的某一个；超时返回 false。
    // map_step_timeout_ms_ <= 0 时视为立即到达（无需等待）。
    bool wait_for_room(const std::vector<std::string>& names) const;
};
