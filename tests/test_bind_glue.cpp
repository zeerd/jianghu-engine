// test_bind_glue.cpp — cross-class Lua glue binds in lua/LuaBindGlue.cpp
// (base_* dispatchers, item-list iterator, call_out).
#include "test_bind_common.h"
#include "Container.h"
#include "LuaRoom.h"
#include "test_lua_env.h"
#include "LuaNpc.h"    // CLuaNpc/Vendor/BankOwner/MirrorNpc
#include "LuaSkill.h"  // CLuaSkill/CLuaThrowing

class BindGlueTest : public BindTest {
protected:
    CChar ch;  // not a CLua* template -> base_* guards return 0
    std::vector<DTItemList*> m_lists;
    void SetUp() override
    {
        BindTest::SetUp();
        ensure_lua_env();  // CLua*::create / quest_do_talk 依赖全局引擎
        ch.set_name("glue", "glue1");
    }
    void TearDown() override
    {
        for (auto* p : m_lists) delete p;
        BindTest::TearDown();
    }
    // build an empty jianghu_item_list userdata (Get_ItemList return shape)
    void push_empty_list()
    {
        auto* list = new DTItemList();
        m_lists.push_back(list);
        DTItemList** ud = (DTItemList**)lua_newuserdata(L, sizeof(DTItemList*));
        *ud             = list;
        luaL_getmetatable(L, "jianghu_item_list");
        lua_setmetatable(L, -2);
    }
};

// base_* dispatchers: self is a plain CChar -> no CLua* branch matches.
// l_base_create returns 0; the others push a 0 result and return 1.
TEST_F(BindGlueTest, BaseDispatchersGuard)
{
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_base_create, 1));
    lua_bind_push_object(L, &ch);
    lua_bind_push_object(L, &ch);  // me slot must hold a real object
    EXPECT_EQ(0, CallRetCount(l_base_init, 2));
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_base_setup, 1));
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_base_reset, 1));
}

// 房间脚本接管 valid_leave 后用 base_valid_leave 取 C++ 基类判定；
// CRoom::valid_leave 恒放行，普通对象无房间分支 -> 0。
TEST_F(BindGlueTest, BaseValidLeaveDispatch)
{
    CLuaRoom room("nonexistent_script");

    lua_bind_push_object(L, &room);
    lua_bind_push_object(L, &ch);  // me
    lua_pushstring(L, "north");
    int k = call(l_base_valid_leave, 3);
    ASSERT_EQ(1, k);
    EXPECT_EQ(1, (int)lua_tointeger(L, -1));
    lua_pop(L, k);

    lua_bind_push_object(L, &ch);
    lua_bind_push_object(L, &ch);  // me
    lua_pushstring(L, "north");
    k = call(l_base_valid_leave, 3);
    ASSERT_EQ(1, k);
    EXPECT_EQ(0, (int)lua_tointeger(L, -1));
    lua_pop(L, k);
}

TEST_F(BindGlueTest, BaseHandleActionNeedsMe)
{
    // object slots hold real userdata; class mismatch -> guard returns 0
    lua_bind_push_object(L, &ch);
    lua_bind_push_object(L, &ch);  // me
    lua_pushstring(L, "go north");
    EXPECT_EQ(1, CallRetCount(l_base_handle_action, 3));

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "go north");
    lua_bind_push_object(L, &ch);  // me
    EXPECT_EQ(1, CallRetCount(l_base_handle_action4, 3));
}

TEST_F(BindGlueTest, BaseDoTalkGuard)
{
    lua_bind_push_object(L, &ch);
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "hello");
    // plain CChar is not CLuaVendor/CLuaNpc -> pushes 0 and returns 1
    EXPECT_EQ(1, CallRetCount(l_base_do_talk, 3));
}

TEST_F(BindGlueTest, HandleActionGuard)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "go");
    lua_bind_push_object(L, &ch);  // me
    int k = call(l_handle_action, 3);
    EXPECT_EQ(1, k);  // pushes 0 (con != CContainer path)
    lua_pop(L, k);
}

