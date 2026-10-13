// test_bind_fbroom.cpp — binds in room/FBRoom.cpp.
// Only the guard / short-circuit paths are exercised: begin_fb touches the
// global heartbeat list and load_fb_room runs a script loader, both unsafe for
// a stack object in a unit test.
#include "test_bind_common.h"
#include "FB.h"
#include "FBRoom.h"

class BindFBRoomTest : public BindTest {
protected:
    CFB fb;
    CFBRoom fbr;
    CChar me;
    void SetUp() override
    {
        BindTest::SetUp();
        me.set_name("m", "m1");
    }
};

// query_fb_parent: CFBRoom with no parent -> nil
TEST_F(BindFBRoomTest, QueryFbParentNil)
{
    lua_bind_push_object(L, &fbr);
    int k = call(l_query_fb_parent, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);
}

// query_fb_parent: self is not a CFBRoom -> nil (same branch)
TEST_F(BindFBRoomTest, QueryFbParentNonRoom)
{
    lua_bind_push_object(L, &me);
    int k = call(l_query_fb_parent, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);
}

// move_in: empty CFB (m_RoomNum == 0) -> short-circuits, returns 0 values
TEST_F(BindFBRoomTest, MoveInEmptyFb)
{
    lua_bind_push_object(L, &fb);
    lua_bind_push_object(L, &me);
    lua_pushinteger(L, 0);
    EXPECT_EQ(0, CallRetCount(l_move_in, 3));
}

// move_in: self not a CFB -> guard returns 0
TEST_F(BindFBRoomTest, MoveInGuard)
{
    lua_bind_push_object(L, &me);
    lua_bind_push_object(L, &me);
    EXPECT_EQ(0, CallRetCount(l_move_in, 2));
}

// begin_fb: self not a CFB -> guard returns 0 (never reaches Set_Heart_Beat)
TEST_F(BindFBRoomTest, BeginFbGuard)
{
    lua_bind_push_object(L, &me);
    lua_pushstring(L, "map");
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    EXPECT_EQ(0, CallRetCount(l_begin_fb, 4));
}

// load_fb_room: self not a CFB -> guard returns 0
TEST_F(BindFBRoomTest, LoadFbRoomGuard)
{
    lua_bind_push_object(L, &me);
    lua_pushstring(L, "room");
    EXPECT_EQ(0, CallRetCount(l_load_fb_room, 2));
}
