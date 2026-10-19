#pragma once

#include <string>
#include <vector>

#include "terminal_ui.h"

class GameState;
class Connection;

class CommandHandler {
public:
    CommandHandler(GameState &state, Connection &conn, TerminalUI &ui);

    // Execute a command entered by the user. Returns true if command was
    // handled.
    bool process(const std::string &input);

    // Internal helpers
    bool process_give(
        const std::string &args_text);  // target via state_.target_id()
    bool process_sell(const std::string &args_text);

private:
    GameState &state_;
    Connection &conn_;
    TerminalUI &ui_;

    // Internal helpers
    void send(const std::string &msg);
    std::vector<std::string> parse_args(const std::string &args_str);

    bool process_tune(const std::string &args_text);
};