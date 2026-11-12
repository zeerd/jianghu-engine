/** @file LuaBind.cpp @brief C++ 对象到 Lua 的绑定实现 */

#include <windows.h>

#include "LuaBind.h"
#include "LuaEngine.h"

#include <lua.hpp>

#include <algorithm>
#include <sys/stat.h>

#include "Char.h"
#include "Global.h"
#include "IniParser.h"

#include "LuaBindTypes.h"
#include "LuaBindDecls.h"
#include "LuaBindEnums.h"
#include "LuaHook.h"

// 把对象压入栈
void lua_bind_push_object(lua_State* L, CMainObject* obj)
{
    if (!obj) {
        lua_pushnil(L);
        return;
    }

    LuaObject* ud = (LuaObject*)lua_newuserdata(L, sizeof(LuaObject));
    ud->obj       = obj;

    // 设置 metatable
    luaL_getmetatable(L, "jianghu_object");
    lua_setmetatable(L, -2);
}

// helper used by scattered binds (declared in LuaBindTypes.h)
CChar* lua_check_char(lua_State* L, int idx)
{
    if (!lua_isuserdata(L, idx)) return NULL;
    return dynamic_cast<CChar*>(get_object(L, idx));
}

static const luaL_Reg object_methods[]
    = {{"set", l_set},
       {"query", l_query},
       {"querystr", l_querystr},
       {"add", l_add},
       {"del", l_del},
       {"set_name", l_set_name},
       {"set_heart_beat", l_set_heart_beat},
       {"create", l_create},
       {"dbase_temp", l_dbase_temp},
       {"name", l_name},
       {"id", l_id},
       {"object_id", l_object_id},
       {"desc", l_desc},
       // CContainer
       {"say", l_say},
       {"set_weight", l_set_weight},
       {"move", l_move},
       {"environment", l_environment},
       {"query_fb_parent", l_query_fb_parent},
       {"present", l_present},
       {"update_me", l_update_me},
       {"remove_betrayer", l_remove_betrayer},
       // CChar
       {"query_skill", l_query_skill},
       {"set_skill", l_set_skill},
       {"set_beast_verbs", l_set_beast_verbs},
       {"delete_skill", l_delete_skill},
       {"is_fighting", l_is_fighting},
       {"apply_condition", l_apply_condition},
       {"query_condition", l_query_condition},
       {"add_balance", l_add_balance},
       {"attr", l_attr},
       // CChar 扩展
       {"receive_curing", l_receive_curing},
       {"receive_healing", l_receive_healing},
       {"receive_damage", l_receive_damage},
       {"base_attack", l_base_attack},
       {"receive_wound", l_receive_wound},
       {"receive_poison", l_receive_poison},
       {"remove_powerup", l_remove_powerup},
       {"start_powerup", l_start_powerup},
       {"lock_data", l_lockdata},
       {"load_from_server", l_load_from_server},
       {"start_busy", l_start_busy},
       {"is_busy", l_is_busy},
       {"reset_level", l_reset_level},
       {"unconcious", l_unconcious},
       {"die", l_die},
       {"kill_ob", l_kill_ob},
       {"set_killer", l_set_killer},
       {"fight_ob", l_fight_ob},
       {"is_killing", l_is_killing},
       {"query_weapon", l_query_weapon},
       {"query_left_weapon", l_query_left_weapon},
       {"inc_skill", l_inc_skill},
       {"clear", l_clear},
       {"clear_skill", l_clear_skill},
       {"command", l_command},
       {"get_item_list", l_get_item_list},
       {"improve_skill", l_improve_skill},
       {"query_team", l_query_team},
       {"select_member", l_select_member},
       {"skill_map", l_skill_map},
       {"skill_prepare", l_skill_prepare},
       {"dbase", l_dbase},
       {"script_table", l_script_table},
       {"send_state", l_send_state},
       {"can_equipped", l_can_equipped},
       {"enable_player", l_enable_player},
       {"disable_player", l_disable_player},
       {"flush_my_info", l_flush_my_info},
       {"is_fighting_target", l_is_fighting_target},
       {"remove_all_killer", l_remove_all_killer},
       {"unlocked", l_unlocked},
       {"locked", l_locked},
       {"call_out", l_call_out},
       {"remove_call_out", l_remove_call_out},
       {"call_out_combat", l_call_out_combat},
       // CNpc
       {"carry_object", l_carry_object},
       {"carry_good", l_carry_good},
       {"perform_action", l_perform_action},
       {"do_active", l_do_active},
       {"ready_pk", l_ready_pk},
       {"random_move", l_random_move},
       {"add_money", l_add_money},
       {"set_inquiry", l_set_inquiry},
       {"set_inquiry_func", l_set_inquiry_func},
       {"download", l_download},
       {"create_family", l_create_family},
       {"apprentice", l_apprentice},
       {"query_skills", l_query_skills},
       {"wield", l_wield},
       {"wear", l_wear},
       // CItem
       {"do_action_base", l_do_action_base},
       {"query_amount", l_query_amount},
       {"query_weight", l_query_weight},
       {"set_amount", l_set_amount},
       {"split", l_split},
       // CFightRoom
       {"can_join", l_can_join},
       {"join_team", l_join_team},
       {"leave_team", l_leave_team},
       {"end_fight", l_end_fight},
       {"flush_my_skill", l_flush_my_skill},
       {"skill_need_points", l_skill_need_points},
       {"learn_bonus", l_learn_bonus},
       {"set_leader", l_set_leader},
       {"setup", l_setup},
       {"base_init", l_base_init},
       {"base_setup", l_base_setup},
       {"base_reset", l_base_reset},
       {"base_handle_action", l_base_handle_action},
       {"base_handle_action4", l_base_handle_action4},
       {"base_valid_leave", l_base_valid_leave},
       {"base_do_talk", l_base_do_talk},
       {"save_to_local", l_save},
       {"load_from_local", l_load_from_local},
       {"remove_all_doors", l_remove_all_doors},
       {"remove_all_box", l_remove_all_box},
       {"remove", l_remove},
       {"reset", l_room_reset},
       {"begin_fight", l_begin_fight},
       {"set_single", l_set_single},
       {"begin_fb", l_begin_fb},
       {"move_in", l_move_in},
       // CSkill
       {"add_magic", l_add_magic},
       {"valid_force", l_valid_force},
       // CRoom / 地图
       {"present_door", l_present_door},
       {"room_exits", l_room_exits},

       {"init", l_init},
       {"load_fb_room", l_load_fb_room},
       // 容器放置（房间系 npc/door/item/box + 箱子 object）
       {"add_object", l_add_object},
       // CContainer
       {"send_menu", l_send_menu},
       {"add_menu_item", l_add_menu_item},
       {"purge", l_purge},
       {NULL, NULL}};

