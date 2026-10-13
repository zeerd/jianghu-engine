/** @file LuaQuest.cpp @brief 「仗剑江湖 / 乌海江湖」商人任务的 C++ → Lua
 * 薄钩子实现 每个函数：require("quest.quest_mgr") → 取字段 → 压参数 → pcall。
 * 全部业务逻辑在 quest_mgr.lua，此处仅做事件转发。 */

#include "LuaQuest.h"

#include "Global.h"
#include "Char.h"
#include "Container.h"
#include "Npc.h"

#include "LuaEngine.h"
#include "LuaBind.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// 把 quest_mgr 模块表压入栈顶（require 结果缓存在 package.loaded，开销极小）。
// 成功返回 true（栈：... [table]），失败返回 false（栈已平衡）。
// 任务系统允许缺席（不同 lua 数据集可不提供 quest_mgr），缺席时静默返回 false，
// 不打错误日志——C++ 不假设任何具体任务系统一定存在。
static bool push_quest_mgr(lua_State* L)
{
    lua_getglobal(L, "require");
    lua_pushstring(L, "quest.quest_mgr");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);  // 静默：quest_mgr 不存在是合法状态
        return false;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// 通用谓词：玩家身上是否有进行中的任务。键名/存在性全由 Lua 决定。
bool quest_has_active(CChar* me)
{
    if (!me) return false;
    lua_State* L = g_LuaEngine->get_state();
    if (!push_quest_mgr(L)) return false;  // quest_mgr 不存在 → 无任务
    lua_getfield(L, -1, "has_quest");      // [table][func]
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_bind_push_object(L, me);  // [table][func][me]
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("quest: has_quest failed: %s", lua_tostring(L, -1));
        lua_pop(L, 2);  // err + table
        return false;
    }
    bool active = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);  // result
    lua_pop(L, 1);  // table
    return active;
}

int quest_accept_object(CNpc* npc, CChar* me, CContainer* item)
{
    if (!npc || !me || !item) return 0;
    lua_State* L = g_LuaEngine->get_state();
    if (!push_quest_mgr(L)) return 0;
    lua_getfield(L, -1, "on_give_item");  // [table][func]
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_bind_push_object(L, npc);   // [table][func][npc]
    lua_bind_push_object(L, me);    // [table][func][npc][me]
    lua_bind_push_object(L, item);  // [table][func][npc][me][item]
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("quest: on_give_item failed: %s", lua_tostring(L, -1));
        lua_pop(L, 2);  // err + table
        return 0;
    }
    int result = lua_isinteger(L, -1) ? (int)lua_tointeger(L, -1) : 0;
    lua_pop(L, 1);  // result
    lua_pop(L, 1);  // table
    return result;
}

bool quest_do_talk(CNpc* npc, CChar* me, char* ask)
{
    if (!npc || !me || !ask) return false;
    lua_State* L = g_LuaEngine->get_state();
    if (!push_quest_mgr(L)) return false;
    lua_getfield(L, -1, "on_talk");  // [table][func]
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_bind_push_object(L, npc);  // [table][func][npc]
    lua_bind_push_object(L, me);   // ...[me]
    lua_pushstring(L, ask);        // ...[ask]
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("quest: on_talk failed: %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return false;
    }
    bool handled = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);  // result
    lua_pop(L, 1);  // table
    return handled;
}

void quest_npc_killed(CChar* killer, CChar* victim)
{
    if (!killer || !victim) return;
    lua_State* L = g_LuaEngine->get_state();
    if (!push_quest_mgr(L)) return;
    lua_getfield(L, -1, "on_npc_killed");  // [table][func]
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    lua_bind_push_object(L, killer);  // [table][func][killer]
    lua_bind_push_object(L, victim);  // ...[victim]
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("quest: on_npc_killed failed: %s", lua_tostring(L, -1));
        lua_pop(L, 1);  // err
    }
    lua_pop(L, 1);  // [table]
}
