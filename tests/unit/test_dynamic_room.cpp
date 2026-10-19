// test_dynamic_room.cpp — 动态实例房间机制（前缀→共享脚本 + 构造期 CAN_SAVE）
//
// 契约（通用房间机制，corps_base/house 等动态实例共用）：
//   1) base_name 命中某房间脚本声明的 prefix 字段 → 工厂实例化该脚本
//      （CLuaRoom），而非 C++ 兜底；脚本 create 生效。
//   2) 前缀房间的存档恢复时序：load() 早于 create()；can_save 字段使
//      query_attr(CAN_SAVE) 在 create 之前即为真，从而 load() 读取 t_room，
//      命中记录则恢复 dbase，未命中则置 fail_ob。
// 夹具 fixture_prefix_room.lua 位于 lua-tests（测试根），prefix="fxroom"。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Container.h"
#include "Global.h"
#include "LuaRoom.h"
#include "Mapping.h"
#include "Room.h"
#include "RoomMgr.h"
#include "mock/mock_database.h"
#include "test_scoped_room.h"
#include "test_lua_env.h"

namespace {

// 需要真实 g_db 的用例：fixture 注入 MockDatabase 并在 TearDown 复位，
// 致命断言提前返回也不会留下悬垂的 g_db。
class DynamicRoomDbTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        g_db = &db;
    }

    void TearDown() override { g_db = nullptr; }

    MockDatabase db;
};

}  // namespace

// 1) 前缀命中 → 实例化为 Lua 房间，脚本 create 生效
TEST(DynamicRoomTest, PrefixResolvesToSharedScript)
{
    ensure_lua_env();

    const char* base_name = "fxroom00001";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    // 走的是共享脚本实例（CLuaRoom），不是 C++ 兜底 CRoom
    EXPECT_NE(dynamic_cast<CLuaRoom*>(room), nullptr);
    // 脚本 create 写入的标志存在
    EXPECT_EQ(room->query("fx_created"), 1);
    // 工厂复刻了 base_name 归属
    EXPECT_STREQ(room->querystr("base_name"), base_name);
}

// 2a) can_save 使 load 早于 create 读取 t_room：命中记录恢复 dbase
TEST_F(DynamicRoomDbTest, PrefixRoomRestoresPersistedDbase)
{
    // 预置一条 t_room 存档：dbase 含 fx_from_db
    CMapping persisted;
    persisted.set("fx_from_db", (int32_t)4242);
    std::string buffer = persisted.get_buffer();

    db.SetQueryResultMatching("t_room", MockDatabase::MakeSingleRow({buffer}));

    const char* base_name = "fxroom00002";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    EXPECT_EQ(room->query("fx_from_db"), 4242) << "存档 dbase 应被恢复";
    EXPECT_EQ(room->query("fail_ob"), 0) << "命中记录不应置 fail_ob";
    EXPECT_EQ(room->query("fx_created"), 1) << "create 仍在 load 之后执行";
}

// 2b) 无存档记录 → 置 fail_ob（与 C++ 兜底同语义）
TEST_F(DynamicRoomDbTest, PrefixRoomMissingArchive_FailsOb)
{
    // 无预置：fixture 的 MockDatabase 对 t_room 查询返回空结果

    const char* base_name = "fxroom00003";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    EXPECT_NE(room->query("fail_ob"), 0) << "无存档应置 fail_ob";
    EXPECT_NE(dynamic_cast<CLuaRoom*>(room), nullptr)
        << "即便无存档仍是脚本实例";
}

// ====================================================================
// 专属存盘表路由（save_table 脚本字段）：帮派总舵等动态实例按声明表存/取/锁。
// C++ 不认识 "t_fxroute" 这个数据集专用表名——它只来自脚本声明。
// ====================================================================

