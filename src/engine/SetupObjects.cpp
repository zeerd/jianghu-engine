#include <windows.h>

#include "CObject.h"
#include "Room.h"

#include "Condition.h"
#include "Room.h"
#include "Skill.h"
#include "Corpse.h"
#include "Equip.h"
#include "FightRoom.h"
#include "Money.h"
#include "Vendor.h"
#include "BankOwner.h"
#include "FBRoom.h"
#include "User.h"
#include "Channel.h"
#include "Combatd.h"
#include "FBRegistry.h"
#include "LuaCondition.h"
#include "LuaEngine.h"
#include "LuaItem.h"
#include "LuaSkill.h"
#include "LuaNpc.h"
#include "LuaRoom.h"

#include <memory>

std::unique_ptr<CSkill> create_skill(string skill_name)
{
    // 所有技能均已 lua 化：直接创建 Lua 脚本技能。
    // 脚本的 base 字段不再选 C++ 基类（技能行为全在脚本里）。
    return std::make_unique<CLuaSkill>(skill_name.c_str());
}

std::unique_ptr<CCondition> create_condition(string base_name)
{
    // condition 已全部 lua 化：直接创建 Lua 脚本 condition。
    // 脚本缺失/损坏由 CLuaCondition::init 拒绝挂载，不在工厂层兜底。
    return std::make_unique<CLuaCondition>(base_name.c_str());
}

std::unique_ptr<CContainer> create_item(string item_name, string unique_id = "")
{
    std::unique_ptr<CContainer> t;
    LOG_DEBUG("base_name=%s, unique_id=%s", item_name.c_str(),
              unique_id.c_str());

    if (item_name == "coin")
        t = std::make_unique<CMCoin>();
    else if (item_name == "box")
        t = std::make_unique<CBox>();
    else if (item_name == "fight_room")
        t = std::make_unique<CFightRoom>();
    else if (item_name == "corpse_ob")
        t = std::make_unique<CCorpse>();
    else if (item_name.substr(0, 9) == "s_weapon_") {
        t = std::make_unique<CLuaEquip>("ssword");
        t->set("base_name", item_name.c_str());
        t->del("equipped");
    }

    // 该名字是否有 item 脚本：决定 C++ 基类来源，也决定是否允许
    // 专属实例存档恢复（coin/box 等无脚本特型不得发起脚本加载）。
    const bool has_script
        = g_LuaEngine->script_exists(item_name.c_str(), LUA_SCRIPT_ITEM);

    if (!t && has_script) {
        // 基类选择：脚本用 base 字段显式声明所需 C++ 基类（如套装声明
        // base="equip"）；未声明时沿用 type 字段。
        std::string base;
        if (!g_LuaEngine->get_script_str_field(item_name.c_str(),
                                               LUA_SCRIPT_ITEM, "base", base))
            base = g_LuaEngine->get_script_type(item_name.c_str(),
                                                LUA_SCRIPT_ITEM);
        if (base == "equip")
            t = std::make_unique<CLuaEquip>(item_name.c_str());
        else
            t = std::make_unique<CLuaItem>(item_name.c_str());
    }

    if (!t) {
        t = std::make_unique<CItem>();

        t->set("base_name", item_name.c_str());

        if (!t->load()) {
            LOG_ERROR("Fail Item:%s", item_name.c_str());

            string msg = "fi-";
            msg += item_name;

            t->set_name(msg.c_str());
        }

        t->del("equipped");
    }
    else if (has_script) {
        // 专属实例物件（脚本声明 save_table/save_key，如套装）：
        // 载入时按调用方给定的实例 id 从专属表恢复存档。
        std::string save_table, save_key;
        g_LuaEngine->get_script_str_field(item_name.c_str(), LUA_SCRIPT_ITEM,
                                          "save_table", save_table);
        g_LuaEngine->get_script_str_field(item_name.c_str(), LUA_SCRIPT_ITEM,
                                          "save_key", save_key);
        if (!save_key.empty() && unique_id.size() > 0) {
            if (!save_table.empty()) t->set("save_table", save_table.c_str());
            t->set("save_key", save_key.c_str());
            // load() 需 CAN_SAVE：声明了专属实例键即可存盘（早于 create 镜像）
            t->set("can_save", 1);
            t->set(save_key.c_str(), unique_id.c_str());
            if (!t->load()) {
                LOG_ERROR("Fail Savedata:%s/%s", item_name.c_str(),
                          unique_id.c_str());
            }
            t->del("equipped");
        }
    }

    return t;
}