static const luaL_Reg global_funcs[]
    = {{"load", l_load},
       {"random_npc", l_random_npc},
       {"room_exits", l_room_exits},
       {"find", l_find},
       {"find_online_users", l_find_online_users},
       {"add_user", l_add_user},
       {"do_channel", l_do_channel},
       {"get_db_type", l_get_db_type},
       {"db_query", l_db_query},
       {"db_execute", l_db_execute},
       {"db_register_ddl", l_db_register_ddl},
       {"register_char", l_register_char},
       {"enter_register_hall", l_enter_register_hall},
       {"random_vendor", l_random_vendor},
       {"current_time", l_current_time},
       {"pay_money", l_pay_money},
       {"global_room_name", l_global_room_name},
       {"room_count", l_room_count},
       {"tell_object", l_tell_object},
       {"message", l_message},
       {"notify_fail", l_notify_fail},
       {"random", l_random},
       {"respect", l_respect},
       {"get_ip", l_get_ip},
       {"is", l_is},
       {"combat_attack", l_combat_attack},
       {"get_team_list", l_get_team_list},
       {"get_first_throwing_weapon", l_get_first_throwing_weapon},
       {"throw_weapon", l_throw_weapon},
       {"destruct", l_destruct},
       {"make_corpse", l_make_corpse},
       {"log", l_log},
       {"xrandom", l_xrandom},
       {"combatd_query", l_combatd_query},
       {"killer_reward", l_killer_reward},
       {"get_fight_team_num", l_get_fight_team_num},
       {"get_fight_team_char", l_get_fight_team_char},
       {"query_recruit", l_query_recruit},
       {"update_recruit", l_update_recruit},
       {"do_emote", l_do_emote},
       {"do_bishi", l_do_bishi},
       {"revive", l_revive},
       {"setup_char", l_setup_char},
       {"iter_items", l_iter_items},
       {"paihangbang", l_paihangbang},
       {"script_exists", l_script_exists},
       {"ini_get", l_ini_get},
       {NULL, NULL}};