// 3) 工厂在 load() 之前把脚本 save_table 镜像进 dbase
TEST_F(DynamicRoomDbTest, PrefixRoomMirrorsSaveTableBeforeLoad)
{
    const char* base_name = "fxroute00001";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    EXPECT_STREQ(room->querystr("save_table"), "t_fxroute")
        << "脚本 save_table 应在构造期镜像进 dbase";
    EXPECT_NE(dynamic_cast<CLuaRoom*>(room), nullptr);
}

// 4a) load() 按声明表读取：t_fxroute 命中记录恢复 dbase（而非 t_room）
TEST_F(DynamicRoomDbTest, PrefixRoomLoadRoutesToDeclaredTable)
{
    CMapping persisted;
    persisted.set("fx_from_corp_table", (int32_t)777);
    // 只预置 t_fxroute 的结果；若误读 t_room 则拿不到该行
    db.SetQueryResultMatching(
        "select obj_data from t_fxroute",
        MockDatabase::MakeSingleRow({persisted.get_buffer()}));

    const char* base_name = "fxroute00002";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    EXPECT_EQ(room->query("fx_from_corp_table"), 777)
        << "存档应从 t_fxroute 恢复";
    EXPECT_EQ(room->query("fail_ob"), 0) << "命中记录不应置 fail_ob";
}

// 4b) save() 按声明表写入：obj_id = base_name
TEST_F(DynamicRoomDbTest, PrefixRoomSaveRoutesToDeclaredTable)
{
    const char* base_name = "fxroute00003";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    ASSERT_EQ(room->save(), 1);
    EXPECT_TRUE(db.ExecutedContaining("t_fxroute"))
        << "存盘 SQL 应命中 t_fxroute";
    EXPECT_TRUE(db.ExecutedContaining(base_name)) << "obj_id 应为 base_name";
}

// 5) lock_data() 按声明表加锁：与 save/load 路由一致
TEST_F(DynamicRoomDbTest, PrefixRoomLockRoutesToDeclaredTable)
{
    db.SetQueryResultMatching("select obj_lock from t_fxroute",
                              MockDatabase::MakeSingleRow({"0"}));

    const char* base_name = "fxroute00004";
    ScopedRoom guard(base_name);

    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    EXPECT_EQ(room->lock_data(1), 1) << "当前锁为 0，加锁应成功";
    EXPECT_TRUE(db.ExecutedContaining("update t_fxroute set obj_lock = 1"))
        << "加锁应写 t_fxroute";
}

// ====================================================================
// LuaRoom.inl 深路：脚本钩子回读 + desc 全程 + create(flag) 全程
// ====================================================================

TEST(DynamicRoomTest, HandleActionResultReturnedFromScript)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);
    ASSERT_NE(lua_room->get_script_ref(), LUA_NOREF) << "create 后脚本已载";

    CChar me;
    me.set_name("访客", "visitor");

    EXPECT_EQ(7,
              lua_room->handle_action((char*)"cmd", &me, (char*)"", (char*)""))
        << "脚本 handle_action 返回值回读(329-331)";
}

TEST(DynamicRoomTest, ValidLeaveResultReturnedFromScript)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);

    CChar me;
    me.set_name("访客", "visitor");

    EXPECT_EQ(0, lua_room->valid_leave(&me, "north"))
        << "脚本 0 = 禁止离开，结果回读(370-372)";
}

TEST(DynamicRoomTest, DoLookResultReturnedFromScript)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);

    CChar me;
    me.set_name("访客", "visitor");

    EXPECT_EQ(1, lua_room->do_look(&me)) << "脚本 do_look 返回值回读(241-242)";
}

TEST(DynamicRoomTest, DescRunsScriptHook)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);
    ASSERT_NE(lua_room->get_script_ref(), LUA_NOREF);

    room->del("desc_ran");
    CChar me;
    me.set_name("访客", "visitor");
    lua_room->desc(&me);

    EXPECT_EQ(1, room->query("desc_ran")) << "脚本 desc 执行(447-468)";
}