std::unique_ptr<CNpc> create_npc(string npc_name)
{
    std::unique_ptr<CNpc> npc;

    if (g_LuaEngine->script_exists(npc_name.c_str(), LUA_SCRIPT_NPC)) {
        const char* stype = g_LuaEngine->get_script_type(npc_name.c_str(),
                                                         LUA_SCRIPT_NPC, "npc");
        if (strcmp(stype, "vendor") == 0)
            npc = std::make_unique<CLuaVendor>(npc_name.c_str());
        else if (strcmp(stype, "bankowner") == 0)
            npc = std::make_unique<CLuaBankOwner>(npc_name.c_str());
        else if (strcmp(stype, "mirror") == 0)
            npc = std::make_unique<CLuaMirrorNpc>(npc_name.c_str());
        else
            npc = std::make_unique<CLuaNpc>(npc_name.c_str());
    }

    if (!npc) {
        npc = std::make_unique<CNpc>();

        npc->set("base_name", npc_name.c_str());
        if (!npc->load()) {
            LOG_ERROR("Fail NPC:%s", npc_name.c_str());

            string msg = "fn-";
            msg += npc_name;
            npc->set_name(msg.c_str());
        }
    }

    return npc;
}

std::unique_ptr<CFB> create_fb(string fb_name)
{
    std::unique_ptr<CFB> f;

    if (g_LuaEngine->script_exists(fb_name.c_str(), LUA_SCRIPT_ROOM)) {
        const char* stype = g_LuaEngine->get_script_type(fb_name.c_str(),
                                                         LUA_SCRIPT_ROOM, "fb");
        if (strcmp(stype, "fb") == 0)
            f = std::make_unique<CLuaFB>(fb_name.c_str());
    }

    return f;
}

std::unique_ptr<CFBRoom> create_fb_room(string room_name, CFB* fb)
{
    std::unique_ptr<CFBRoom> r;

    string area_name;
    area_name = room_name.substr(0, 8);

    const char* script_name = g_LuaEngine->resolve_room_name(room_name.c_str());
    if (g_LuaEngine->script_exists(script_name, LUA_SCRIPT_ROOM)) {
        r = std::make_unique<CLuaFBRoom>(script_name);
        if (r) {
            r->set("area", area_name.c_str());
            if (fb) r->m_parent_fb = fb;
        }
    }

    if (!r) {
        r = std::make_unique<CFBRoom>(fb);

        r->set("base_name", room_name.c_str());

        LOG_ERROR("没有该FB房间%s，替换成标准FB房间。", room_name.c_str());

        string msg = "fail";
        msg += room_name;
        r->set_name(msg.c_str());
        r->set("fail_ob", 1);

        r->set("infinity", 1);
    }

    return r;
}

std::unique_ptr<CRoom> create_room(string room_name)
{
    std::unique_ptr<CRoom> r;

    {
        string area_name;
        area_name = room_name.substr(0, 4);
        LOG_DEBUG("room_name=%s", room_name.c_str());

        const char* script_name
            = g_LuaEngine->resolve_room_name(room_name.c_str());
        LOG_DEBUG("script_name=%s", script_name);
        if (g_LuaEngine->script_exists(script_name, LUA_SCRIPT_ROOM)) {
            r = std::make_unique<CLuaRoom>(script_name);
            if (r) r->set("area", area_name.c_str());
        }
    }

    if (!r) {
        // 动态实例房间：base_name 命中某共享脚本声明的前缀时实例化该脚本，
        // 并复刻存档恢复时序（load 早于 create）。
        const char* prefix_script
            = g_LuaEngine->resolve_room_prefix(room_name.c_str());
        if (prefix_script[0]) {
            r = std::make_unique<CLuaRoom>(prefix_script);
            r->set("base_name", room_name.c_str());
            // 专属存盘表（如帮派总舵）：脚本声明 save_table，镜像进 dbase，
            // 使 load() 早于 create() 时即按该表路由。C++ 不认识具体表名。
            std::string save_table;
            if (g_LuaEngine->get_script_str_field(
                    prefix_script, LUA_SCRIPT_ROOM, "save_table", save_table)
                && !save_table.empty()) {
                r->set("save_table", save_table.c_str());
            }
            if (!r->load()) {
                LOG_ERROR("没有该房间%s，替换成标准房间。", room_name.c_str());
                string msg = "fail";
                msg += room_name;
                r->set_name(msg.c_str());
                r->set("fail_ob", 1);
            }
            r->set("infinity", 1);
        }
    }

    if (!r) {
        // 房屋等前缀动态房由 create_room 的 resolve_room_prefix 分支在
        // 上面就绪；此处只兜底静态房与 fb_registry
        if (room_name == "fb_registry") {
            // 内置副本注册处：纯 C++ 容器，不落库，t_room 里没有它的行。
            // 走 load() 必然失败，既误报"没有该房间"，又给它挂上 fail_ob，
            // 因此这里直接跳过读库。
            r = std::make_unique<CFBRegistry>();
            r->set("base_name", room_name.c_str());
        }
        else {
            r = std::make_unique<CRoom>();

            r->set("base_name", room_name.c_str());

            if (!r->load()) {
                LOG_ERROR("没有该房间%s，替换成标准房间。", room_name.c_str());

                string msg = "fail";
                msg += room_name;
                r->set_name(msg.c_str());
                r->set("fail_ob", 1);
            }
        }

        r->set("infinity", 1);
    }

    return r;
}
