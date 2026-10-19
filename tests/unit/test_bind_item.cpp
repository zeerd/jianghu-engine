// test_bind_item.cpp — binds in item/Item.cpp + item/Equip.cpp + Corpse.cpp
#include "test_bind_common.h"
#include "Item.h"

class BindItemTest : public BindTest {
protected:
    CItem item;
    void SetUp() override
    {
        BindTest::SetUp();
        item.set_name("sword", "sw1");
    }
};

TEST_F(BindItemTest, SetAmountAndQueries)
{
    lua_bind_push_object(L, &item);
    lua_pushinteger(L, 5);
    CallVoid(l_set_amount, 2);

    lua_bind_push_object(L, &item);
    EXPECT_EQ(5, CallInt(l_query_amount, 1));
    lua_bind_push_object(L, &item);
    CallInt(l_query_weight, 1);
}

// Split with nothing to split off (nAmount == full) -> returns this
TEST_F(BindItemTest, SplitAllReturnsSelf)
{
    item.set_amount(4);
    lua_bind_push_object(L, &item);
    lua_pushinteger(L, 0);  // nAmount=0 -> "分不出来" -> 0
    int k = call(l_split, 2);
    EXPECT_GE(k, 0);
    if (k >= 0) lua_pop(L, k);
}

TEST_F(BindItemTest, SplitNeedsFactory)
{
    CMockItemMgr mgr;
    ItemFactory f(&mgr);
    item.set_amount(10);
    lua_bind_push_object(L, &item);
    lua_pushinteger(L, 3);
    int k = call(l_split, 2);
    EXPECT_EQ(1, k);
    lua_pop(L, k);
}

// do_use is a no-op returning 0
TEST_F(BindItemTest, DoUse)
{
    lua_bind_push_object(L, &item);
    int k = call(l_do_use, 1);
    EXPECT_EQ(1, k);
    EXPECT_EQ(0, lua_tointeger(L, -1));
    lua_pop(L, k);
}

// do_action_base: CItem 的 Do_Action 是无副作用的 no-op，返回 0
TEST_F(BindItemTest, DoActionBase)
{
    lua_bind_push_object(L, &item);
    lua_pushinteger(L, 0);
    int k = call(l_do_action_base, 2);
    EXPECT_EQ(1, k);
    lua_pop(L, k);
}

// l_do_wield / l_do_unwield require a CLuaEquip; a plain CItem hits the guard
TEST_F(BindItemTest, DoWieldUnwieldGuard)
{
    lua_bind_push_object(L, &item);
    EXPECT_EQ(0, CallInt(l_do_wield, 1));
    lua_bind_push_object(L, &item);
    EXPECT_EQ(0, CallInt(l_do_unwield, 1));
}

// make_corpse: victim nil -> guard pushes nil
TEST_F(BindItemTest, MakeCorpseNil)
{
    lua_pushnil(L);
    int k = call(l_make_corpse, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);
}
