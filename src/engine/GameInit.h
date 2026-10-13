/**
 * @file GameInit.h
 * @brief 游戏初始化/关闭/服务器配置变量。
 */
#pragma once

// --- 服务器配置变量 ---
extern int g_server_id;           // 服务器编号	1-999
extern int g_server_port;         // 服务器端口号  6600
extern char g_server_name[64];    // 服务器名称
extern int g_server_charge;       // 是否收费
extern char g_server_dbname[64];  // 数据库名称
extern char g_start_room[128];    // 起始房间
extern int g_ip_limit;            // 每 IP 最大连接数

// --- 时间变量 ---
extern int g_current_time;     // 当前时间
extern int g_heart_beat_flag;  // 心跳标志
extern int g_boot_time;        // 启动时间

// --- 游戏生命周期 ---
int init_game(const char* ipaddress, const char* name, const char* password);
void exit_game();
void init_lua_engine();
void dis_connect_database();
