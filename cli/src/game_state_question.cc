#include <algorithm>
#include <iostream>
#include <sstream>

#include "color.h"
#include "game_state.h"

// 27: QUESTION — NPC menu / dialog system.
// Three variants of C=27:
//   Menu item:   N=<npc_id>&s=<text>&c=<cmd>&[b=1&]  (b=1 = beginning of menu)
//   Menu end:    N=<npc_id>&e=1&
//   NPC dialog:  N=<npc_id>&n=<name>&m=<message>&

// Check if a command string has unresolved placeholders ($2, $3, etc.)
static std::vector<std::string> find_unresolved_placeholders(
    const std::string &cmd)
{
    std::vector<std::string> result;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (cmd[i] == '$' && i + 1 < cmd.size() && isdigit(cmd[i + 1])) {
            // Found $ followed by digit - check if it's $2, $3, etc. (not $1
            // which is already replaced)
            char digit = cmd[i + 1];
            if (digit >= '2' && digit <= '9') {
                std::string placeholder = cmd.substr(i, 2);
                if (std::find(result.begin(), result.end(), placeholder)
                    == result.end()) {
                    result.push_back(placeholder);
                }
            }
        }
    }
    return result;
}

// Replace unresolved placeholders with user-provided values
static std::string replace_placeholders(
    std::string cmd, const std::vector<std::string> &placeholders,
    const std::vector<std::string> &values)
{
    for (size_t i = 0; i < placeholders.size() && i < values.size(); ++i) {
        size_t pos = 0;
        while ((pos = cmd.find(placeholders[i], pos)) != std::string::npos) {
            cmd.replace(pos, placeholders[i].size(), values[i]);
            pos += values[i].size();
        }
    }
    return cmd;
}

static std::string resolve_menu_cmd(const std::string &raw_cmd, int64_t npc_id)
{
    // Strip $ format prefix and replace $1 with NPC ID
    std::string result;
    size_t pos = 0;
    if (raw_cmd.size() >= 2 && raw_cmd[0] == '$') {
        pos = 2;  // skip $0, $1, $2, $9
    }
    result = raw_cmd.substr(pos);

    // Replace $1 with NPC object ID
    std::string npc_str = std::to_string(npc_id);
    size_t n            = 0;
    while ((n = result.find("$1", n)) != std::string::npos) {
        result.replace(n, 2, npc_str);
        n += npc_str.size();
    }

    return result;
}

std::vector<std::string> GameState::handle_question(const GameMessage &msg)
{
    std::vector<std::string> out;
    ObjectID npc_id = msg.get_int("N");

    // DBG("QUESTION: " << msg.to_string());  // Debug output

    // NPC dialog variant: has n= (name) and m= (message)
    if (msg.has("n") && msg.has("m")) {
        menu_state_.in_dialog       = true;
        menu_state_.dialog_npc_name = msg.get("n");
        menu_state_.dialog_text     = msg.get("m");
        out.push_back(
            color::convert("$CYN[" + menu_state_.dialog_npc_name + " 说]$COM"));
        out.push_back(color::convert("  " + menu_state_.dialog_text));
        return out;
    }

    // Menu end marker: e=1
    if (msg.has("e")) {
        menu_state_.in_menu  = true;
        menu_state_.building = false;
        // Reset any pending input state
        menu_state_.waiting_for_input = false;
        menu_state_.pending_cmd.clear();
        menu_state_.pending_item_text.clear();
        menu_state_.pending_placeholders.clear();

        if (menu_state_.items.empty()) {
            // Empty menu — auto-cancel without displaying
            menu_state_.in_menu = false;
            return out;
        }

        out.push_back(color::convert("$YEL========= 菜单 =========$COM"));
        for (size_t i = 0; i < menu_state_.items.size(); ++i) {
            out.push_back("  [" + std::to_string(i + 1) + "] "
                          + color::convert(menu_state_.items[i].text));
        }
        out.push_back(color::convert("$YEL==========================$COM"));
        out.push_back("输入数字选择菜单项，或输入 0 取消菜单。");
        return out;
    }

    // Menu item: has s= (text) and c= (cmd)
    if (msg.has("s") && msg.has("c")) {
        MenuItem item;
        item.text    = msg.get("s");
        item.raw_cmd = msg.get("c");
        if (item.text.empty() || item.raw_cmd.empty()) {
        }
        else {
            menu_state_.items.push_back(std::move(item));
        }
        menu_state_.npc_id   = npc_id;
        menu_state_.building = true;
        // Don't output anything yet — wait for e=1
        return out;
    }

    // Unknown C=27 variant — just show raw
    out.push_back(color::convert("$YEL[QUESTION]$COM"));
    return out;
}

