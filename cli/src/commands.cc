#include "commands.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "color.h"
#include "connection.h"
#include "dbg.h"
#include "game_state.h"
#include "protocol.h"

CommandHandler::CommandHandler(GameState &state, Connection &conn,
                               TerminalUI &ui)
    : state_(state), conn_(conn), ui_(ui)
{
}

bool CommandHandler::process(const std::string &input)
{
    if (input.empty()) return false;

    // Parse: first word is command, rest is args
    std::istringstream iss(input);
    std::string cmd;
    iss >> cmd;

    std::string args;
    std::getline(iss, args);
    // Trim leading space
    if (!args.empty() && args[0] == ' ') args.erase(0, 1);

    // Handle .target delegated from main.cc as __target__<args>
    if (cmd.rfind("__target__", 0) == 0) {
        std::string target_args = cmd.substr(10);
        if (!args.empty()) {
            if (!target_args.empty()) target_args += " ";
            target_args += args;
        }
        if (target_args.empty()) {
            ui_.add_line(color::convert("$YELCurrent target: "
                                        + std::to_string(state_.target_id())
                                        + "$COM"));
            ui_.add_line(
                "Usage: /t <id>  — player: numeric ID; NPC/item: English id");
            ui_.add_line("       /t 0 or /t clear             — clear target");
        }
        else {
            if (target_args == "0" || target_args == "clear") {
                state_.set_target_id(0);
                ui_.add_line(color::convert("$GRNTarget cleared.$COM"));
            }
            else {
                ObjectID tid;
                if (target_args == state_.account_id()) {
                    tid = state_.player_id();
                }
                else {
                    tid = state_.find_target(target_args);
                }
                if (tid != 0) {
                    state_.set_target_id(tid);
                    ui_.add_line(color::convert(
                        "$GRNTarget set to " + std::to_string(tid) + "$COM"));
                }
                else {
                    ui_.add_line(color::convert(
                        "$REDTarget not found: " + target_args + "$COM"));
                }
            }
        }
        return true;
    }

    // Handle .tune delegated from InputProcessor as __tune__<args>
    if (cmd.rfind("__tune__", 0) == 0) {
        std::string tune_args = cmd.substr(8);
        if (!args.empty()) {
            if (!tune_args.empty()) tune_args += " ";
            tune_args += args;
        }
        return process_tune(tune_args);
    }

    // Handle /give delegated from main.cc as __give__<args>
    if (cmd.rfind("__give__", 0) == 0) {
        std::string give_args = cmd.substr(8);
        if (!args.empty()) {
            if (!give_args.empty()) give_args += " ";
            give_args += args;
        }
        return process_give(give_args);
    }

    // Handle /sell delegated from main.cc as __sell__<args>
    if (cmd.rfind("__sell__", 0) == 0) {
        std::string sell_args = cmd.substr(8);
        if (!args.empty()) {
            if (!sell_args.empty()) sell_args += " ";
            sell_args += args;
        }
        return process_sell(sell_args);
    }

    // /get <item> from <container> — 从容器（箱子/尸体）里取物。
    // 服务端语义：get <item_id> <container_id>（target=物品，arg=容器）。
    if (cmd == "get") {
        size_t pos = args.find(" from ");
        if (pos != std::string::npos) {
            auto trim = [](const std::string &s) {
                size_t a = s.find_first_not_of(" \t");
                if (a == std::string::npos) return std::string();
                size_t b = s.find_last_not_of(" \t");
                return s.substr(a, b - a + 1);
            };
            std::string item_name      = trim(args.substr(0, pos));
            std::string container_name = trim(args.substr(pos + 6));

            ObjectID item_id = item_name.empty()
                                   ? 0
                                   : state_.find_target(item_name);
            ObjectID box_id  = container_name.empty()
                                   ? 0
                                   : state_.find_target(container_name);

            if (item_id == 0) {
                ui_.add_line(color::convert(
                    "$REDItem not found: " + item_name + "$COM"));
                return true;
            }
            if (box_id == 0) {
                ui_.add_line(color::convert(
                    "$REDContainer not found: " + container_name + "$COM"));
                return true;
            }

            // 服务端语义：get <item_id> from <container_id>
            // （analyse_string 取「首个空格之后」为 arg，故 from 不可省）
            std::string wire = std::to_string(state_.player_id()) + " get "
                               + std::to_string(item_id) + " from "
                               + std::to_string(box_id) + "\t";
            ui_.add_line("Cmd: " + wire + "\n");
            return conn_.send_command_utf8(wire);
        }
    }

    // All commands sent to server (UTF-8 → GBK)
    // Protocol matches xclient's SendSrvCmd: prepend player object ID so the
    // wire format is "<player_id> <command> <args>\t" (XOR-encoded). Also
    // insert current target as first arg when target is active (xclient
    // behavior).
    DBG("Sending command: " << cmd << " with args: [" << args << "]");

    std::vector<std::string> parsed_args = parse_args(args);

    // Wire format: "<player_id> <command> <target> <args>\t"
    // protocol::command() always expects a target field between cmd and text.
    // xclient always inserts m_Target in SendInputStr, and the server requires
    // it.
    if (state_.target_id() != 0) {
        parsed_args.insert(parsed_args.begin(),
                           std::to_string(state_.target_id()));
    }
    else if (parsed_args.empty()) {
        parsed_args.push_back("0");  // empty target when no args
    }
    else {
        parsed_args.insert(parsed_args.begin(), "0");  // explicit null-target
    }

    std::string full_cmd = cmd;
    if (state_.is_logged_in() && state_.player_id() != 0) {
        full_cmd = std::to_string(state_.player_id()) + " " + cmd;
    }
    full_cmd = protocol::command(full_cmd, parsed_args);
    ui_.add_line("Cmd: " + full_cmd + "\n");
    return conn_.send_command_utf8(full_cmd);
}

