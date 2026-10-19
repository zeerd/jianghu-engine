#include "game_state.h"

ObjectID GameState::find_target(const std::string &name_or_id) const
{
    // 1. Try numeric ID first
    try {
        ObjectID id = std::stoll(name_or_id);
        // Check room objects
        for (const auto &[container, objs] : room_objects_) {
            for (const auto &o : objs) {
                if (o.id == id) return id;
            }
        }
        // Check inventory (equipped/worn items)
        for (const auto &o : inventory_) {
            if (o.id == id) return id;
        }
        // Check online users
        for (const auto &u : online_users_) {
            if (u.uid == id) return id;
        }
        // Also check the player's own ID — otherwise "/t 1" fails when your own
        // ID is 1
        if (player_id_ == id) return id;
    } catch (...) {
    }

    // 2. Try matching by name (display name)
    for (const auto &[container, objs] : room_objects_) {
        for (const auto &o : objs) {
            if (o.name == name_or_id) return o.id;
        }
    }
    // Check inventory (equipped/worn items)
    for (const auto &o : inventory_) {
        if (o.name == name_or_id) return o.id;
    }
    for (const auto &u : online_users_) {
        if (u.name == name_or_id) return u.uid;
    }
    // Also check player's own name as an alias
    if (!player_attr_.name.empty() && player_attr_.name == name_or_id)
        return player_id_;

    // 3. Try matching by internal_id (account ID)
    for (const auto &[container, objs] : room_objects_) {
        for (const auto &o : objs) {
            if (o.internal_id == name_or_id) return o.id;
        }
    }
    // Check inventory (equipped/worn items)
    for (const auto &o : inventory_) {
        if (o.internal_id == name_or_id) return o.id;
    }
    for (const auto &u : online_users_) {
        if (u.internal_id == name_or_id) return u.uid;
    }

    return 0;
}