std::string GameState::select_menu_item(int index)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!menu_state_.in_menu || index < 0
        || static_cast<size_t>(index) >= menu_state_.items.size()) {
        return "";
    }

    const auto &item = menu_state_.items[static_cast<size_t>(index)];
    std::string cmd  = resolve_menu_cmd(item.raw_cmd, menu_state_.npc_id);

    // Check for unresolved placeholders ($2, $3, etc.)
    std::vector<std::string> placeholders = find_unresolved_placeholders(cmd);
    if (!placeholders.empty()) {
        // Need additional user input
        menu_state_.waiting_for_input    = true;
        menu_state_.pending_cmd          = cmd;
        menu_state_.pending_item_text    = item.text;
        menu_state_.pending_placeholders = placeholders;
        menu_state_.in_menu = true;  // Keep menu open while waiting

        // Return empty - the UI should prompt for input
        return "";
    }

    // No unresolved placeholders - close menu and return command
    menu_state_.in_menu  = false;
    menu_state_.building = false;
    menu_state_.items.clear();
    menu_state_.npc_id            = 0;
    menu_state_.waiting_for_input = false;

    return cmd;
}

// Complete a pending menu command with user input
// Returns the completed command string, or empty if still waiting for more
// input
std::string GameState::complete_pending_menu_command(
    const std::string &user_input)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!menu_state_.waiting_for_input) {
        return "";
    }

    // 去掉首尾空白后再切分
    std::string trimmed = user_input;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";  // 空输入，继续等待
    size_t last = trimmed.find_last_not_of(" \t\r\n");
    trimmed     = trimmed.substr(first, last - first + 1);

    std::vector<std::string> values;

    if (menu_state_.pending_placeholders.size() == 1) {
        // 单占位符：整段输入作为一个参数，支持 "21 21 21 21" 这类多词参数
        values.push_back(trimmed);
    }
    else {
        std::istringstream iss(trimmed);
        std::string token;
        while (iss >> token) {
            values.push_back(token);
        }

        // If not enough values provided, prompt again
        if (values.size() < menu_state_.pending_placeholders.size()) {
            return "";  // Still waiting
        }

        // 多余 token 合并进最后一个参数，避免被丢弃
        if (values.size() > menu_state_.pending_placeholders.size()) {
            std::string joined
                = values[menu_state_.pending_placeholders.size() - 1];
            for (size_t i = menu_state_.pending_placeholders.size();
                 i < values.size(); ++i) {
                joined += " " + values[i];
            }
            values.resize(menu_state_.pending_placeholders.size());
            values.back() = joined;
        }
    }

    // Replace all placeholders
    std::string completed_cmd = replace_placeholders(
        menu_state_.pending_cmd, menu_state_.pending_placeholders, values);

    // Clear pending state and close menu
    menu_state_.waiting_for_input = false;
    menu_state_.pending_cmd.clear();
    menu_state_.pending_item_text.clear();
    menu_state_.pending_placeholders.clear();
    menu_state_.in_menu  = false;
    menu_state_.building = false;
    menu_state_.items.clear();
    menu_state_.npc_id = 0;

    return completed_cmd;
}

// Check if we're waiting for additional input for a menu command
bool GameState::is_waiting_for_menu_input() const
{
    return menu_state_.waiting_for_input;
}

// Get the prompt text for pending menu input
std::string GameState::get_pending_menu_prompt() const
{
    if (!menu_state_.waiting_for_input) {
        return "";
    }
    std::string prompt = "[" + menu_state_.pending_item_text + "] 请输入";
    for (size_t i = 0; i < menu_state_.pending_placeholders.size(); ++i) {
        if (i > 0) prompt += " ";
        prompt += " <参数" + std::to_string(i + 1) + ">";
    }
    prompt += ": ";
    return prompt;
}

void GameState::cancel_menu()
{
    std::lock_guard<std::mutex> lock(mutex_);

    menu_state_.in_menu  = false;
    menu_state_.building = false;
    menu_state_.items.clear();
    menu_state_.npc_id    = 0;
    menu_state_.in_dialog = false;
    menu_state_.dialog_text.clear();
    menu_state_.dialog_npc_name.clear();
    menu_state_.waiting_for_input = false;
    menu_state_.pending_cmd.clear();
    menu_state_.pending_item_text.clear();
    menu_state_.pending_placeholders.clear();
}
