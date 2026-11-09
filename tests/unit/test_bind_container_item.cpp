// test_bind_container_item.cpp — binds in object/ContainerItem.cpp
// (present / get_item_list / descriptions / say / relay channel)
#include "test_bind_common.h"

class BindContainerItemTest : public BindTest {
protected:
    CContainer room;
    CContainer item;
};

TEST_F(BindContainerItemTest, DescDispatchesByKind)
{
    room.set("long", "a long road");
    room.set("short", "road");
    room.set("title", "the great");

    std::string s;
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "long");
    ASSERT_TRUE(CallStr(l_desc, 2, s));
    EXPECT_NE(std::string::npos, s.find("long"));

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "short");
    ASSERT_TRUE(CallStr(l_desc, 2, s));
    EXPECT_NE(std::string::npos, s.find("road"));

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "title");
    ASSERT_TRUE(CallStr(l_desc, 2, s));
    EXPECT_NE(std::string::npos, s.find("great"));

    // 未登记 KIND 报错，不返回空串
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "nope");
    EXPECT_EQ(-1, call(l_desc, 2));
}

TEST_F(BindContainerItemTest, GetItemListEmptyNil)
{
    lua_bind_push_object(L, &room);
    int k = call(l_get_item_list, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);  // nil
}

TEST_F(BindContainerItemTest, PresentFound)
{
    item.set("id", "sword");
    item.move(&room);

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "id");
    lua_pushstring(L, "sword");
    int k = call(l_present, 3);
    ASSERT_EQ(1, k);
    EXPECT_TRUE(lua_isuserdata(L, -1));
    lua_pop(L, k);
}

TEST_F(BindContainerItemTest, PresentByIdNotFound)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "object_id");
    lua_pushinteger(L, 999999);
    int k = call(l_present, 3);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));  // not present -> nil
    lua_pop(L, k);
}

TEST_F(BindContainerItemTest, PresentUnknownKindErrors)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "nope");
    EXPECT_EQ(-1, call(l_present, 2));
}

TEST_F(BindContainerItemTest, SayNoCrash)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "hello");
    CallVoid(l_say, 2);
}

TEST_F(BindContainerItemTest, present_name)
{
    item.set("id", "apple");
    item.set("name", "apple");
    item.move(&room);
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "name");
    lua_pushstring(L, "apple");
    lua_pushinteger(L, 0);
    int k = call(l_present, 4);
    EXPECT_GE(k, 0);
    if (k >= 0) lua_pop(L, k);
}