// item-list iterator over an empty list
TEST_F(BindGlueTest, IterItemsEmpty)
{
    push_empty_list();
    // a valid (even empty) list yields an iterator closure -> returns 1
    int k = call(l_iter_items, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, k);
}

// non-empty list: the closure yields the element, then nil
TEST_F(BindGlueTest, IterItemsNonEmpty)
{
    auto* list = new DTItemList();
    m_lists.push_back(list);
    list->push_back(&ch);
    DTItemList** ud = (DTItemList**)lua_newuserdata(L, sizeof(DTItemList*));
    *ud             = list;
    luaL_getmetatable(L, "jianghu_item_list");
    lua_setmetatable(L, -2);

    int k = call(l_iter_items, 1);  // list -> closure
    ASSERT_EQ(1, k);
    ASSERT_TRUE(lua_isfunction(L, -1));
    lua_pushvalue(L, -1);  // keep a copy: lua_pcall consumes the function

    // first step yields the element itself (object userdata)
    ASSERT_EQ(LUA_OK, lua_pcall(L, 0, 1, 0));
    EXPECT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    lua_pop(L, 1);

    // second step: list exhausted -> nil
    ASSERT_EQ(LUA_OK, lua_pcall(L, 0, 1, 0));
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);
}

// ---- call_out family: self has no script ref and no active script -> 0 ----
TEST_F(BindGlueTest, CallOutNoScript)
{
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "some_func");
    lua_pushinteger(L, 1);
    int k = call(l_call_out, 3);
    EXPECT_LE(k, 0);  // returns 0 (no id) when there is no script

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "some_func");
    lua_pushinteger(L, 1);
    k = call(l_call_out_combat, 3);
    EXPECT_LE(k, 0);

    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "some_func");
    k = call(l_remove_call_out, 2);
    EXPECT_EQ(1, k);
    lua_pop(L, k);
}

// ====================================================================
// 真类型分发：各 l_base_* 命中真实 CLua* 分支（守卫测试只覆盖 plain CChar）
// ====================================================================

