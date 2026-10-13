/** @file LuaBind.cpp @brief C++ 对象到 Lua 的绑定实现 */

#include <windows.h>

#include "LuaBind.h"
#include "LuaEngine.h"

#include <lua.hpp>

#include "Char.h"
#include "Global.h"

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

CMainObject* lua_bind_check_object(lua_State* L, int idx)
{
    return get_object(L, idx);
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
       {"Query", l_query},
       {"querystr", l_querystr},
       {"add", l_add},
       {"del", l_del},
       {"set_name", l_set_name},
       {"set_temp", l_set_temp},
       {"set_heart_beat", l_set_heart_beat},
       {"create", l_create},
       {"query_temp", l_query_temp},
       {"querymap_temp", l_querymap_temp},
       {"querymap_temp_keys", l_querymap_temp_keys},
       {"querystr_temp", l_querystr_temp},
       {"name", l_name},
       {"id", l_id},
       {"object_id", l_object_id},
       {"Long", l_long},
       {"Short", l_short},
       {"title", l_title},
       // CContainer
       {"say", l_say},
       {"set_weight", l_set_weight},
       {"move", l_move},
       {"environment", l_environment},
       {"query_fb_parent", l_query_fb_parent},
       {"present", l_present},
       {"Present", l_present_by_id},
       {"Get_ItemList", l_get_item_list},
       {"UpdateMe", l_update_me},
       {"Remove_Betrayer", l_remove_betrayer},
       // CChar
       {"query_skill", l_query_skill},
       {"mini_skill", l_mini_skill},
       {"set_skill", l_set_skill},
       {"delete_skill", l_delete_skill},
       {"is_fighting", l_is_fighting},
       {"apply_condition", l_apply_condition},
       {"query_condition", l_query_condition},
       {"clear_condition", l_clear_condition},
       {"add_balance", l_add_balance},
       // CChar 扩展
       {"receive_curing", l_receive_curing},
       {"receive_healing", l_receive_healing},
       {"receive_damage", l_receive_damage},
       {"base_attack", l_base_attack},
       {"receive_wound", l_receive_wound},
       {"receive_poison", l_receive_poison},
       {"remove_powerup", l_remove_powerup},
       {"start_powerup", l_start_powerup},
       {"Save", l_save},
       {"Load", l_load},
       {"LockData", l_lockdata},
       {"LoadFromServer", l_load_from_server},
       {"SaveToServer", l_save_to_server},
       {"Offline", l_offline},
       {"start_busy", l_start_busy},
       {"is_busy", l_is_busy},
       {"reset_level", l_reset_level},
       {"unconcious", l_unconcious},
       {"die", l_die},
       {"die_base", l_die_base},
       {"kill_ob", l_kill_ob},
       {"set_killer", l_set_killer},
       {"fight_ob", l_fight_ob},
       {"is_killing", l_is_killing},
       {"query_weapon", l_query_weapon},
       {"query_left_weapon", l_query_left_weapon},
       {"query_dex", l_query_dex},
       {"query_combat_exp", l_query_combat_exp},
       {"query_skill_mapped", l_query_skill_mapped},
       {"query_skill_map_list", l_query_skill_map_list},
       {"query_hardcore_skills", l_query_hardcore_skills},
       {"inc_skill", l_inc_skill},
       {"query_skill_prepared", l_query_skill_prepared},
       {"query_skill_prepare_list", l_query_skill_prepare_list},
       {"clear", l_clear},
       {"clear_skill", l_clear_skill},
       {"clear_skill_map", l_clear_skill_map},
       {"clear_skill_prepare", l_clear_skill_prepare},
       {"get_map", l_get_map},
       {"set_map", l_set_map},
       {"command", l_command},
       {"object_id", l_object_id},
       {"get_item_list", l_get_item_list},
       {"present_name", l_present_name},
       {"catch_ob", l_catch_ob},
       {"improve_skill", l_improve_skill},
       {"query_team", l_query_team},
       {"Select_Member", l_select_member},
       {"wield_weapon", l_wield_weapon},
       {"unwield_weapon", l_unwield_weapon},
       {"map_skill", l_map_skill},
       {"prepare_skill", l_prepare_skill},
       {"queryvec", l_queryvec},
       {"set_verbs", l_set_verbs},
       {"add_temp", l_add_temp},
       {"delete_temp", l_delete_temp},
       {"querymap_temp", l_querymap_temp},
       {"querymap_temp_keys", l_querymap_temp_keys},
       {"querymap", l_querymap},
       {"querymap_keys", l_querymap_keys},
       {"dbase_defined", l_dbase_defined},
       {"SendState", l_send_state},
       {"is_character", l_is_character},
       {"is_char", l_is_character},
       {"can_equipped", l_can_equipped},
       {"enable_player", l_enable_player},
       {"disable_player", l_disable_player},
       {"FlushMyInfo", l_flush_my_info},
       {"query_apply", l_query_apply},
       {"query_str", l_query_str},
       {"query_con", l_query_con},
       {"query_int", l_query_int},
       {"query_per", l_query_per},
       {"is_fighting_target", l_is_fighting_target},
       {"is_invalid", l_is_invalid},
       {"remove_all_killer", l_remove_all_killer},
       {"UnLocked", l_unlocked},
       {"Locked", l_locked},
       {"call_out", l_call_out},
       {"remove_call_out", l_remove_call_out},
       {"call_out_combat", l_call_out_combat},
       // CNpc
       {"carry_object", l_carry_object},
       {"carry_good", l_carry_good},
       {"perform_action", l_perform_action},
       {"make_mirror", l_make_mirror},
       {"DoActive", l_do_active},
       {"ready_pk", l_ready_pk},
       {"random_move", l_random_move},
       {"add_money", l_add_money},
       {"query_valid_exp", l_query_valid_exp},
       {"set_inquiry", l_set_inquiry},
       {"set_inquiry_func", l_set_inquiry_func},
       {"DownLoad", l_download},
       {"create_family", l_create_family},
       {"assign_apprentice", l_assign_apprentice},
       {"query_exp", l_query_exp},
       {"query_skills", l_query_skills},
       {"recruit_num", l_recruit_num},
       {"release_master", l_release_master},
       {"is_apprentice_of", l_is_apprentice_of},
       {"recruit_apprentice", l_recruit_apprentice},
       {"recognize_apprentice", l_recognize_apprentice},
       {"prevent_learn", l_prevent_learn},
       {"attempt_apprentice", l_attempt_apprentice},
       {"wield", l_wield},
       {"wear", l_wear},
       {"wield_ownered_item", l_wield_ownered_item},
       // CItem
       {"do_use", l_do_use},
       {"Do_Wield", l_do_wield},
       {"Do_UnWield", l_do_unwield},
       {"do_action_base", l_do_action_base},
       {"Query_Amount", l_query_amount},
       {"Query_Weight", l_query_weight},
       {"set_amount", l_set_amount},
       {"Split", l_split},
       {"PresentName", l_present_name},
       // CFightRoom
       {"Can_Join", l_can_join},
       {"Join_Team", l_join_team},
       {"Leave_Team", l_leave_team},
       {"End_Fight", l_end_fight},
       {"FlushMySkill", l_flush_my_skill},
       {"skill_need_points", l_skill_need_points},
       {"learn_bonus", l_learn_bonus},
       {"set_leader", l_set_leader},
       {"setup", l_setup},
       {"base_create", l_base_create},
       {"base_init", l_base_init},
       {"base_setup", l_base_setup},
       {"base_reset", l_base_reset},
       {"base_handle_action", l_base_handle_action},
       {"base_handle_action4", l_base_handle_action4},
       {"base_valid_leave", l_base_valid_leave},
       {"base_do_talk", l_base_do_talk},
       {"save_to_local", l_save},
       {"load_from_local", l_load},
       {"remove_all_doors", l_remove_all_doors},
       {"remove_doors", l_remove_doors},
       {"reset", l_room_reset},
       {"Begin_Fight", l_begin_fight},
       {"set_single", l_set_single},
       {"Begin_FB", l_begin_fb},
       {"MoveIn", l_move_in},
       {"is_fight_room", l_is_fight_room},
       // CSkill
       {"add_magic", l_add_magic},
       // CRoom / 地图
       {"add_npc", l_add_npc},
       {"add_door", l_add_door},
       {"bind", l_door_bind},
       {"PresentDoor", l_present_door},
       {"room_exits", l_room_exits},

       {"add_item", l_add_item},
       {"Add_Box", l_add_box},
       {"init", l_init},
       {"load_fb_room", l_load_fb_room},
       // CBox
       {"add_object", l_add_object},
       // CContainer
       {"SendMenu", l_send_menu},
       {"AddMenuItem", l_add_menu_item},
       {"purge", l_purge},
       {NULL, NULL}};

