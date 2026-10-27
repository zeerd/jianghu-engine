#include "game_state.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "color.h"
#include "dbg.h"
#include "game_state_info.h"
#include "protocol.h"

namespace {

// 合并重复的 ADDOBJECT（改名重发会再发一次 11+17）：同一容器内按 object
// id 幂等——只刷新可见状态位，保留已通过 DOWNOBJECT 下载的名字等详情；
// 不在则追加一条。
void merge_object_info(ObjectInfo &dst, const ObjectInfo &src)
{
    dst.is_user = src.is_user;
    dst.is_item = src.is_item;
    dst.updated = src.updated;
    dst.tea     = src.tea;
    dst.ap      = src.ap;
    dst.sta     = src.sta;
}

void merge_object_info(std::vector<ObjectInfo> &list, const ObjectInfo &src)
{
    for (auto &o : list) {
        if (o.id == src.id) {
            merge_object_info(o, src);
            return;
        }
    }
    list.push_back(src);
}

void merge_object_info(std::map<ObjectID, ObjectInfo> &table,
                       const ObjectInfo &src)
{
    auto it = table.find(src.id);
    if (it == table.end()) {
        table.emplace(src.id, src);
        return;
    }
    merge_object_info(it->second, src);
}

}  // namespace

std::vector<std::string> GameState::handle_message(const GameMessage &msg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> output_lines;

    // DBG("Cmd:" << msg.cmd << ": " << msg.to_string());
    switch (msg.cmd) {
        // --- 10: GOTOROOM ---
        case 10: {
            DBG("GOTOROOM: " << msg.to_string());
            current_room_    = msg.get_int("R");
            room_map_name_   = msg.get("I");  // 佛山英雄会馆
            room_name_       = msg.get("N");  // 英雄会馆
            room_fight_flag_ = static_cast<int>(msg.get_int("F"));
            room_entry_dir_  = msg.get("D");  // 佛山

            // Clear room objects on room change
            room_objects_.clear();
            exits_.clear();

            output_lines.push_back(
                color::convert("$GRN你进入了" + room_name_ + "。\n$COM"));

            map_changed();
            break;
        }

        // 11: ADDOBJECT — object enters container/room
        case 11: {
            DBG("ADDOBJECT: " << msg.to_string());
            ObjectID env = msg.get_int("E");
            ObjectInfo obj;
            obj.id      = msg.get_int("T");
            obj.is_user = (msg.get_int("u") != 0);
            obj.is_item = (msg.get_int("i") != 0);
            obj.updated = (msg.get_int("U") != 0);
            obj.tea     = static_cast<int>(msg.get_int("tea"));
            obj.ap      = static_cast<int>(msg.get_int("ap"));
            obj.sta     = static_cast<int>(msg.get_int("sta"));

            // If this is an inventory item (env is player itself), track it
            if (env == player_id_ && obj.is_item) {
                merge_object_info(inventory_, obj);
            }

            merge_object_info(objects_, obj);  // Store in objects_ map
            // Add to room_objects_ map
            merge_object_info(room_objects_[env], obj);
            break;
        }

        // 12: REMOVEOBJECT
        case 12: {
            DBG("REMOVEOBJECT: " << msg.to_string());
            ObjectID env_id    = msg.get_int("E");
            ObjectID target_id = msg.get_int("T");

            // Remove from inventory if applicable (try all items, not
            // env-dependent)
            inventory_.erase(
                std::remove_if(inventory_.begin(), inventory_.end(),
                               [target_id](const ObjectInfo &o) {
                                   return o.id == target_id;
                               }),
                inventory_.end());

            // Remove from room objects
            auto it = room_objects_.find(env_id);
            if (it != room_objects_.end()) {
                auto &vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                                         [target_id](const ObjectInfo &o) {
                                             return o.id == target_id;
                                         }),
                          vec.end());
            }
            objects_.erase(target_id);  // Remove from objects_ map
            break;
        }

        // 13: RTNSKILL
        case 13: {
            DBG("RTNSKILL: " << msg.to_string());
            SkillInfo sk;
            sk.id           = msg.get_int("T");
            sk.base_name    = msg.get("S");
            sk.level        = static_cast<int>(msg.get_int("L"));
            sk.potential    = static_cast<int>(msg.get_int("P"));
            sk.mapped_skill = msg.get("M");
            sk.prepared     = msg.has("K");
            // 服务端每次技能变化都全量重推技能列表（flush_my_skill），
            // 按 base_name 幂等合并：已存在则更新动态字段，保留已下载
            // 的元数据（显示名/图标/法术表由后续推送刷新）。
            bool merged = false;
            for (auto &s : skills_) {
                if (s.base_name != sk.base_name) continue;
                s.id           = sk.id;
                s.level        = sk.level;
                s.potential    = sk.potential;
                s.mapped_skill = sk.mapped_skill;
                s.prepared     = sk.prepared;
                merged         = true;
                break;
            }
            if (!merged) skills_.push_back(sk);

            conn_.send_command_utf8("0 downloadskill 0 " + sk.base_name + "\t");
            break;
        }

        // 14: RTNITEM — player's unified item list (all items with wield pos)
        case 14: {
            DBG("RTNITEM: " << msg.to_string());
            int idx = 0;
            while (true) {
                std::string ikey = "I" + std::to_string(idx);
                std::string ekey = "E" + std::to_string(idx);
                if (!msg.has(ikey) && !msg.has(ekey)) break;
                ObjectID item_oid = msg.get_int(ikey, 0);
                int wield_pos     = static_cast<int>(msg.get_int(ekey, 0));

                // Update inventory item's wield position
                bool found = false;
                for (auto &inv_item : inventory_) {
                    if (inv_item.id == item_oid) {
                        inv_item.wield = wield_pos;
                        found          = true;
                        break;
                    }
                }
                // Add new entry for items not yet tracked in inventory_
                if (!found && item_oid > 0) {
                    ObjectInfo obj;
                    obj.id      = item_oid;
                    obj.is_item = true;
                    obj.wield   = wield_pos;
                    inventory_.push_back(std::move(obj));
                }
                ++idx;
            }
            break;
        }

        // 15: RTNINFO
        case 15: {
            ObjectID target_id = msg.get_int("T");
            if (target_id == player_id_) {
                DBG("RTNINFO: " << msg.to_string());
                player_attr_.id           = target_id;
                player_attr_.name         = msg.get("p");
                player_attr_.icon         = static_cast<int>(msg.get_int("P"));
                player_attr_.gender       = msg.get("q");
                player_attr_.job          = msg.get("r");
                player_attr_.race         = msg.get("s");
                player_attr_.title        = msg.get("t");
                player_attr_.carry_weight = msg.get_int("u");
                player_attr_.max_carry    = msg.get_int("v");
                player_attr_.exp          = msg.get_int("exp");
                player_attr_.next_exp     = msg.get_int("nex");
                player_attr_.skill_exp    = msg.get_int("sk");
                player_attr_.master       = msg.get("master");
                player_attr_.age          = static_cast<int>(msg.get_int("a"));
                player_attr_.level        = static_cast<int>(msg.get_int("b"));
                player_attr_.attack       = msg.get_int("I");  // attack
                player_attr_.damage       = msg.get_int("J");  // apply/damage
                player_attr_.defense      = msg.get_int("K");  // defense
                player_attr_.armor        = msg.get_int("L");  // apply/armor
                player_attr_.str          = static_cast<int>(msg.get_int("E"));
                player_attr_.con          = static_cast<int>(msg.get_int("F"));
                player_attr_.intel        = static_cast<int>(msg.get_int("G"));
                player_attr_.dex          = static_cast<int>(msg.get_int("H"));
                player_attr_.hp           = msg.get_int("e");
                player_attr_.max_hp       = msg.get_int("g");
                player_attr_.mp           = msg.get_int("f");
                player_attr_.max_mp       = msg.get_int("h");
                player_attr_.eff_hp       = msg.get_int("i");
                player_attr_.gin          = msg.get_int("gin");
                player_attr_.max_gin      = msg.get_int("max_gin");
                player_attr_.tp           = msg.get_int("tp");
                player_attr_.shen         = msg.get_int("shen");
                player_attr_.shen_type
                    = static_cast<int>(msg.get_int("shen_type"));
                player_attr_.combat_exp = static_cast<int>(msg.get_int("d"));
                player_attr_.pks        = static_cast<int>(msg.get_int("m"));
                player_attr_.mks        = static_cast<int>(msg.get_int("n"));
            }
            break;
        }

        // 16: DOWNSKILL
        case 16: {
            // Skill metadata download
            DBG("DOWNSKILL: " << msg.to_string());
            std::string base_name  = msg.get("S");  // huashan_jianfa
            std::string skill_name = msg.get("N");  // 华山剑法
            std::string base_skill = msg.get("E");  // sword
            int16_t icon           = static_cast<int16_t>(msg.get_int("P"));
            for (auto &s : skills_) {
                if (s.base_name == base_name) {
                    s.skill_name = skill_name;
                    s.base_skill = base_skill;
                    s.icon       = icon;
                }
            }
            break;
        }

        // 17: DOWNOBJECT — detailed object info
        case 17: {
            ObjectID obj_id = msg.get_int("O");
            ObjectInfo info;
            info.id          = obj_id;
            info.name        = msg.get("N");
            info.internal_id = msg.get("I");
            info.icon        = static_cast<int>(msg.get_int("P"));
            info.qty         = static_cast<int>(msg.get_int("a", 1));
            info.is_user     = (msg.get_int("U") != 0);
            info.weight      = msg.get_int("W");
            info.bx          = static_cast<int>(msg.get_int("bx"));
            info.by          = static_cast<int>(msg.get_int("by"));
            info.ex          = static_cast<int>(msg.get_int("ex"));
            info.ey          = static_cast<int>(msg.get_int("ey"));
            info.direction   = static_cast<int>(msg.get_int("dr"));
            info.color       = static_cast<int>(msg.get_int("ac"));
            info.title       = msg.get("t");

            // Search for AddObject entry and fill in name
            for (auto &[container, objs] : room_objects_) {
                for (auto &o : objs) {
                    if (o.id == obj_id) {
                        o.name        = info.name;
                        o.internal_id = info.internal_id;
                        o.title       = info.title;
                        o.qty         = info.qty;
                        o.weight      = info.weight;
                    }
                }
            }
            for (auto &[container, o] : objects_) {
                if (o.id == obj_id) {
                    o.name        = info.name;
                    o.internal_id = info.internal_id;
                    o.title       = info.title;
                    o.qty         = info.qty;
                    o.weight      = info.weight;
                }
            }

            // Sync to player inventory as well
            for (auto &inv_item : inventory_) {
                if (inv_item.id == obj_id) {
                    inv_item.name        = info.name;
                    inv_item.internal_id = info.internal_id;
                    inv_item.title       = info.title;
                    inv_item.qty         = info.qty;
                    inv_item.weight      = info.weight;
                    break;
                }
            }
            break;
        }

        // 18: LOGINUSER
        case 18: {
            if (msg.has("F")) {
                // Login failure
                output_lines.push_back(color::convert(
                    "$RED" + msg.get("M", "Login failed") + "$COM"));
            }
            else {
                player_id_         = msg.get_int("U");
                grant_level_       = static_cast<int>(msg.get_int("G"));
                server_version_    = msg.get("V");
                server_uptime_     = msg.get_int("R");
                server_name_       = msg.get("M");
                remaining_points_  = static_cast<int>(msg.get_int("l"));
                logged_in_         = true;

                // seconds to days, hours, minutes, seconds
                int days    = static_cast<int>(server_uptime_ / 86400);
                int hours   = static_cast<int>((server_uptime_ % 86400) / 3600);
                int minutes = static_cast<int>((server_uptime_ % 3600) / 60);
                int seconds = static_cast<int>(server_uptime_ % 60);
                std::ostringstream oss;
                oss << "$HIG【系统】" << server_name_ << "已经运行了 " << days
                    << "天" << hours << "小时" << minutes << "分" << seconds
                    << "秒。当前版本" << server_version_ << "。$COM";
                output_lines.push_back(color::convert(oss.str()));
                map_changed();

                // Refresh all player info (including inventory) after login
                conn_.send_command_utf8("0 downloaduser "
                                        + std::to_string(player_id_) + "\t");
            }
            break;
        }

        // 19: NOTIFY
        case 19: {
            DBG("NOTIFY: " << msg.to_string());
            std::string text = msg.get("M");
            output_lines.push_back(color::convert(text));
            break;
        }

        // 20: WAKEUP
        case 20: {
            output_lines.push_back(color::convert("$CYNYou wake up.$COM"));
            break;
        }

        // 21: DOWNEMOTE
        case 21: {
            emotes_.push_back(msg.get("e"));
            break;
        }

        // 22: COMMON_ATTACK
        case 22: {
            DBG("COMMON_ATTACK: " << msg.to_string());
            output_lines.push_back(color::convert(msg.get("M")));
            break;
        }

        // 23: STATE
        case 23: {
            ObjectID target_id = msg.get_int("T");
            if (target_id == player_id_) {
                DBG("STATE: " << msg.to_string());
                if (msg.has("tea")) {
                    // tea field present (player is in battle)
                }
                if (msg.has("sta")) player_attr_.state = msg.get_int("sta");
                if (msg.has("idl")) player_attr_.idle = msg.get_int("idl") != 0;
                if (msg.has("hp")) player_attr_.hp = msg.get_int("hp");
                if (msg.has("mp")) player_attr_.mp = msg.get_int("mp");
                if (msg.has("eh")) player_attr_.eff_hp = msg.get_int("eh");
                if (player_attr_.state > 0)
                    player_attr_.max_hp
                        = player_attr_.hp * 100 / player_attr_.state;
                if (msg.has("mm")) player_attr_.max_mp = msg.get_int("mm");
                if (msg.has("att")) player_attr_.attack = msg.get_int("att");
                if (msg.has("def")) player_attr_.defense = msg.get_int("def");
                if (msg.has("jl")) player_attr_.enforce = msg.get_int("jl");
            }
            break;
        }

        // 24: ADDDOOR
        case 24: {
            DBG("ADDDOOR: " << msg.to_string());
            ObjectID room_id = msg.get_int("R");
            if (room_id == current_room_) {
                DoorInfo door;
                door.door_key  = msg.get("D");
                door.dir_label = msg.get("A");
                door.room_name = msg.get("N");
                door.dest_room_id
                    = msg.get_int("R2");  // Try to get destination room ID
                if (door.dest_room_id == 0) {
                    door.dest_room_id = msg.get_int("T");  // Alternative field
                }
                exits_.push_back(std::move(door));
            }
            break;
        }

        // 25: SENDUSER
        case 25: {
            DBG("SENDUSER message: " << msg.to_string());
            bool online = (msg.get_int("O") != 0);
            int64_t uid = msg.get_int("U");
            if (online && player_id_ != 0) {
                conn_.send_command_utf8(std::to_string(player_id_)
                                        + " downloaduser " + std::to_string(uid)
                                        + "\t");
            }
            break;
        }

        // 26: VENDOR_LIST
        case 26: {
            DBG("VENDOR_LIST: " << msg.to_string());
            int page      = static_cast<int>(msg.get_int("P"));
            int start_idx = static_cast<int>(msg.get_int("B", 0));
            output_lines.push_back("--- Shop (page " + std::to_string(page + 1)
                                   + ") ---");
            for (int i = 0; i < 10; ++i) {
                std::string gkey = "g" + std::to_string(start_idx + i);
                std::string nkey = "n" + std::to_string(start_idx + i);
                std::string bkey = "b" + std::to_string(start_idx + i);
                std::string akey = "a" + std::to_string(start_idx + i);
                if (!msg.has(gkey)) break;
                int code          = static_cast<int>(msg.get_int(gkey));
                std::string name  = msg.get(nkey);
                std::string price = msg.get(bkey);
                int qty           = static_cast<int>(msg.get_int(akey));
                output_lines.push_back("  [" + std::to_string(code) + "] "
                                       + name + "  $" + price + "  (x"
                                       + std::to_string(qty) + ")");
            }
            break;
        }

        // 27: QUESTION → NPC menu/dialog
        case 27: {
            auto menu_lines = handle_question(msg);
            output_lines.insert(output_lines.end(), menu_lines.begin(),
                                menu_lines.end());
            break;
        }

        // 28: DOWNMAGIC
        case 28: {
            DBG("DOWNMAGIC: " << msg.to_string());
            std::string skill_base_name = msg.get("S");  // 所属技能
            std::string magic_name      = msg.get("M");  // 法术标识
            std::string display_name    = msg.get("N");  // 法术显示名称
            int force = static_cast<int>(msg.get_int("R"));  // 内力消耗
            int mp    = static_cast<int>(msg.get_int("A"));  // 气/MP消耗
            int valid_cast
                = static_cast<int>(msg.get_int("V"));  // 有效施展条件
            int target = static_cast<int>(msg.get_int("T"));  // 目标类型

            // Find the skill and add magic to it
            for (auto &sk : skills_) {
                if (sk.base_name != skill_base_name) continue;
                // 法术随技能全量重推反复下发，按 name 幂等合并
                MagicInfo *slot = nullptr;
                for (auto &m : sk.magics) {
                    if (m.name == magic_name) {
                        slot = &m;
                        break;
                    }
                }
                if (!slot) {
                    sk.magics.emplace_back();
                    slot = &sk.magics.back();
                }
                slot->name         = magic_name;
                slot->display_name = display_name;
                slot->force        = force;
                slot->mp           = mp;
                slot->valid_cast   = valid_cast;
                slot->target       = target;
                break;
            }
            break;
        }

        // 29: NOTIFY_FAIL
        case 29: {
            output_lines.push_back(color::convert(
                "$RED" + msg.get("M", "Command failed") + "$COM"));
            break;
        }

        // 30: BULLET
        case 30: {
            DBG("BULLET: " << msg.to_string());
            if (msg.has("B") && msg.get_int("B") == 1) {
                bulletin_active_ = true;
            }
            else if (msg.has("M")) {
                output_lines.push_back(color::convert(msg.get("M")));
            }
            else {
                bulletin_active_ = false;
            }
            break;
        }

        // 31: TELLOBJECT
        case 31: {
            output_lines.push_back(color::convert(msg.get("M")));
            break;
        }

        // 32: LOCATION
        case 32: {
            DBG("LOCATION: " << msg.to_string());
            break;
        }

        // 33: USERLIST
        case 33: {
            online_users_.clear();
            int count = static_cast<int>(msg.get_int("n", 0));
            for (int i = 0; i < count; ++i) {
                std::string key = "i" + std::to_string(i);
                if (!msg.has(key)) break;
                OnlineUser user;
                user.uid   = msg.get_int(key);
                user.level = 0;
                online_users_.push_back(user);
            }
            break;
        }

        // 34: DOWNUSER
        case 34: {
            ObjectID uid           = msg.get_int("U");
            std::string name       = msg.get("N");
            std::string account_id = msg.get("I");  // account ID string
            int level              = static_cast<int>(msg.get_int("r"));
            std::string title      = msg.get("t");

            // Update existing entry or add new one
            bool found = false;
            for (auto &u : online_users_) {
                if (u.uid == uid) {
                    u.level       = level;
                    u.title       = title;
                    u.name        = name;
                    u.internal_id = account_id;
                    found         = true;
                    break;
                }
            }
            if (!found) {
                OnlineUser user;
                user.uid         = uid;
                user.name        = name;
                user.internal_id = account_id;
                user.level       = level;
                user.title       = title;
                online_users_.push_back(user);
            }

            // Also update room_objects if this user is in the room
            for (auto &[container, objs] : room_objects_) {
                for (auto &o : objs) {
                    if (o.id == uid) {
                        o.name        = name;
                        o.internal_id = account_id;
                    }
                }
            }
            break;
        }

        // 35: MAGICCAST
        case 35: {
            DBG("MAGICCAST: " << msg.to_string());
            output_lines.push_back(color::convert("$HIM[MAGIC]$COM"));
            break;
        }

        // 36: SYN_AP
        case 36: {
            DBG("SYN_AP: " << msg.to_string());
            break;
        }

        // 37: SENDTABLE
        case 37: {
            break;
        }

        // 38: CONTROLNPC
        case 38: {
            break;
        }

        // 39: ADDLOG
        case 39: {
            break;
        }

        default:
            break;
    }

    return output_lines;
}