TEST_F(BindGlueTest, BaseDispatchersRealTypes)
{
    // ---- l_base_do_talk：Vendor/BankOwner/MirrorNpc/Npc 四支(344-362) ----
    {
    }

    {
        CLuaNpc n("t_error_npc");
        lua_bind_push_object(L, &n);
        lua_bind_push_object(L, &ch);
        lua_pushstring(L, "hi");
        EXPECT_EQ(1, CallRetCount(l_base_do_talk, 3)) << "Npc 支(361,362)";
    }

    // ---- l_base_create：MirrorNpc/Throwing/Skill 三支(420,432,438) ----
    {
        CLuaMirrorNpc m("mk2");
        lua_bind_push_object(L, &m);
        EXPECT_EQ(0, CallRetCount(l_base_create, 1)) << "MirrorNpc create(420)";
    }
    {
        CLuaThrowing t("tc_throwing");
        lua_bind_push_object(L, &t);
        EXPECT_EQ(0, CallRetCount(l_base_create, 1)) << "Throwing create(432)";
    }
    {
        CLuaSkill s("force");
        lua_bind_push_object(L, &s);
        EXPECT_EQ(0, CallRetCount(l_base_create, 1)) << "Skill create(438)";
    }

    // ---- l_base_handle_action4：Room/Resident/FBRoom/Npc 四支 ----
    {
        CLuaRoom r("fixture_prefix_room");
        lua_bind_push_object(L, &r);
        lua_pushstring(L, "go north");
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(1, CallRetCount(l_base_handle_action4, 3)) << "Room(472)";
    }
    {
        CLuaResident rs("rs");
        lua_bind_push_object(L, &rs);
        lua_pushstring(L, "go north");
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(1, CallRetCount(l_base_handle_action4, 3))
            << "Resident(480,484)";
    }
    {
        CLuaFBRoom fr("fr");
        lua_bind_push_object(L, &fr);
        lua_pushstring(L, "go north");
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(1, CallRetCount(l_base_handle_action4, 3))
            << "FBRoom(488,492)";
    }
    {
        CLuaNpc n("t_error_npc");
        lua_bind_push_object(L, &n);
        lua_pushstring(L, "go north");
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(1, CallRetCount(l_base_handle_action4, 3)) << "Npc(496,500)";
    }

    // ---- l_base_init：Npc/FB 两支(542,567) ----
    {
        CLuaNpc n("t_error_npc");
        lua_bind_push_object(L, &n);
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(0, CallRetCount(l_base_init, 2)) << "Npc init(542)";
    }
    {
        CLuaFB fb("fb");
        lua_bind_push_object(L, &fb);
        lua_bind_push_object(L, &ch);
        EXPECT_EQ(0, CallRetCount(l_base_init, 2)) << "FB init(567)";
    }

    // ---- l_base_valid_leave：FB 支(617,619) ----
    {
        CLuaFB fb("fb");
        lua_bind_push_object(L, &fb);
        lua_bind_push_object(L, &ch);
        lua_pushstring(L, "north");
        int k = call(l_base_valid_leave, 3);
        ASSERT_EQ(1, k) << "FB valid_leave 回推(617,619)";
        lua_pop(L, k);
    }

    // ---- l_base_setup：Room/Resident/FBRoom/FB 四支(643,649,655,661) ----
    {
        CLuaRoom r("fixture_prefix_room");
        lua_bind_push_object(L, &r);
        EXPECT_EQ(0, CallRetCount(l_base_setup, 1)) << "Room setup(643)";
    }
    {
        CLuaResident rs("rs2");
        lua_bind_push_object(L, &rs);
        EXPECT_EQ(0, CallRetCount(l_base_setup, 1)) << "Resident setup(649)";
    }
    {
        CLuaFBRoom fr("fr2");
        lua_bind_push_object(L, &fr);
        EXPECT_EQ(0, CallRetCount(l_base_setup, 1)) << "FBRoom setup(655)";
    }
    {
        CLuaFB fb("fb2");
        lua_bind_push_object(L, &fb);
        EXPECT_EQ(0, CallRetCount(l_base_setup, 1)) << "FB setup(661)";
    }

    // ---- l_base_reset：Resident/FB 两支(705,717) ----
    {
        CLuaResident rs("rs3");
        lua_bind_push_object(L, &rs);
        EXPECT_EQ(0, CallRetCount(l_base_reset, 1)) << "Resident reset(705)";
    }
    {
        CLuaFB fb("fb3");
        lua_bind_push_object(L, &fb);
        EXPECT_EQ(0, CallRetCount(l_base_reset, 1)) << "FB reset(717)";
    }
}

TEST_F(BindGlueTest, CallOutRegisterReturnsId)
{
    CLuaRoom room("fixture_prefix_room");
    room.create();

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "reset");  // fixture_prefix_room 真实存在的钩子
    lua_pushinteger(L, 999999);  // delay 极大 → 桥不触发（无悬垂）
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    int k = call(l_call_out, 5);
    ASSERT_EQ(1, k) << "回推 calloutId(287-289)";
    int id = (int)lua_tointeger(L, -1);
    EXPECT_GE(id, 0) << "register_callout 成功(281-285)";
    lua_pop(L, k);

    room.remove_call_out_by_id(id);  // 出队防栈对象悬垂
}

TEST_F(BindGlueTest, IniGetBindReturnsDefault)
{
    lua_pushstring(L, "server");
    lua_pushstring(L, "no_such_key");
    lua_pushstring(L, "fallback_value");
    int k = call(l_ini_get, 3);
    ASSERT_EQ(1, k) << "回推一个字符串(732-733)";
    EXPECT_STREQ(lua_tostring(L, -1), "fallback_value")
        << "配置缺失 → 返回默认值(731-732)";
    if (k > 0) lua_pop(L, k);
}
