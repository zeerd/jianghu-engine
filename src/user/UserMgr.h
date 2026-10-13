/**
 * @file UserMgr.h
 * @brief 用户查找与管理。
 */
#pragma once

#include <cstdint>

class CUser;

// --- 用户查找 ---
CUser* find_player(int32_t id);

// --- 用户管理（内部） ---
CUser* add_user();
void user_release();
void remove_user(int32_t id);
void user_command_loop();