std::vector<std::string> CommandHandler::parse_args(const std::string &args_str)
{
    std::vector<std::string> result;
    std::istringstream iss(args_str);
    std::string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

bool CommandHandler::process_tune(const std::string &args_text)
{
    if (args_text.empty()) {
        // Show current channel info
        ui_.add_line(color::convert("$HIG=== Channel ===$COM"));
        ui_.add_line("Current channel: " + state_.current_channel());
        std::ostringstream iss;
        iss << "Available channels: ";
        const auto &chs = state_.channels();
        for (size_t i = 0; i < chs.size(); ++i) {
            if (i > 0) iss << ", ";
            iss << chs[i];
        }
        ui_.add_line(iss.str());
        ui_.add_line("Usage: .tune <channel_name> — switch to a channel");
        return true;
    }

    // 切频道是纯客户端行为：后续纯文本以前缀动词直发（如 "new 0 <msg>"），
    // 故本地切换不下发任何 wire。服务端 tune 是「关闭接收频道」的另一语义，
    // 按前缀约定（`.`=本地、`/`=服务器）经 /tune 直发、由服务端回执确认。
    std::string ch_name = args_text;
    state_.set_current_channel(ch_name);
    ui_.add_line(
        color::convert("$GRNChannel switched to: " + ch_name + "$COM"));
    return true;
}

// /give <item_type> [amount]
// Target is set beforehand via /t command (using state_.target_id()).
// xclient wire format: give <obj_id> to <target_id>
bool CommandHandler::process_give(const std::string &args_text)
{
    if (args_text.empty()) {
        ui_.add_line(color::convert("$HIG=== Give ===$COM"));
        ui_.add_line(
            "Usage: /give <item_type> [amount]  — choose via /t first");
        ui_.add_line(
            "  Example: /t npc_id                   — select target NPC/PC");
        ui_.add_line(
            "           /give gold                 — give 1 coin to target");
        ui_.add_line(
            "           /give sword                — give 1 sword to target");
        ui_.add_line(
            "           /give coin 3               — give 3 coins to target");
        return true;
    }

    if (!state_.is_logged_in()) {
        ui_.add_line(color::convert("$REDNot logged in.$COM"));
        return true;
    }

    ObjectID tid = state_.target_id();
    if (tid == 0) {
        ui_.add_line(
            color::convert("$REDNo target set. Use /t <id> to select a "
                           "target first.$COM"));
        return true;
    }

    // First token is item_type, second (optional) is amount
    std::istringstream iss(args_text);
    std::string item_type;
    iss >> item_type;
    if (item_type.empty()) {
        ui_.add_line(color::convert("$REDEmpty item type.$COM"));
        return true;
    }

    // Parse optional amount
    int64_t amount = -1;
    std::string amt_token;
    if (iss >> amt_token) {
        try {
            amount = std::stoll(amt_token);
        } catch (...) {
            item_type = amt_token;
        }
    }

    // Find item in inventory by internal_id or display name
    ObjectID obj_id = 0;
    int qty_in_inv  = 1;
    for (const auto &item : state_.inventory()) {
        if (item.internal_id == item_type || item.name == item_type) {
            obj_id     = item.id;
            qty_in_inv = item.qty > 0 ? item.qty : 1;
            break;
        }
    }

    if (obj_id == 0) {
        ui_.add_line(color::convert("$REDItem '" + item_type
                                    + "' not found in inventory.$COM"));
        return true;
    }

    // Build wire format: "give <obj_id> to <tid>" or "give <obj_id> to <tid> N"
    // (stacks)
    std::ostringstream cmd_ss;
    cmd_ss << state_.player_id() << " give " << obj_id << " to " << tid;

    if (amount > 0) {
        // Explicit amount provided
        cmd_ss << " " << amount;
    }
    else if (qty_in_inv > 1 && item_type != "coin") {
        // Stackable item: xclient calls RequireAmount; default to 1 here
        cmd_ss << " 1";
    }
    cmd_ss << "\t";

    DBG("Give command: " << cmd_ss.str());
    ui_.add_line(color::convert("$CYNGiving item " + item_type + " to target "
                                + std::to_string(tid) + "$COM"));
    return conn_.send_command_utf8(cmd_ss.str());
}

bool CommandHandler::process_sell(const std::string &args_text)
{
    const auto &inv = state_.inventory();
    for (const auto &item : inv) {
        if (item.internal_id == args_text || item.name == args_text) {
            std::ostringstream cmd_ss;
            ObjectID vendor_id = state_.target_id();
            if (vendor_id == 0) {
                ui_.add_line(color::convert(
                    "$REDNo vendor target set. Use /t <vendor_id> to "
                    "select a vendor.$COM"));
                return true;
            }
            cmd_ss << state_.player_id() << " sell " << vendor_id << " "
                   << item.id << "\t";
            DBG("Sell command: " << cmd_ss.str());
            ui_.add_line(
                color::convert("$CYNSelling item " + item.name + "$COM"));
            return conn_.send_command_utf8(cmd_ss.str());
        }
    }
    return false;
}
