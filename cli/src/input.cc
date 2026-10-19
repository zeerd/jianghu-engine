#include "input.h"

#include <sstream>

#include "color.h"
#include "commands.h"
#include "connection.h"
#include "game_state.h"
#include "terminal_ui.h"

// Display all player info panels (used by .info CLI command).
static void show_player_info(const GameState &gs, TerminalUI &ui)
{
    auto panels = gs.player_info_panels();
    for (const auto &line : panels) {
        if (!line.empty()) {
            ui.add_line(color::convert(line));
        }
    }
}

// Display skills info (used by .skills CLI command).
static void show_skills(const GameState &gs, TerminalUI &ui)
{
    const auto &skills = gs.skills();
    ui.add_line(color::convert("$HIG=== 技能列表 ===$COM"));
    if (skills.empty()) {
        ui.add_line("  (无技能)");
        return;
    }
    for (const auto &s : skills) {
        if (s.level > 0 || s.potential > 0) {
            std::string line = "  " + s.skill_name;
            if (!s.base_skill.empty() && s.base_skill != s.skill_name) {
                line += " (" + s.base_skill + ")";
            }
            if (s.level > 0) {
                line += " Lv." + std::to_string(s.level);
            }
            if (s.potential > 0) {
                line += " 潜能:" + std::to_string(s.potential);
            }
            if (s.prepared) {
                line += " [准备]";
            }
            if (!s.mapped_skill.empty()) {
                line += " -> " + s.mapped_skill;
            }
            ui.add_line(line);

            // Display magics/绝招 for this skill
            if (!s.magics.empty()) {
                for (const auto &magic : s.magics) {
                    std::ostringstream magic_ss;
                    magic_ss << "    |_ " << magic.display_name << "("
                             << magic.name << ")";
                    if (magic.force > 0 || magic.mp > 0) {
                        magic_ss << " [内力";
                        if (magic.force > 0) magic_ss << magic.force;
                        if (magic.mp > 0) magic_ss << "/" << magic.mp;
                        magic_ss << "]";
                    }
                    if (magic.valid_cast > 0) {
                        magic_ss << " [施展:" << magic.valid_cast << "]";
                    }
                    if (magic.target > 0) {
                        magic_ss << " [目标:" << magic.target << "]";
                    }
                    ui.add_line(color::convert(magic_ss.str()));
                }
            }
        }
    }
}

static void show_player_info_panel(const GameState &gs,
                                   const std::string &panel_name,
                                   TerminalUI &ui)
{
    auto panels = gs.player_info_panel(panel_name);
    for (const auto &line : panels) {
        if (!line.empty()) {
            ui.add_line(color::convert(line));
        }
    }
}

void InputProcessor::show_help_text()
{
    ui_.add_line(color::convert("$HIG=== Commands ===$COM"));
    ui_.add_line("Server commands start with /, e.g. /look /tell 你好");
    ui_.add_line("Type plain text to chat (sent as say): hello world");
    ui_.add_line(
        "Channel: .tune show current or switch channels (chat/new/...)");
    ui_.add_line(
        "Standard: /get /drop /wield /unwield /use /go /give /look /say /tell");
    ui_.add_line(
        "  /get <item> from <container> — 从箱子/尸体取物（便捷式）");
    ui_.add_line("  /kill /fight /halt /flee /join /apprentice /learn");
    ui_.add_line("  /teach /sleep /exercise /enable /prepare /study /practice");
    ui_.add_line("  /abandon /enforce /cast");
    ui_.add_line("User: /save /talk /info /item");
    ui_.add_line("  /who /buy /list");
    ui_.add_line(color::convert("$HIG=== CLI Commands ===$COM"));
    ui_.add_line(".help .quit .exit .status .info .skills");
    ui_.add_line("  .look .objects .chars .doors .items");
    ui_.add_line(".info    — detailed player status panel");
    ui_.add_line(".skills  — list all skills with levels and potential");
    ui_.add_line(
        ".look    — show all characters, items and exits in the scene");
    ui_.add_line(".objects — list NPCs, players and items in the room");
    ui_.add_line(".chars   — list all characters (NPCs + players)");
    ui_.add_line(".doors    — 查看所处区域及明显的出口信息。");
    ui_.add_line(".items    — list items on the ground");
    ui_.add_line("Target:   /t <id|name> — set, /t — show, /t 0 — clear");
    ui_.add_line("Channels: .tune — show current, .tune <name> to switch");
    ui_.add_line("Give:     /give <item_name> [amount] — give item to target");
    ui_.add_line(
        "Vendor:   /list [0-N] — list shop items, /buy <index> buy item, /sell "
        "<name|id> - sell item to vendor");
    ui_.add_line(
        "Banker:   /balance — 查看存款, /deposit — 存款, /withdraw — 取款, "
        "/loan — 贷款");
}

