// test_bind_common.h — shared helpers for testing the scattered l_* Lua binds.
//
// Strategy: spin up a bare lua_State, run LuaBind_Init (creates the
// "jianghu_object" metatable), push a real C++ object as `self`, then call the
// bind function directly (they are declared in LuaBindDecls.h). Read the
// pushed return values off the stack.
//
#pragma once

#include "pch.h"

#include <lua.hpp>
#include <algorithm>
#include <mutex>
#include <vector>

#include "Char.h"
#include "Container.h"
#include "HeartBeat.h"
#include "Item.h"
#include "Npc.h"
#include "User.h"
#include "ItemMgr.h"
#include "mock/mock_itemmgr.h"
#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaBindDecls.h"
#include "test_lua_env.h"

// RAII helper: install a g_ItemMgr factory that hands out real CItem objects
// (so carry_object / add_money / Add_Box do not deref a null load_item result).
// Tracks allocated items and deletes them on scope exit.
class ItemFactory {
public:
    explicit ItemFactory(CMockItemMgr* mgr) : m_mgr(mgr)
    {
        m_saved             = g_item_mgr;
        m_mgr->load_factory = &ItemFactory::Create;
        g_item_mgr          = m_mgr;
    }
    ~ItemFactory()
    {
        g_item_mgr          = m_saved;
        m_mgr->load_factory = nullptr;

        // 业务方（如 CVendor::Add_Good）会用 DESTRUCT_OB 把对象移交给延迟销毁
        // 队列，由主循环冲刷；测试进程没有主循环，工厂收回这些对象的所有权，
        // 避免工厂与延迟队列重复销毁同一对象。
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                if (std::find(s_created.begin(), s_created.end(), *it)
                    != s_created.end()) {
                    it = g_destruct_list.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        for (auto* p : s_created) delete p;
        s_created.clear();
    }
    static CContainer* Create(const char*)
    {
        CItem* it = new CItem();
        it->set("id", "item");
        it->set("name", "item");
        s_created.push_back(it);
        return it;
    }

private:
    static inline std::vector<CContainer*> s_created;
    CMockItemMgr* m_mgr;
    IItemMgr* m_saved;
};

class BindTest : public ::testing::Test {
protected:
    lua_State* L = nullptr;

    void SetUp() override
    {
        L = luaL_newstate();
        ASSERT_NE(L, nullptr);
        luaL_openlibs(L);
        lua_bind_init(L);
    }
    void TearDown() override
    {
        if (L) lua_close(L);
    }

    // push object as self (stack index 1 for the bind call)
    void PushSelf(CMainObject* o) { lua_bind_push_object(L, o); }

    // Call `fn` with the top `nargs` stack slots as its arguments, protected by
    // lua_pcall so that any luaL_check* error inside the bind is caught (and
    // the error message popped) instead of aborting the process. Returns the
    // number of values the bind left on the stack, or -1 if it raised an error.
    int call(int (*fn)(lua_State*), int nargs)
    {
        int base = lua_gettop(L) - nargs;
        lua_pushcfunction(L, fn);
        lua_insert(L, base + 1);  // fn sits just below its nargs args
        if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK) {
            lua_pop(L, 1);  // drop the error message
            return -1;
        }
        return lua_gettop(L) - base;
    }

    // call a void bind that consumed `nargs` stack slots (incl. self)
    void CallVoid(int (*fn)(lua_State*), int nargs) { call(fn, nargs); }

    // call a bind returning one integer; pops args+returns
    int64_t CallInt(int (*fn)(lua_State*), int nargs)
    {
        int k     = call(fn, nargs);
        int64_t v = (k >= 1) ? (int64_t)lua_tointeger(L, -1) : 0;
        if (k >= 0) lua_pop(L, k);
        return v;
    }

    // call a bind returning one boolean; pops args+returns
    bool CallBool(int (*fn)(lua_State*), int nargs)
    {
        int k     = call(fn, nargs);
        bool v    = (k >= 1) ? (lua_toboolean(L, -1) != 0) : false;
        bool nilv = (k >= 1) ? (lua_isnil(L, -1) != 0) : true;
        if (k >= 0) lua_pop(L, k);
        return nilv ? false : v;
    }

    // call a bind returning one string; copies into `out`, pops args+returns
    bool CallStr(int (*fn)(lua_State*), int nargs, std::string& out)
    {
        int k   = call(fn, nargs);
        bool ok = false;
        if (k >= 1 && lua_isstring(L, -1)) {
            out.assign(lua_tostring(L, -1));
            ok = true;
        }
        else if (k >= 1 && lua_isnil(L, -1)) {
            out.clear();
            ok = false;
        }
        if (k >= 0) lua_pop(L, k);
        return ok;
    }

    // call a bind and report how many values it returned (-1 on error), then
    // pop
    int CallRetCount(int (*fn)(lua_State*), int nargs)
    {
        int k = call(fn, nargs);
        if (k >= 0) lua_pop(L, k);
        return k;
    }
};

// Fixture that also boots the real CLuaEngine (needed by loader / g_LuaEngine
// binds such as load_room, room_count, script_exists) — 配置、引擎、绑定
// 由 test_lua_env.h 的 ensure_lua_env() 统一初始化。
class BindEngineTest : public BindTest {
protected:
    void SetUp() override
    {
        BindTest::SetUp();
        ASSERT_NE(ensure_lua_env(), nullptr);
    }
};