static const luaL_Reg global_funcs[]
    = {{"load_npc", l_load_npc},
       {"load_item", l_load_item},
       {"find_item", l_find_item},
       {"random_npc", l_random_npc},
       {"get_my_house", l_get_my_house},
       {"load_room", l_load_room},
       {"load_fb", l_load_fb},
       {"load_skill", l_load_skill},
       {"room_exits", l_room_exits},
       {"find_object", l_find_object},
       {"find_npc", l_find_npc},
       {"find_player", l_find_player},
       {"find_online", l_find_online},
       {"get_all_online_users", l_get_all_online_users},
       {"add_user", l_add_user},
       {"do_channel", l_do_channel},
       {"sql_update_record", l_sql_update_record},
       {"update_mf_record", l_update_mf_record},
       {"db_query", l_db_query},
       {"db_execute", l_db_execute},
       {"db_register_ddl", l_db_register_ddl},
       {"drop_all", l_drop_all},
       {"random_vendor", l_random_vendor},
       {"draw_map", l_draw_map},
       {"current_time", l_current_time},
       {"pay_player", l_pay_player},
       {"player_pay", l_player_pay},
       {"global_room_name", l_global_room_name},
       {"room_count", l_room_count},
       {"tell_object", l_tell_object},
       {"message_vision", l_message_vision},
       {"message_vision_self", l_message_vision_self},
       {"notify_fail", l_notify_fail},
       {"random", l_random},
       {"make_random_yewai", l_make_random_yewai_global},
       {"load_room", l_load_room},
       {"load_npc", l_load_npc},
       {"RandomGetItem", l_random_get_item},
       {"query_respect", l_query_respect},
       {"query_rude", l_query_rude},
       {"query_self_rude", l_query_self_rude},
       {"query_self", l_query_self},
       {"check_legal_name", l_check_legal_name},
       {"valid_string", l_valid_string},
       {"GetIP", l_get_ip},
       {"skill2level", l_skill2level},
       {"level2skill", l_level2skill},
       {"Level2Exp", l_level2exp},
       {"exp2level", l_exp2level},
       {"userp", l_userp},
       {"MessageVision", l_message_vision_channel},
       {"message_channel", l_message_channel},
       {"combat_attack", l_combat_attack},
       {"combat_attack_unarmed", l_combat_attack_unarmed},
       {"combat_attack_weapon", l_combat_attack_weapon},
       {"damge_to_string", l_damge_to_string},
       {"eff_status_msg", l_eff_status_msg},
       {"chinese_number", l_chinese_number},
       {"get_team_list", l_get_team_list},
       {"get_first_throwing_weapon", l_get_first_throwing_weapon},
       {"throw_weapon", l_throw_weapon},
       {"wizardp", l_wizardp},
       {"is_relative", l_is_relative},
       {"living", l_living},
       {"tell_room", l_tell_room},
       {"destruct", l_destruct},
       {"current_time_info", l_current_time_info},
       {"MakeCorpse", l_make_corpse},
       {"log_trace", l_log_trace},
       {"log_debug", l_log_debug},
       {"log_info", l_log_info},
       {"log_warn", l_log_warn},
       {"log_error", l_log_error},
       {"log_fatal", l_log_fatal},
       {"xrandom", l_xrandom},
       {"combatd_query", l_combatd_query},
       {"killer_reward", l_killer_reward},
       {"get_server_id", l_get_server_id},
       {"get_fight_team_num", l_get_fight_team_num},
       {"get_fight_team_char", l_get_fight_team_char},
       {"get_no_force_conflict", l_get_no_force_conflict},
       {"get_room_char", l_get_room_char},
       {"get_newlife_no_conflict", l_get_newlife_no_conflict},
       {"query_recruit", l_query_recruit},
       {"update_recruit", l_update_recruit},
       {"do_emote", l_do_emote},
       {"Do_BiShi", l_do_bishi},
       {"handle_action", l_handle_action},
       {"revive", l_revive},
       {"setup_char", l_setup_char},
       {"iter_items", l_iter_items},
       {"paihangbang", l_paihangbang},
       {"script_exists", l_script_exists},
       {"ini_get", l_ini_get},
       {NULL, NULL}};

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

    LOG_DEBUG("Lua Bind Finished.");
}
