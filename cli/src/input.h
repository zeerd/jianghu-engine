#pragma once

#include <string>

// Forward declarations
class CommandHandler;
class GameState;
class Connection;
class TerminalUI;

/** Encapsulates all stdin line processing: menus, CLI commands, server
 * commands, chat. */
class InputProcessor {
public:
    InputProcessor(CommandHandler &cmd_handler, GameState &state,
                   Connection &conn, TerminalUI &ui);

    /** Process a single line of user input. Returns false if client should
     * quit. */
    bool process_line(const std::string &input);

    /** True if the user requested to quit (via .quit / .exit). Clear after
     * checking. */
    bool should_quit() { return should_quit_; }
    void clear_quit() { should_quit_ = false; }

    /** Show CLI help text. */
    void show_help_text();

    /** Get the GameState reference. */
    GameState &game_state() { return state_; }
    const GameState &game_state() const { return state_; }

private:
    CommandHandler &cmd_handler_;
    GameState &state_;
    Connection &conn_;
    TerminalUI &ui_;
    bool should_quit_ = false;

    void process_cli_cmd(const std::string &input);
    void process_server_cmd(const std::string &input);
};
