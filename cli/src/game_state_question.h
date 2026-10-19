#pragma once

#include <cstdint>
#include <string>
#include <vector>

// A single menu item from the NPC QUESTION system (C=27).
struct MenuItem {
    std::string text;  // s= display text shown to user
    std::string
        raw_cmd;  // c= command format (may contain $0, $1, $2 placeholders)
};

// State tracker for NPC menu interaction.
struct MenuState {
    int64_t npc_id = 0;           // N= NPC object ID
    std::vector<MenuItem> items;  // accumulated menu items
    bool in_menu  = false;        // true while menu is open
    bool building = false;  // true while receiving menu items (before e=1)

    // NPC dialog (also sent as C=27 but with n= and m= fields)
    bool in_dialog = false;
    std::string dialog_npc_name;
    std::string dialog_text;

    // Waiting for additional user input (for $2, $3 placeholders in menu
    // commands)
    bool waiting_for_input = false;
    std::string pending_cmd;  // partial command with unresolved placeholders
    std::string pending_item_text;                  // menu item text for prompt
    std::vector<std::string> pending_placeholders;  // e.g., ["$2", "$3"]
};
