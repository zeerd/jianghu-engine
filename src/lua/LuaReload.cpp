/** @file LuaReload.cpp @brief Lua 实例热重载注册表的非模板实现 NPC/Item/Room
 * 的实例注册表和 NotifyReload 函数集中在此文件编译， 避免 .inl 中的 inline
 * 定义在未被 include 的编译单元中产生链接错误。 */

#include <windows.h>

#include "LuaInstanceRegistry.h"

#include <vector>

//---------------------------------------------------------
// NPC 实例注册表
//---------------------------------------------------------

std::vector<CLuaNpcInstanceEntry>& CLuaNpcInstanceRegistry::instances()
{
    static std::vector<CLuaNpcInstanceEntry> s_instances;
    return s_instances;
}

void CLuaNpcInstanceRegistry::register_entry(void* p, const char* name,
                                             NpcReloadFunc fn)
{
    instances().push_back({p, name ? name : "", fn});
}

void CLuaNpcInstanceRegistry::unregister(void* p)
{
    std::vector<CLuaNpcInstanceEntry>& v = instances();
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it->ptr == p) {
            v.erase(it);
            return;
        }
    }
}

void lua_npc_notify_reload(const char* scriptName)
{
    if (!scriptName || !scriptName[0]) return;
    std::vector<CLuaNpcInstanceEntry> snapshot
        = CLuaNpcInstanceRegistry::instances();
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (snapshot[i].scriptName == scriptName)
            snapshot[i].reloadFunc(snapshot[i].ptr);
    }
}

//---------------------------------------------------------
// Item 实例注册表
//---------------------------------------------------------

std::vector<CLuaItemInstanceEntry>& CLuaItemInstanceRegistry::instances()
{
    static std::vector<CLuaItemInstanceEntry> s_instances;
    return s_instances;
}

void CLuaItemInstanceRegistry::register_entry(void* p, const char* name,
                                              ItemReloadFunc fn)
{
    instances().push_back({p, name ? name : "", fn});
}

void CLuaItemInstanceRegistry::unregister(void* p)
{
    std::vector<CLuaItemInstanceEntry>& v = instances();
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it->ptr == p) {
            v.erase(it);
            return;
        }
    }
}

void lua_item_notify_reload(const char* scriptName)
{
    if (!scriptName || !scriptName[0]) return;
    std::vector<CLuaItemInstanceEntry> snapshot
        = CLuaItemInstanceRegistry::instances();
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (snapshot[i].scriptName == scriptName)
            snapshot[i].reloadFunc(snapshot[i].ptr);
    }
}

//---------------------------------------------------------
// Room 实例注册表
//---------------------------------------------------------

std::vector<CLuaRoomInstanceEntry>& CLuaRoomInstanceRegistry::instances()
{
    static std::vector<CLuaRoomInstanceEntry> s_instances;
    return s_instances;
}

void CLuaRoomInstanceRegistry::register_entry(void* p, const char* name,
                                              RoomReloadFunc fn)
{
    instances().push_back({p, name ? name : "", fn});
}

void CLuaRoomInstanceRegistry::unregister(void* p)
{
    std::vector<CLuaRoomInstanceEntry>& v = instances();
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it->ptr == p) {
            v.erase(it);
            return;
        }
    }
}

void lua_room_notify_reload(const char* scriptName)
{
    if (!scriptName || !scriptName[0]) return;
    std::vector<CLuaRoomInstanceEntry> snapshot
        = CLuaRoomInstanceRegistry::instances();
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (snapshot[i].scriptName == scriptName)
            snapshot[i].reloadFunc(snapshot[i].ptr);
    }
}
