#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "connection.h"
#include "game_state_question.h"
#include "types.h"

class GameState {
public:
    GameState(Connection& conn) : conn_(conn) {}

    // Parse a GameMessage and update internal state.
    // Returns any display text that should be shown to the user.
    std::vector<std::string> handle_message(const GameMessage& msg);

    // Handle C=27 QUESTION (menu/dialog) messages.
    std::vector<std::string> handle_question(const GameMessage& msg);

    // ---- Accessors ----

    void set_account_id(const std::string& id) { account_id_ = id; }
    const std::string& account_id() const { return account_id_; }
    ObjectID player_id() const { return player_id_; }
    ObjectID current_room() const { return current_room_; }
    ObjectID target_id() const { return target_id_; }
    void set_target_id(ObjectID id) { target_id_ = id; }

    std::string room_name() const;
    std::string room_map() const;
    bool in_battle() const { return room_fight_flag_ != 0; }

    const PlayerAttr& player_attr() const { return player_attr_; }
    PlayerAttr& mutable_player_attr() { return player_attr_; }

    const std::vector<SkillInfo>& skills() const { return skills_; }
    const std::map<ObjectID, std::vector<ObjectInfo>>& room_objects() const
    {
        return room_objects_;
    }
    const std::vector<OnlineUser>& online_users() const
    {
        return online_users_;
    }

    bool is_logged_in() const { return logged_in_; }

    // Target system: note target_id() and set_target_id() are declared above.

    // Exits of the current room
    const std::vector<DoorInfo>& exits() const { return exits_; }

    // Inventory items
    const std::vector<ObjectInfo>& inventory() const { return inventory_; }

    // Filter helpers for room objects
    std::vector<ObjectInfo> room_npcs() const;   // NPCs in current room
    std::vector<ObjectInfo> room_chars() const;  // PCs in current room
    std::vector<ObjectInfo> room_items()
        const;  // Items on ground in current room

    // Lookup: find object ID by name or internal_id (account ID).
    // Searches room objects and online users. Returns 0 if not found.
    ObjectID find_target(const std::string& name_or_id) const;

    // Simple status line text
    std::string status_line() const;

    // Detailed player info panels (used by .info CLI command).
    std::vector<std::string> player_info_panels() const;
    std::vector<std::string> player_info_panel(
        const std::string& panel_name) const;

    // ---- Channel management ----
    const std::vector<std::string>& channels() const { return channels_; }
    const std::string& current_channel() const { return current_channel_; }
    void set_current_channel(const std::string& ch) { current_channel_ = ch; }

    const std::map<ObjectID, ObjectInfo>& objects() const { return objects_; }

    // ---- Test helpers ----
    void set_battle_flag_for_test(bool enabled)
    {
        room_fight_flag_ = enabled ? 1 : 0;
    }
    void set_current_room_for_test(ObjectID room) { current_room_ = room; }
    void set_room_name_for_test(const std::string& name) { room_name_ = name; }

    // ---- NPC Menu / Dialog (QUESTION system) ----

    // Is the user currently in an NPC menu?
    bool in_menu() const;

    // Is the user currently in an NPC dialog? (n=name + m=message variant)
    bool in_dialog() const { return menu_state_.in_dialog; }

    // Get the current menu items for display
    const std::vector<MenuItem>& menu_items() const
    {
        return menu_state_.items;
    }

    // Select menu item by 0-based index. Returns the command string to send to
    // server. Closes the menu. Returns empty string if index is invalid or if
    // waiting for more input.
    std::string select_menu_item(int index);

    // Check if we're waiting for additional input for a menu command (e.g., $2,
    // $3 placeholders)
    bool is_waiting_for_menu_input() const;

    // Get the prompt text for pending menu input
    std::string get_pending_menu_prompt() const;

    // Complete a pending menu command with user input
    // Returns the completed command string, or empty if still waiting for more
    // input
    std::string complete_pending_menu_command(const std::string& user_input);

    // Cancel/close the current menu or dialog.
    void cancel_menu();

    void map_changed();

private:
    Connection& conn_;
    // --- Login & Identity ---
    bool logged_in_         = false;
    std::string account_id_ = "";  // account ID string
    ObjectID player_id_     = 0;
    int grant_level_        = 0;
    std::string server_name_;
    std::string server_version_;
    int64_t server_uptime_ = 0;
    int remaining_points_  = 0;

    // --- Player ---
    PlayerAttr player_attr_;

    // --- Skills ---
    std::vector<SkillInfo> skills_;

    // --- Room ---
    ObjectID current_room_ = 0;
    std::string room_name_;
    std::string room_map_name_;
    int room_fight_flag_ = 0;
    std::string room_entry_dir_;

    // --- Room objects ---
    // Key: container ID, Value: objects inside
    std::map<ObjectID, std::vector<ObjectInfo>> room_objects_;
    std::map<ObjectID, ObjectInfo> objects_;

    // --- Inventory ---
    std::vector<ObjectInfo> inventory_;

    // --- Online users ---
    std::vector<OnlineUser> online_users_;

    // --- Emotes ---
    std::vector<std::string> emotes_;

    // --- Bulletin ---
    bool bulletin_active_ = false;

    // --- Exits ---
    std::vector<DoorInfo> exits_;

    // --- Target ---
    ObjectID target_id_ = 0;

    // --- NPC Menu / Dialog ---
    MenuState menu_state_;

    // --- Channel management ---
    std::vector<std::string> channels_ = {
        "chat", "new", "ad",     "noise",
        "rumor", "party", "corps", "answer"};  // 服务端可发言频道
    std::string current_channel_ = "chat";     // active channel

    mutable std::mutex mutex_;
};