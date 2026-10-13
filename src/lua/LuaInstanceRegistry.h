/**
 * @file LuaInstanceRegistry.h
 * @brief Lua 实例热重载注册表，集中管理 NPC/Item/Room 实例的热重载通知。
 */

//---------------------------------------------------------

#pragma once

#include <string>
#include <vector>

//---------------------------------------------------------
// NPC 实例注册表
//---------------------------------------------------------

typedef void (*NpcReloadFunc)(void*);

struct CLuaNpcInstanceEntry {
    void* ptr = nullptr;
    std::string scriptName;
    NpcReloadFunc reloadFunc = nullptr;
};

struct CLuaNpcInstanceRegistry {
    static std::vector<CLuaNpcInstanceEntry>& instances();
    static void register_entry(void* p, const char* name, NpcReloadFunc fn);
    static void unregister(void* p);
};

void lua_npc_notify_reload(const char* scriptName);

//---------------------------------------------------------
// Item 实例注册表
//---------------------------------------------------------

typedef void (*ItemReloadFunc)(void*);

struct CLuaItemInstanceEntry {
    void* ptr = nullptr;
    std::string scriptName;
    ItemReloadFunc reloadFunc = nullptr;
};

struct CLuaItemInstanceRegistry {
    static std::vector<CLuaItemInstanceEntry>& instances();
    static void register_entry(void* p, const char* name, ItemReloadFunc fn);
    static void unregister(void* p);
};

void lua_item_notify_reload(const char* scriptName);

//---------------------------------------------------------
// Room 实例注册表
//---------------------------------------------------------

typedef void (*RoomReloadFunc)(void*);

struct CLuaRoomInstanceEntry {
    void* ptr = nullptr;
    std::string scriptName;
    RoomReloadFunc reloadFunc = nullptr;
};

struct CLuaRoomInstanceRegistry {
    static std::vector<CLuaRoomInstanceEntry>& instances();
    static void register_entry(void* p, const char* name, RoomReloadFunc fn);
    static void unregister(void* p);
};

void lua_room_notify_reload(const char* scriptName);
