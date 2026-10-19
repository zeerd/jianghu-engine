#pragma once

#include <string>

#include "types.h"

// Target lookup helpers.
// Searches room objects and online users in the provided containers.
// Returns the object/player ID, or 0 if not found.

ObjectID find_target_by_id(
    const std::string &name_or_id,
    const std::map<ObjectID, std::vector<ObjectInfo>> &room_objects,
    const std::vector<OnlineUser> &online_users);

#endif  // GAME_STATE_TARGET_H