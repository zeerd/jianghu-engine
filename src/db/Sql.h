/**
 * @file Sql.h
 * @brief 数据库操作封装接口（容器/玩家/箱子/账号 CRUD）。
 */

#pragma once

#include <string>
#include "IDatabase.h"

// 前向声明
class CChar;
class CUser;
class CContainer;

// ============================================================
// 数据库操作封装接口
// ============================================================

// 获取数据库类型 ("mysql", "sqlite", 或 "none")
const char* get_db_type();

// 基础数据库操作
bool select_game_database();  // 选择游戏数据库
bool init_sql_server(const char* ipaddress, const char* name,
                     const char* password);

// 数据集专用表：脚本开机注册的建表语句清单（C++ 不认识任何具体表名，
// prepare_tables 末尾逐条执行）。由 data/*.lua 经 db_register_ddl 填充。
extern std::vector<std::string> g_dataset_table_ddl;

bool restore_data_table();
bool verify_user(const char* userid, char* passwd);
bool ensure_wizard_accounts();
void disconnect_sql_server();

// ------------------------------------------------------------
// 容器/物品/房间/NPC 存储操作 (对应 Container.cpp)
// ------------------------------------------------------------
bool db_save_container(const char* table, const char* obj_id,
                       const char* obj_name, const char* obj_data,
                       bool is_update);
bool db_load_container(const char* table, const char* obj_id,
                       std::string& out_data);
bool db_delete_container(const char* table, const char* obj_id);
bool db_lock_container(const char* table, const char* obj_id, int locked,
                       int& out_current_lock);

// ------------------------------------------------------------
// 玩家物品存储操作 (对应 Char.cpp - t_saveobj)
// ------------------------------------------------------------
bool db_save_object(const char* owner_id, long grid, const char* base_name,
                    const char* savedata);
bool db_load_object(const char* owner_id, long grid, std::string& out_base_name,
                    std::string& out_savedata);

// ------------------------------------------------------------
// 箱子存储操作 (对应 Box.cpp - t_box)
// ------------------------------------------------------------
bool db_save_box(const char* obj_id, const char* obj_name, const char* obj_data,
                 const char* room_id, int grid);
bool db_load_box(const char* obj_id, std::string& out_data);
bool db_delete_box(const char* obj_id);
bool db_load_room_boxes(const char* room_base_name,
                        std::vector<std::string>& out_data);

// ------------------------------------------------------------
// 用户/账号操作 (对应 Login.cpp, User.cpp, Char.cpp)
// ------------------------------------------------------------
bool db_verify_password(const char* id, const char* pass);
bool db_change_password(const char* id, const char* new_pass);
bool db_change_verify_password(const char* id, const char* new_pass);
bool db_set_online_flag(const char* id, int online);
int db_query_server(const char* id);
int db_query_freeze(const char* id);
bool db_set_freeze(const char* id, int freeze);

// ------------------------------------------------------------
// 频道/社交操作 (对应 Channel.cpp)
// ------------------------------------------------------------
bool db_query_recruit(const char* master_id, DbResultSet& result);
bool db_update_recruit(const char* master, const char* id, const char* name,
                       long exp, int apprentice);
bool db_query_relative(const char* me, const char* target, long& out_point,
                       int& out_relative);
bool db_update_relative(const char* me, const char* target, long pt, int flag);

// ------------------------------------------------------------
// 排行榜
// ------------------------------------------------------------
int paihangbang(CContainer* me);

// ------------------------------------------------------------
// 游戏卡/麻将操作 (对应 sql_jx.cpp, sql_mj.cpp)
// ------------------------------------------------------------
bool register_card(const char* card, const char* id);
bool unregister_card(const char* card, const char* id);
bool create_card(CChar* me);
bool show_cards(CChar* me);

bool verify_id(const char* id, char* endtime);

bool sql_update_record(const char* id, int type, int level, const char* data,
                       bool update = false);
