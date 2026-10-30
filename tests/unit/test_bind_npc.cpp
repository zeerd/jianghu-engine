// test_bind_npc.cpp — binds in npc/Npc.cpp (Vendor carry_good included)
#include "test_bind_common.h"
#include "Npc.h"
#include "Vendor.h"

class BindNpcTest : public BindTest {
protected:
    CNpc npc;
    CMockItemMgr mgr;
    void SetUp() override
    {
        BindTest::SetUp();
        npc.set_name("npc", "npc1");
    }
};

// self:carry_object(name[, amount]) -> object (needs item factory)
TEST_F(BindNpcTest, CarryObject)
{
    ItemFactory f(&mgr);
    lua_bind_push_object(L, &npc);
    lua_pushstring(L, "obj1");
    lua_pushinteger(L, 1);
    int k = call(l_carry_object, 3);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isuserdata(L, -1));
    lua_pop(L, k);
}

// self:perform_action(magic, target) -> 1/0：npc 无环境下 command 早退
// 回 0；绑定回传结果，脚本据以分流后续行为
TEST_F(BindNpcTest, PerformAction)
{
    lua_bind_push_object(L, &npc);
    lua_pushstring(L, "strike");
    lua_pushinteger(L, 0);  // target=0 -> skip environment() deref
    int k = call(l_perform_action, 3);
    EXPECT_EQ(1, k) << "绑定必须回传一个结果值（不推 nil）";
    EXPECT_EQ(0, lua_tointeger(L, -1));
    if (k >= 0) lua_pop(L, k);
}

// self:perform_action：self 不是 NPC 时守卫同样回 0——不推 nil，
// 免得脚本的 `~= 0` 判断把守卫失败当成成功
TEST_F(BindNpcTest, PerformActionGuardPushesZero)
{
    CContainer not_npc;
    not_npc.set_name("box", "box1");
    lua_bind_push_object(L, &not_npc);
    lua_pushstring(L, "strike");
    lua_pushinteger(L, 0);
    int k = call(l_perform_action, 3);
    EXPECT_EQ(1, k);
    EXPECT_EQ(0, lua_tointeger(L, -1));
    if (k >= 0) lua_pop(L, k);
}

// self:wield_ownered_item(name) -> carry_object + wield (needs item factory)
TEST_F(BindNpcTest, WieldOwneredItem)
{
    ItemFactory f(&mgr);
    lua_bind_push_object(L, &npc);
    lua_pushstring(L, "sword");
    EXPECT_EQ(0, CallRetCount(l_wield_ownered_item, 2));
    // 契约：携带的物品确实进入背包（工厂 id="item"）
    EXPECT_NE(npc.present("item"), nullptr);
}

// self:set_inquiry(ask, answer, topic) — plain CNpc hits the CLuaNpc guard
TEST_F(BindNpcTest, SetInquiryGuard)
{
    // 非 CLuaNpc → dynamic_cast 守卫拦下（void 无返回）
    lua_bind_push_object(L, &npc);
    lua_pushstring(L, "hello");
    lua_pushstring(L, "hi there");
    lua_pushstring(L, "greet");
    EXPECT_EQ(0, CallRetCount(l_set_inquiry, 4));
}

TEST_F(BindNpcTest, SetInquiryFuncGuard)
{
    lua_bind_push_object(L, &npc);
    lua_pushstring(L, "ask");
    lua_pushstring(L, "funcName");
    EXPECT_EQ(0, CallRetCount(l_set_inquiry_func, 3));
}

// self:random_move() — environment is not a room -> early return
TEST_F(BindNpcTest, RandomMoveNoEnv)
{
    CContainer env;
    env.set_name("env", "env1");
    npc.move(&env);
    lua_bind_push_object(L, &npc);
    EXPECT_EQ(0, CallRetCount(l_random_move, 1));
    // 契约：非房间环境不移动（Npc.cpp IS_ROOM 守卫）
    EXPECT_EQ(&env, npc.environment());
    npc.move(nullptr);
}

// self:add_money(amount) — needs item factory (load_item("coin"))
TEST_F(BindNpcTest, AddMoney)
{
    ItemFactory f(&mgr);
    lua_bind_push_object(L, &npc);
    lua_pushinteger(L, 1);  // 单枚：amount>1 会触发 move() 的“涉嫌复制”守卫
    EXPECT_EQ(0, CallRetCount(l_add_money, 2));
    // 契约：金钱物品进入 NPC 背包（工厂 id="item"）
    EXPECT_NE(npc.present("item"), nullptr);
}

// ---- Vendor: carry_good ----
TEST_F(BindNpcTest, CarryGood)
{
    ItemFactory f(&mgr);
    CVendor v;
    v.set_name("v", "v1");
    lua_bind_push_object(L, &v);
    lua_pushstring(L, "bread");
    lua_pushinteger(L, 3);
    int k = call(l_carry_good, 3);
    EXPECT_GE(k, 0);
    if (k >= 0) lua_pop(L, k);
}
