/**
 * @file Login.h
 * @brief 登录服务 umbrella header，汇总认证/注册/登录流程接口。
 */

#pragma once

#include "LoginAuth.h"
#include "LuaLogin.h"
#include "LoginRegister.h"

#include <cstdint>

class CUser;

// --- 登录主流程 ---
int on_login(uint32_t serial, int sockfd, char* arg);
void on_send_online(CUser* me);

// --- 游客登录（guest/guest 进游戏大厅 rk） ---
extern int g_guest_account;
bool is_guest_login(const char* nid, const char* passwd);
long alloc_guest_account();
void setup_guest_user(CUser* me);

// --- 在线管理 ---
void on_offline(long objectid);
bool splite_command_parse(long userid, char* command_string, int ret);
CUser* find_online(const char* id);
void send_info_to_client(CUser* me);