InputProcessor::InputProcessor(CommandHandler &cmd_handler, GameState &state,
                               Connection &conn, TerminalUI &ui)
    : cmd_handler_(cmd_handler), state_(state), conn_(conn), ui_(ui)
{
}

void InputProcessor::process_cli_cmd(const std::string &input)
{
    if (input == ".quit" || input == ".exit") {
        should_quit_ = true;
        ui_.shutdown();
    }
    else if (input == ".help") {
        show_help_text();
    }
    else if (input == ".status") {
        ui_.add_line(color::convert("$HIG" + state_.status_line() + "$COM"));
    }
    else if (input == ".info") {
        show_player_info(state_, ui_);
    }
    else if (input == ".skills") {
        show_skills(state_, ui_);
    }
    else if (input == ".inventory") {
        show_player_info_panel(state_, "inventory", ui_);
    }
    else if (input == ".objects") {
        auto npcs  = state_.room_npcs();
        auto chars = state_.room_chars();
        auto items = state_.room_items();
        ui_.add_line(color::convert("$HIG=== Room Objects ===$COM"));
        if (!npcs.empty()) {
            ui_.add_line(color::convert("$YELNPCs:$COM"));
            for (const auto &o : npcs) {
                std::string line
                    = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                      + "] " + o.name;
                if (o.qty > 1) line += " x" + std::to_string(o.qty);
                if (!o.title.empty()) line += " " + o.title;
                ui_.add_line(line);
            }
        }
        if (!chars.empty()) {
            ui_.add_line(color::convert("$GRNPlayers:$COM"));
            for (const auto &o : chars) {
                std::string line
                    = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                      + "] " + o.name;
                if (!o.title.empty()) line += " " + o.title;
                ui_.add_line(line);
            }
        }
        if (!items.empty()) {
            ui_.add_line(color::convert("$CYNItems:$COM"));
            for (const auto &o : items) {
                std::string line
                    = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                      + "] " + o.name;
                if (o.qty > 1) line += " x" + std::to_string(o.qty);
                ui_.add_line(line);
            }
        }
        if (npcs.empty() && chars.empty() && items.empty()) {
            ui_.add_line("  (******)");
        }
    }
    else if (input == ".chars") {
        auto npcs  = state_.room_npcs();
        auto chars = state_.room_chars();
        ui_.add_line(color::convert("$HIG=== Characters ===$COM"));
        if (npcs.empty() && chars.empty()) {
            ui_.add_line("  (******)");
        }
        else {
            if (!npcs.empty()) {
                ui_.add_line(color::convert("$YELNPCs:$COM"));
                for (const auto &o : npcs) {
                    std::string line
                        = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                          + "] " + o.name;
                    if (o.qty > 1) line += " x" + std::to_string(o.qty);
                    if (!o.title.empty()) line += " " + o.title;
                    ui_.add_line(line);
                }
            }
            if (!chars.empty()) {
                ui_.add_line(color::convert("$GRNPlayers:$COM"));
                for (const auto &o : chars) {
                    std::string line
                        = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                          + "] " + o.name;
                    if (!o.title.empty()) line += " " + o.title;
                    ui_.add_line(line);
                }
            }
        }
    }
    else if (input == ".doors") {
        auto doors = state_.exits();
        ui_.add_line(color::convert("$HIG场景：" + state_.room_name() + " ("
                                    + state_.room_map() + ")$COM"));
        ui_.add_line(color::convert("$HIG明显的出口是：$COM"));
        if (doors.empty()) {
            ui_.add_line("  无");
        }
        else {
            for (const auto &d : doors) {
                std::string line = "  " + d.dir_label + " -> " + d.room_name;
                // if (!d.door_key.empty()) line += " (" + d.door_key + ")";
                ui_.add_line(line);
            }
        }
    }
    else if (input == ".look") {
        // Show a comprehensive view of the current scene
        auto npcs  = state_.room_npcs();
        auto chars = state_.room_chars();
        auto items = state_.room_items();
        auto doors = state_.exits();

        ui_.add_line(
            color::convert("$HIG=== " + state_.room_name() + " ===$COM"));

        // Characters (NPCs + players)
        if (!npcs.empty() || !chars.empty()) {
            ui_.add_line(color::convert("$HIG人物：$COM"));
            for (const auto &o : npcs) {
                std::string line = "  " + o.name;
                if (!o.title.empty()) line += " " + o.title;
                ui_.add_line(line);
            }
            for (const auto &o : chars) {
                std::string line = "  " + o.name;
                if (!o.title.empty()) line += " " + o.title;
                ui_.add_line(line);
            }
        }
        else {
            ui_.add_line(color::convert("$HIG人物：$COM"));
            ui_.add_line("  (无)");
        }

        // Items on ground
        if (!items.empty()) {
            ui_.add_line(color::convert("$HIG物品：$COM"));
            for (const auto &o : items) {
                std::string line = "  " + o.name;
                if (o.qty > 1) line += " x" + std::to_string(o.qty);
                ui_.add_line(line);
            }
        }

        // Exits
        if (!doors.empty()) {
            ui_.add_line(color::convert("$HIG出口：$COM"));
            for (const auto &d : doors) {
                std::string line = "  " + d.dir_label + " -> " + d.room_name;
                ui_.add_line(line);
            }
        }
    }
    else if (input == ".items") {
        auto items = state_.room_items();
        ui_.add_line(color::convert("$HIG=== Items on Ground ===$COM"));
        if (items.empty()) {
            ui_.add_line("  (******)");
        }
        else {
            for (const auto &o : items) {
                std::string line
                    = "  [" + (!o.internal_id.empty() ? o.internal_id : "-")
                      + "] " + o.name;
                if (o.qty > 1) line += " x" + std::to_string(o.qty);
                ui_.add_line(line);
            }
        }
    }
    else if (input == ".tune" || input.rfind(".tune ", 0) == 0) {
        // 本地切频道：纯客户端状态，不下发任何 wire
        std::string tune_args = (input == ".tune") ? "" : input.substr(6);
        cmd_handler_.process("__tune__" + tune_args);
    }
    else {
        ui_.add_line(
            color::convert("$REDUnknown CLI command: " + input + "$COM"));
        ui_.add_line(
            "CLI commands start with '.', Server commands start with '/'.");
    }
}

