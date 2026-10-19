#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

using ObjectID = int64_t;

struct GameMessage {
    int cmd;                                    // Command code (&C=N)
    std::map<std::string, std::string> params;  // key=val pairs

    std::string get(const std::string &key,
                    const std::string &default_val = "") const;
    int64_t get_int(const std::string &key, int64_t default_val = 0) const;
    bool has(const std::string &key) const;
    std::string to_string() const;  // for debugging
};

// Player attribute snapshot
struct PlayerAttr {
    ObjectID id = 0;
    std::string name;
    int icon = 0;
    std::string gender;
    std::string job;
    std::string race;
    std::string title;
    int64_t carry_weight = 0;
    int64_t max_carry    = 0;
    int64_t exp          = 0;
    int64_t next_exp     = 0;
    int64_t skill_exp    = 0;
    std::string master;
    int age         = 0;
    int level       = 0;
    int64_t attack  = 0;
    int64_t damage  = 0;
    int64_t defense = 0;
    int64_t armor   = 0;
    int str = 0, con = 0, intel = 0, dex = 0;
    int8_t state = 0;  // hp * 100 / max_hp
    bool idle    = false;
    int64_t hp = 0, max_hp = 0, eff_hp = 0;
    int64_t mp = 0, max_mp = 0;
    int64_t enforce = 0;
    int64_t gin = 0, max_gin = 0;
    int64_t tp         = 0;
    int64_t shen       = 0;
    int64_t shen_type  = 0;
    int64_t combat_exp = 0;
    int pks            = 0;
    int mks            = 0;
};

// Magic/绝招 info (绝招)

struct MagicInfo {
    std::string name;          // 法术标识 (base_name, from M)
    std::string display_name;  // 法术显示名称 (from N)
    int force = 0;             // 每次施展消耗内力点 (from R)
    int mana = 0;  // 每次施展消耗法力/内力 (from M field - xclient uses mana)
    int mp         = 0;  // 每次施展消耗MP/气 (from A)
    int ap         = 0;  // 每次施展消耗AP (from A - xclient uses ap)
    int valid_cast = 0;  // 有效施展条件 (from V)
    int target     = 0;  // 目标类型 (from T)
};

// Skill info

struct SkillInfo {
    ObjectID id   = 0;
    int level     = 0;
    int potential = 0;
    std::string mapped_skill;
    bool prepared = false;
    std::string base_name;
    std::string skill_name;
    std::string base_skill;
    int16_t icon = 0;
    std::vector<MagicInfo> magics;
};

// Object in room / inventory
struct ObjectInfo {
    ObjectID id = 0;
    std::string name;
    std::string internal_id;
    int icon       = 0;
    int qty        = 0;
    bool is_user   = false;  // player character (&u=1)
    bool is_item   = false;  // item (&i=1)
    bool updated   = false;  // updated since last display
    int64_t weight = 0;
    // NPC extras
    int bx = 0, by = 0, ex = 0, ey = 0;
    int direction = 0;
    int color     = 0;           // character color
    std::string title;           // NPC title
    int fight_flag = 0;          // combat flag
    std::string team_a, team_b;  // team info
    int wield = 0;               // equipped slot (WIELD_*)
    // State tracking
    int tea = 0;  // attack power
    int ap  = 0;  // action points
    int sta = 0;  // HP percent
};

// Door / exit
struct DoorInfo {
    std::string door_key;  // &D= — direction keyword e.g. "north", "south"
    std::string dir_label;  // &A= — display direction label e.g. "正北", "南方"
    std::string room_name;  // &N= — destination room display name
    ObjectID dest_room_id
        = 0;  // destination room object ID (from &R= or tracking)
};

// Vendor listing
struct VendorItem {
    int code = 0;
    std::string name;
    std::string price;
    int qty = 0;
};

// Online user
struct OnlineUser {
    ObjectID uid = 0;
    std::string name;
    std::string internal_id;  // account ID (&I=)
    int level = 0;
    std::string title;
};