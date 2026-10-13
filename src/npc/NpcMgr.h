/**
 * @file NpcMgr.h
 * @brief NPC 加载与管理。
 */
#pragma once

#include <cstdint>
#include <string>

class CNpc;

using std::string;

// --- NPC 加载 ---
CNpc* load_npc(const char* base_name);

// --- NPC 查找 ---
CNpc* find_npc(int32_t id);
CNpc* random_npc();

// --- NPC 管理（内部） ---
void npc_release();
void remove_npc(int32_t id);
