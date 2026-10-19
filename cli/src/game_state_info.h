#pragma once

#include <string>
#include <vector>

class GameState;  // forward declaration

namespace detail {

// Helper: format a section header with color prefix.
std::string make_header(const std::string &text);

// Helper: format a line as "  key: value".
std::string make_line(const std::string &key, const std::string &value);

// Helper: create a progress bar like [===     ] (8 chars).
std::string make_bar(double pct);

}  // namespace detail

// ---- GameState helper methods (declared in game_state_info.cc) ----

// Generate all player info panels as display lines.
std::vector<std::string> player_info_panels(const GameState &gs);

// Generate a specific panel for the given name.
std::vector<std::string> player_info_panel(const GameState &gs,
                                           const std::string &panel_name);
