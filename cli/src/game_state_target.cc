#include "game_state.h"

ObjectID GameState::find_target(const std::string &target_key) const
{
    // /t 只收 ID（不支持显示名——全服允许重名，名字无法唯一定位目标）：
    //   玩家      → 数字账号 ID，全服有效（在线表条目经 DOWNUSER 回填，
    //                无论目标是否同房间）
    //   NPC/物品等 → 英文 id（对象 dbase"id"，服务器 I= 字段），当前环境
    //                （所在房间 + 背包）有效
    // 先按数字 ID 试（会话内对象号 / 在线玩家会话号），再按英文 id 精确匹配。

    // 1. Try numeric ID first
    try {
        ObjectID id = std::stoll(target_key);
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

    // 2. Try matching by English id (dbase "id", 服务器对象信息 I= 字段)
    for (const auto &[container, objs] : room_objects_) {
        for (const auto &o : objs) {
            if (o.internal_id == target_key) return o.id;
        }
    }
    // Check inventory (equipped/worn items)
    for (const auto &o : inventory_) {
        if (o.internal_id == target_key) return o.id;
    }
    for (const auto &u : online_users_) {
        if (u.internal_id == target_key) return u.uid;
    }

    return 0;
}