TEST(DynamicRoomTest, CreateIntFlagRunsScriptHook)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);

    room->del("fx_created");
    lua_room->create(3);

    EXPECT_EQ(1, room->query("fx_created"))
        << "create(self, flag) 走脚本全程(116-141)";
}

// ====================================================================
// LuaRoom.inl：空脚本/缺脚本的 C++ 兜底链 + reload/bridge
// ====================================================================

TEST(DynamicRoomTest, BlankScriptCreateSkipsScriptEffects)
{
    ensure_lua_env();
    CLuaRoom room("fixture_blank_room");
    room.create();

    EXPECT_NE(room.get_script_ref(), LUA_NOREF)
        << "脚本加载成功但缺 create 钩子";
    EXPECT_EQ(0, (long)room.query("fx_created"))
        << "缺钩子 → C++ 兜底，无脚本副作用(57-59)";
}

TEST(DynamicRoomTest, BlankScriptHooksRunCppDefaults)
{
    ensure_lua_env();
    CLuaRoom room("fixture_blank_room");
    room.create();
    ASSERT_NE(room.get_script_ref(), LUA_NOREF);

    CChar me;
    me.set_name("住客", "guest");

    room.set("base_name", "blankroom01");
    room.init(&me);  // 无 init 钩子 → C++ 默认(193)
    room.setup();    // 无 setup 钩子 → C++ 默认(254)
    EXPECT_STREQ("blankroom01", room.querystr("map"))
        << "C++ setup 兜底设 map=base_name";

    EXPECT_EQ(0, room.handle_action((char*)"xyz", &me, (char*)"", (char*)""))
        << "无 handle_action 钩子 → C++ 处理(302-303)";

    room.set("no_leave", 1);
    EXPECT_EQ(0, room.valid_leave(&me, "north"))
        << "无 valid_leave 钩子 → C++ no_leave 生效(351-352)";
}

TEST(DynamicRoomTest, MissingScriptHooksFallBackToCppBase)
{
    ensure_lua_env();
    CLuaRoom room("definitely_missing_room_xyz");
    room.set("base_name", "missingroom01");
    room.create();
    ASSERT_EQ(LUA_NOREF, room.get_script_ref())
        << "加载失败保持 NOREF，走 C++ 兜底(89)";

    CChar me;
    me.set("id", "p1");
    room.init(&me);  // NOREF → C++ init(193)
    room.setup();    // NOREF → C++ setup(254)
    EXPECT_STREQ("missingroom01", room.querystr("map"))
        << "无脚本时 setup 仍设 map=base_name";
}

TEST(DynamicRoomTest, ReloadScriptRunsResetHook)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);
    auto* lua_room = dynamic_cast<CLuaRoom*>(room);
    ASSERT_NE(lua_room, nullptr);

    room->del("reset_ran");
    EXPECT_EQ(1, lua_room->reload_script()) << "reset 钩子执行返回 1";
    EXPECT_EQ(1, (long)room->query("reset_ran")) << "reset 效果落地(164)";
}

TEST(DynamicRoomTest, ReloadScriptFallbacksReturnZero)
{
    ensure_lua_env();

    CLuaRoom blank("fixture_blank_room");
    blank.create();
    ASSERT_NE(blank.get_script_ref(), LUA_NOREF);
    EXPECT_EQ(0, blank.reload_script()) << "无 reset 钩子 → 0(174)";

    CLuaRoom missing("definitely_missing_room_xyz");
    EXPECT_EQ(0, missing.reload_script()) << "NOREF → 0(167)";
}

TEST(DynamicRoomTest, NotifyReloadRunsInstanceBridge)
{
    ensure_lua_env();
    ScopedRoom guard("fxroom00001");
    CRoom* room = guard.get();
    ASSERT_NE(room, nullptr);

    room->del("reset_ran");
    lua_room_notify_reload("fixture_prefix_room");
    EXPECT_EQ(1, (long)room->query("reset_ran"))
        << "registry bridge → reload_script → reset(17-19)";
}