//---------------------------------------------------------
// 启动期前导脚本（prelude）
//---------------------------------------------------------
// <script_root|default_script_root>/data/prelude.lua：script_root 优先，
// 内容层可用同名脚本整体覆盖默认根。以 jianghu_object 元表为唯一入参执行
// （该元表 __index 指向自身，脚本内 `local index = ...` 即方法表），脚本
// 在全局环境定义的函数即全局 API。文件不存在试下一个根；文件在但读/载/
// 执行失败记 ERROR 并停止——用户根的问题不被默认根掩盖。
static bool prelude_run_script(lua_State* L, const char* root)
{
    if (!root || !root[0]) return false;

    std::string path = std::string(root) + "/data/prelude.lua";
    std::replace(path.begin(), path.end(), '\\', '/');

    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;

    std::string gbk = load_file_as_gbk(path.c_str());
    if (gbk.empty()) {
        LOG_ERROR("[LuaBind] read %s failed", path.c_str());
        return true;
    }
    if (luaL_loadbuffer(L, gbk.c_str(), gbk.size(), path.c_str()) != LUA_OK) {
        LOG_ERROR("[LuaBind] load %s failed: %s", path.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
        return true;
    }
    luaL_getmetatable(L, "jianghu_object");
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        LOG_ERROR("[LuaBind] run %s failed: %s", path.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return true;
}

// 装载前导脚本；两根都未配置（配置未加载的裸状态）静默跳过。
static void lua_bind_run_prelude(lua_State* L)
{
    const char* roots[2] = {ini_get("lua", "script_root", ""),
                            ini_get("lua", "default_script_root", "")};
    bool configured      = roots[0][0] != '\0' || roots[1][0] != '\0';
    if (prelude_run_script(L, roots[0])) return;
    if (prelude_run_script(L, roots[1])) return;
    if (configured)
        LOG_ERROR("[LuaBind] data/prelude.lua missing in all script roots");
}

void lua_bind_init(lua_State* L)
{
    LOG_DEBUG("Lua Bind Initializing...");
    // 创建 metatable
    luaL_newmetatable(L, "jianghu_object");

    // metatable.__index = object_methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, object_methods, 0);

    // 弹出 metatable
    lua_pop(L, 1);

    // 物品列表 userdata 类型标识（iter_items 的 luaL_checkudata 依赖此名字）
    luaL_newmetatable(L, "jianghu_item_list");
    lua_pop(L, 1);

    // 注册全局函数到全局环境
    lua_pushglobaltable(L);
    luaL_setfuncs(L, global_funcs, 0);
    lua_pop(L, 1);

    // 注册全局颜色码表 g_Color（对应 g_Color[13]）
    lua_createtable(L, 13, 0);
    for (int i = 0; i < 13; i++) {
        lua_pushstring(L, g_color[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setglobal(L, "g_Color");

    // 注册枚举常量到全局环境
    // 禁止在 Lua 脚本中使用数字替代枚举！统一用这些常量名
    // 各模块条目数组维护在枚举所属头文件对应的源文件中（见 LuaBindEnums.h）
    lua_pushglobaltable(L);
    // 起始房间（从配置文件读取）
    lua_pushstring(L, g_start_room);
    lua_setfield(L, -2, "START_ROOM");
    lua_pop(L, 1);

    lua_bind_reg_object_attr_enums(L);
    lua_bind_reg_char_enums(L);
    lua_bind_reg_skill_enums(L);
    lua_bind_reg_combat_enums(L);
    lua_bind_reg_wield_enums(L);
    lua_bind_reg_fight_room_enums(L);

    // 钩子机制的全局 hooks/commands 表（幂等，不清已有内容）
    lua_hook_init_tables(L);

    // 内容层前导脚本：紧随绑定注册，晚于钩子表（prelude 可读 hooks/commands）
    lua_bind_run_prelude(L);

    LOG_DEBUG("Lua Bind Finished.");
}
