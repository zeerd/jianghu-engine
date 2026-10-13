/**
 * @file NpcMgr.cpp
 * @brief NPC 管理  */

#include "NpcMgr.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "Npc.h"
#include "Vendor.h"
#include "../util/Logger.h"

//---------------------------------------------------------
// NPC 加载
//---------------------------------------------------------
std::unique_ptr<CNpc> create_npc(string base_name);
CNpc* load_npc(const char* base_name)
{
    std::unique_ptr<CNpc> npc = create_npc(base_name);
    if (!npc) {
        LOG_ERROR("load_npc: Create_NPC failed for %s", base_name);
        return NULL;
    }

    npc->set("base_name", base_name);
    npc->set("infinity", 1);

    npc->create();
    npc->setup();
    npc->del("infinity");

    CNpc* obj                    = npc.release();
    g_npc_list[obj->object_id()] = obj;
    if (obj->query_attr(CMainObject::IS_VENDOR)) {
        g_vendor_list[obj->object_id()] = reinterpret_cast<CVendor*>(obj);
    }

    return obj;
}

//---------------------------------------------------------
// NPC 释放
//---------------------------------------------------------
void npc_release()
{
    iterator_npc p;
    CNpc* npc;

    for (p = g_npc_list.begin(); p != g_npc_list.end();) {
        npc           = p->second;
        int32_t npcid = p->first;

        npc->set_destructed();
        p++;

        npc->set_heart_beat(0);
        npc->move(NULL);

        if (g_last_destruct_object == npcid) continue;

        npc->clean_up();
        delete npc;
    }

    g_npc_list.clear();
}

//---------------------------------------------------------
// NPC 查找 / 删除
//---------------------------------------------------------
void remove_npc(int32_t id) { g_npc_list.erase(id); }

CNpc* find_npc(int32_t id)
{
    iterator_npc p;

    p = g_npc_list.find(id);
    if (p == g_npc_list.end()) return 0;
    return p->second;
}

//---------------------------------------------------------
// 随机 NPC
//---------------------------------------------------------
CNpc* random_npc()
{
    MAP2NPC::iterator p2;
    int i = 0;

    int index = g_utils.random(g_npc_list.size());

    for (p2 = g_npc_list.begin(); p2 != g_npc_list.end(); p2++) {
        CNpc* npc = p2->second;
        if (i == index) return npc;
        i++;
    }

    return 0;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// load_npc(name) -> object
int l_load_npc(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    lua_bind_push_object(L, load_npc(name));
    return 1;
}

// random_npc() -> object - 随机选取一个 NPC
int l_random_npc(lua_State* L)
{
    extern CNpc* random_npc();
    lua_bind_push_object(L, random_npc());
    return 1;
}

// find_npc(id) -> object - 按对象ID查找 NPC
int l_find_npc(lua_State* L)
{
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    lua_bind_push_object(L, find_npc(id));
    return 1;
}