void InputProcessor::process_server_cmd(const std::string &input)
{
    // Login handler
    if (input.rfind("login", 0) == 0) {
        std::istringstream iss(input);
        std::string cmd, account, password;
        iss >> cmd >> account >> password;
        if (!account.empty() && !password.empty()) {
            conn_.send_login(account, password);
            ui_.add_line("Logging in...");
            state_.set_account_id(account);
        }
        return;
    }

    // Register handler
    if (input.rfind("register", 0) == 0) {
        std::istringstream iss(input);
        std::string cmd, account, password, gender;
        int str = 0, con = 0, intel = 0, dex = 0;
        iss >> cmd >> account >> password >> gender >> str >> con >> intel
            >> dex;
        if (!account.empty()) {
            if (conn_.send_register(account, password, gender, str, con, intel,
                                    dex)) {
                ui_.add_line("Registering...");
            }
        }
        else {
            ui_.add_line(
                color::convert("$REDUsage: /register <account> <password> "
                               "<gender> <str> <con> <int> <dex>$COM"));
        }
        return;
    }

    // /t — target (CLI feature delegated to CommandHandler)
    if (input == "t" || input.rfind("t ", 0) == 0) {
        std::string target_args = (input == "t") ? "" : input.substr(2);
        cmd_handler_.process("__target__" + target_args);
        return;
    }

    // /give — give items to a target (CLI feature delegated to CommandHandler)
    if (input == "give" || input.rfind("give ", 0) == 0) {
        std::string give_args = (input == "give") ? "" : input.substr(5);
        cmd_handler_.process("__give__" + give_args);
        return;
    }

    // /sell — sell item to a vendor (CLI feature delegated to CommandHandler)
    if (input == "sell" || input.rfind("sell ", 0) == 0) {
        std::string sell_args = (input == "sell") ? "" : input.substr(5);
        cmd_handler_.process("__sell__" + sell_args);
        return;
    }

    // All other /-commands go straight to server（含 /tune：服务端 tune
    // = 关闭该频道接收；本地切换频道是 .tune）
    if (!cmd_handler_.process(input)) {
        ui_.add_line(
            color::convert("$REDFailed to send command: " + input + "$COM"));
    }
}

