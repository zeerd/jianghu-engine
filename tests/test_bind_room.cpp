// test_bind_room.cpp — binds in room/Room.cpp (+ Resident.cpp)
// 箱子/出口自身的绑定（l_unlocked/l_locked/l_add_object/l_door_bind）见
// test_box_door.cpp
#include "test_bind_common.h"
#include "Room.h"

class BindRoomTest : public BindTest {
protected:
    CRoom room;
    void SetUp() override
    {
        BindTest::SetUp();
        room.set_name("lobby", "lobby1");
    }
};

TEST_F(BindRoomTest, AddDoorAndExits)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "north");
    lua_pushstring(L, "room/next1");
    lua_pushstring(L, "south");
    lua_pushstring(L, "N");
    int k = call(l_add_door, 5);
    ASSERT_EQ(1, k);  // add_door 恒定把新建的出口对象交还脚本
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    CMainObject* door = lua_bind_check_object(L, -1);
    ASSERT_NE(nullptr, door);
    EXPECT_STREQ("north", door->querystr("id"));
    EXPECT_EQ(static_cast<CMainObject*>(room.present_door("north")), door);
    lua_pop(L, k);

    lua_bind_push_object(L, &room);
    k = call(l_room_exits, 1);
    EXPECT_GE(k, 1);
    if (k >= 0) lua_pop(L, k);
}

// 脚本侧以 self:PresentDoor(id) / self:room_exits() 形态调用，两者必须在
// object_methods 注册；直接调 C 函数的用例绕过了注册表，抓不到缺失。
TEST_F(BindRoomTest, PresentDoorAndRoomExitsMethods)
{
    room.add_door("north", "room/next1", "south", "N");

    lua_bind_push_object(L, &room);
    lua_setglobal(L, "__room");
    ASSERT_EQ(LUA_OK,
              luaL_dostring(L,
                            "__byid = __room:PresentDoor('north')\n"
                            "__bydir = __room:PresentDoor('N')\n"
                            "__miss = __room:PresentDoor('no_such_door')\n"
                            "__exits = __room:room_exits()\n"));

    lua_getglobal(L, "__byid");
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    EXPECT_EQ(static_cast<CMainObject*>(room.present_door("north")),
              lua_bind_check_object(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "__bydir");
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    EXPECT_EQ(static_cast<CMainObject*>(room.present_door("N")),
              lua_bind_check_object(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "__miss");
    EXPECT_EQ(LUA_TNIL, lua_type(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "__exits");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    ASSERT_EQ(1, (int)lua_rawlen(L, -1));
    lua_rawgeti(L, -1, 1);
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "id");
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ("north", lua_tostring(L, -1));
    lua_getfield(L, -2, "next_door");
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ("south", lua_tostring(L, -1));
    lua_pop(L, 4);

    luaL_dostring(
        L,
        "__room = nil __byid = nil __bydir = nil __miss = nil __exits = nil");
}

TEST_F(BindRoomTest, RemoveDoors)
{
    room.add_door((char*)"east", (char*)"room/e1", (char*)"west", (char*)"E");
    ASSERT_EQ(1u, room.query_exits()->size());

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "room/e1");
    EXPECT_EQ(0, CallRetCount(l_remove_doors, 2));
    // 契约：按 next_room 匹配删除出口（CRoom::remove_doors）
    EXPECT_EQ(0u, room.query_exits()->size());

    room.add_door((char*)"east", (char*)"room/e1", (char*)"west", (char*)"E");
    lua_bind_push_object(L, &room);
    EXPECT_EQ(0, CallRetCount(l_remove_all_doors, 1));
    EXPECT_EQ(0u, room.query_exits()->size());
}

// door_bind / locked / unlocked / add_object need CBox/CDoor -> guard branch
TEST_F(BindRoomTest, BoxAndDoorGuards)
{
    lua_bind_push_object(L, &room);  // CRoom is not CDoor/CBox
    lua_pushstring(L, "room/x");
    lua_pushstring(L, "y");
    EXPECT_EQ(0, CallRetCount(l_door_bind, 3));

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "s");
    lua_pushinteger(L, 10);
    EXPECT_EQ(0, CallRetCount(l_locked, 3));

    lua_bind_push_object(L, &room);
    EXPECT_EQ(0, CallRetCount(l_unlocked, 1));

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "obj");
    EXPECT_EQ(0, CallRetCount(l_add_object, 2));
}

