// test_object_create.cpp — 物品创建缺省钩子（hooks.object_create）
//
// 契约：CContainer::create 派发 hooks.object_create(obj, flag)；钩子可按
// 身份补写图片号等缺省，钩子缺席时 icon 保持未定义。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>

#include "Char.h"
#include "Corpse.h"
#include "FightRoom.h"
#include "HeartBeat.h"
#include "LuaEngine.h"
#include "Room.h"
#include "test_lua_env.h"
#include "test_scoped_room.h"
#include "mock/mock_itemmgr.h"

namespace {

// 只认 corpse_ob；与 CItemMgr::load_item 同序：先写基名再 create
CContainer* corpse_factory(const char* base_name)
{
    if (std::strcmp(base_name, "corpse_ob") != 0) return nullptr;
    CCorpse* corpse = new CCorpse();
    corpse->set("base_name", base_name);
    corpse->create();
    return corpse;
}

class ObjectCreateHookTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        m_saved_mgr = g_item_mgr;
        mockItemMgr.reset_counts();
        mockItemMgr.load_factory = corpse_factory;
        g_item_mgr               = &mockItemMgr;
    }

    void TearDown() override { g_item_mgr = m_saved_mgr; }

    // 遇害者置于房间内：make_corpse 要求 environment 承接尸体
    static CChar* MakeVictim(CRoom* room)
    {
        CChar* me = new CChar();
        me->set("id", "oc_victim");
        me->set("name", "遇害者");
        me->set("age", (long)33);
        me->set_weight(1234);
        me->move(room);
        return me;
    }

    // 尸体构造时注册了心跳，销毁前先注销（心跳表保留空位、不再解引用）
    static void FreeCorpse(CCorpse* corpse)
    {
        set_heart_beat_obj(corpse, 0);
        delete corpse;
    }

    CMockItemMgr mockItemMgr;
    IItemMgr* m_saved_mgr = nullptr;
};

}  // namespace

// 钩子缺席：icon 保持未定义
TEST_F(ObjectCreateHookTest, HookAbsent_IconUndefined)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_oc = hooks.object_create "
                            "hooks.object_create = nil"),
              LUA_OK);

    CFightRoom fr;
    fr.create();

    luaL_dostring(L, "hooks.object_create = __orig_oc __orig_oc = nil");
    EXPECT_EQ(fr.query("icon"), 0) << "钩子缺席时无 icon 写入";
}

// 实参管线：obj 为 userdata、flag 为数值
TEST_F(ObjectCreateHookTest, Dispatch_PushesObjectAndFlag)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(
                  L,
                  "__orig_oc = hooks.object_create\n"
                  "hooks.object_create = function(obj, flag)\n"
                  "  __oc_args = tostring(obj ~= nil) .. ',' .. type(flag)\n"
                  "  obj:set(\"icon\", 42)\n"
                  "end"),
              LUA_OK);

    CFightRoom fr;
    fr.create(7);

    luaL_dostring(L, "hooks.object_create = __orig_oc __orig_oc = nil");

    lua_getglobal(L, "__oc_args");
    ASSERT_TRUE(lua_isstring(L, -1)) << "钩子未收到实参";
    EXPECT_STREQ(lua_tostring(L, -1), "true,number");
    lua_pop(L, 1);
    EXPECT_EQ(fr.query("icon"), 42) << "obj 形参即创建中的对象";
}