bool InputProcessor::process_line(const std::string &input)
{
    if (input.empty()) return true;

    // --- Menu / Dialog interaction ---
    if (state_.is_waiting_for_menu_input()) {
        // User is providing additional input for a menu command with
        // placeholders
        std::string completed_cmd = state_.complete_pending_menu_command(input);
        if (!completed_cmd.empty()) {
            // Menu commands are already fully resolved with all args.
            // Send directly (prepend player_id) to avoid CommandHandler
            // inserting target_id_ again, which would cause duplicate args.
            if (state_.is_logged_in() && state_.player_id() != 0) {
                std::string wire_cmd = std::to_string(state_.player_id()) + " "
                                       + completed_cmd + "\t";
                ui_.add_line("Cmd: " + wire_cmd + "\n");
                conn_.send_command_utf8(wire_cmd);
            }
            return true;
        }
        else {
            // Still waiting for more input - show prompt again
            ui_.add_line(color::convert(
                "$YEL" + state_.get_pending_menu_prompt() + "$COM"));
            return true;
        }
    }

    if (state_.in_menu() || state_.in_dialog()) {
        bool has_menu_items = state_.in_menu();
        bool consumed       = false;
        try {
            int choice = std::stoi(input);
            if (choice == 0) {
                state_.cancel_menu();
                ui_.add_line(color::convert("$YELInteraction cancelled.$COM"));
                consumed = true;
            }
            else if (has_menu_items) {
                // Only process as menu selection when actually in menu mode
                std::string cmd = state_.select_menu_item(choice - 1);
                if (cmd.empty()) {
                    // Check if waiting for additional input
                    if (state_.is_waiting_for_menu_input()) {
                        ui_.add_line(color::convert(
                            "$YEL" + state_.get_pending_menu_prompt()
                            + "$COM"));
                        consumed = true;
                    }
                    else {
                        ui_.add_line(color::convert("$REDInvalid choice.$COM"));
                        consumed = true;
                    }
                }
                else {
                    // Menu command is already fully resolved (NPC ID
                    // replaced, user args filled in). Send directly to
                    // avoid CommandHandler inserting target_id_ again.
                    if (state_.is_logged_in() && state_.player_id() != 0) {
                        std::string wire_cmd
                            = std::to_string(state_.player_id()) + " " + cmd
                              + "\t";
                        ui_.add_line("Cmd: " + wire_cmd + "\n");
                        conn_.send_command_utf8(wire_cmd);
                    }
                    consumed = true;
                }
            }
            else {
                // In dialog mode (no choices) — cancel and let input go
                // as chat
                state_.cancel_menu();
            }
        } catch (...) {
            // Non-numeric input: cancel menu/dialog and fall through
            // for processing
            state_.cancel_menu();
        }
        if (consumed) return true;
    }

    if (input[0] == '.') {
        process_cli_cmd(input);
        return true;
    }

    if (input[0] == '/') {
        process_server_cmd(input.substr(1));
        return true;
    }

    // Chat: plain text input — prepend current channel from state
    std::string channel_cmd = state_.current_channel() + " " + input;
    if (!cmd_handler_.process(channel_cmd)) {
        ui_.add_line(color::convert("$REDFailed to send message.$COM"));
    }
    return true;
}