void GameState::map_changed()
{
    if (player_id_ == 0) return;
    conn_.send_command_utf8(std::to_string(player_id_) + " loc 0 12 12\t");
}

std::string GameState::room_name() const { return room_name_; }

std::string GameState::room_map() const { return room_map_name_; }

std::string GameState::status_line() const
{
    std::ostringstream ss;
    ss << room_map_name_ << " | " << room_name_;
    if (room_fight_flag_) ss << " [BATTLE]";
    ss << " | HP:" << player_attr_.hp << "/" << player_attr_.max_hp;
    ss << " | MP:" << player_attr_.mp << "/" << player_attr_.max_mp;
    ss << " | EXP:" << player_attr_.exp;
    return ss.str();
}

std::vector<std::string> GameState::player_info_panels() const
{
    return ::player_info_panels(*this);
}

std::vector<std::string> GameState::player_info_panel(
    const std::string &panel_name) const
{
    return ::player_info_panel(*this, panel_name);
}

bool GameState::in_menu() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return menu_state_.in_menu;
}

std::vector<ObjectInfo> GameState::room_npcs() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ObjectInfo> npcs;
    auto it = room_objects_.find(current_room_);
    if (it == room_objects_.end()) return npcs;
    for (const auto &obj : it->second) {
        if (!obj.is_user && !obj.is_item) {
            npcs.push_back(obj);
        }
    }
    return npcs;
}

std::vector<ObjectInfo> GameState::room_chars() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ObjectInfo> chars;
    auto it = room_objects_.find(current_room_);
    if (it == room_objects_.end()) return chars;
    for (const auto &obj : it->second) {
        if (obj.is_user) {
            chars.push_back(obj);
        }
    }
    return chars;
}

std::vector<ObjectInfo> GameState::room_items() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ObjectInfo> items;
    auto it = room_objects_.find(current_room_);
    if (it == room_objects_.end()) return items;
    for (const auto &obj : it->second) {
        if (obj.is_item) {
            items.push_back(obj);
        }
    }
    return items;
}