// add_item / add_box need an item factory; add_npc needs a real load_npc
// (not mockable here) so it is covered only via its type guard below.
TEST_F(BindRoomTest, AddItemBoxFactory)
{
    CMockItemMgr mgr;
    ItemFactory f(&mgr);
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "sword");
    int k = call(l_add_item, 2);
    EXPECT_GE(k, 0);
    if (k >= 0) lua_pop(L, k);

    lua_bind_push_object(L, &room);
    lua_pushstring(L, "box");
    k = call(l_add_box, 2);
    EXPECT_GE(k, 0);
    if (k >= 0) lua_pop(L, k);
}

// add_npc guard: self not a CRoom -> returns 0 (never calls load_npc)
TEST_F(BindRoomTest, AddNpcGuard)
{
    CChar ch;
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "mob");
    EXPECT_EQ(0, CallRetCount(l_add_npc, 2));
}

// init with no user -> base init（void 转发、pcall 不抛）
TEST_F(BindRoomTest, InitNoUser)
{
    lua_bind_push_object(L, &room);
    EXPECT_EQ(0, CallRetCount(l_init, 1));
}

// room_reset: exercise the type guard (self not a room)
TEST_F(BindRoomTest, RoomResetGuard)
{
    CChar ch;
    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_room_reset, 1));
}

// ---------------- door_bind：真实 CDoor 全参（48-56 深路） ----------------
TEST_F(BindRoomTest, DoorBind_FullArgsBindsTargetRoom)
{
    CDoor door;
    door.set_name("东门", "dbind_probe");

    lua_bind_push_object(L, &door);
    lua_pushstring(L, "room/z");
    lua_pushstring(L, "north");
    EXPECT_EQ(0, CallRetCount(l_door_bind, 3)) << "取串+截断缓冲+绑定(48-56)";
    EXPECT_STREQ("room/z", door.querystr("next_room"));
    EXPECT_STREQ("north", door.querystr("next_door"));
}

// ====================================================================
// l_locked / l_unlocked / l_add_object 的深路（类型守卫之上的真箱路径）
// ====================================================================

TEST_F(BindRoomTest, LockedBindRecordsCasterAsOwner)
{
    CBox box;
    box.move(&room);

    CChar holder;
    holder.set_name("保管员", "keeper");
    holder.move(&room);

    lua_bind_push_object(L, &box);
    lua_pushstring(L, "lost");
    lua_pushinteger(L, 60);
    lua_bind_push_object(L, &holder);
    CallVoid(l_locked, 4);

    EXPECT_STREQ("locked", box.querystr("status")) << "锁定生效(311)";
    EXPECT_STREQ("keeper", box.querystr("owner"))
        << "第4参 userdata 记主(307-309)";
    EXPECT_EQ(60, box.query("duration")) << "时长入 dbase(305)";
}

TEST_F(BindRoomTest, UnlockedBindResumesDurationThenOpens)
{
    CBox box;
    box.move(&room);
    box.locked((char*)"lost", 100);

    lua_bind_push_object(L, &box);
    lua_pushinteger(L, 40);
    EXPECT_EQ(60, CallInt(l_unlocked, 2)) << "扣 40 剩 60(290-291)";
    EXPECT_EQ(60, box.query("duration")) << "duration 已扣减";

    lua_bind_push_object(L, &box);
    lua_pushinteger(L, 60);
    EXPECT_EQ(0, CallInt(l_unlocked, 2)) << "扣完归 0(292)";
    EXPECT_STREQ("open", box.querystr("status")) << "到期即开";
}

TEST_F(BindRoomTest, AddObjectBindLoadsItemsIntoBox)
{
    CMockItemMgr mgr;
    ItemFactory f(&mgr);

    CBox box;
    box.move(&room);

    lua_bind_push_object(L, &box);
    lua_pushstring(L, "sword");
    lua_pushinteger(L, 2);
    int k = call(l_add_object, 3);
    if (k > 0) lua_pop(L, k);

    int n = 0;
    for (auto* it : *box.get_item_list()) {
        (void)it;
        ++n;
    }
    EXPECT_EQ(2, n) << "amount=2 非合并 → 两件入箱(324-328)";
}
