/**
 * @file RoomMgr.h
 * @brief 房间加载与管理。
 */
#pragma once

#include <string>

class CRoom;

using std::string;

// --- 房间加载 ---
CRoom* load_room(const char* base_name);

// --- 房间管理（内部） ---
void room_release();
void remove_room(string room_name);
void draw_map();
