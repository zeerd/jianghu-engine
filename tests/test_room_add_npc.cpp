// test_room_add_npc.cpp — 房间 NPC 摆放坐标的写入与复位
//
// 契约：
//   1. add_npc 把矩形区域六键（map/bx by ex ey dir active）与 start_city 写到
//   NPC 上，
//      并把同一份坐标记进房间节点，供 reset 复用时回填。
//   2. 节点 interval 是复位节流：>0 期间即使 NPC
//   消失也不重新载入，归零那一次才载入。
//   3. NPC 健在但身在别处 -> return_home 领回，不重复写坐标。
//   4. add_npc 必须是可多态分派的：经 CRoom* 调用副本房间时，副本特有约束
//      （NPC 不可拾取）同样生效。
//   5. 绑定 self:add_npc(...) 正向路径返回 NPC 对象，缺省参数取 0。
#include "test_bind_common.h"

#include <string>

#include "FBRoom.h"
#include "Global.h"
#include "NpcMgr.h"
#include "Room.h"

namespace {

// 按 base_name 在 NPC 表里找物件（reset 复生会换 object_id，无法按 id 追）。
CNpc* find_npc_by_base_name(const char* base_name)
{
    for (auto& kv : g_npc_list) {
        if (EQUALSTR(kv.second->querystr("base_name"), base_name))
            return kv.second;
    }
    return nullptr;
}

// 用例退出时摘除 NPC：去心跳 -> 脱离房间 -> 出 NPC 表 -> 销毁。
class ScopedNpc {
public:
    explicit ScopedNpc(CNpc* npc = nullptr) : m_npc(npc) {}
    ~ScopedNpc() { discard(); }
    ScopedNpc(const ScopedNpc&)            = delete;
    ScopedNpc& operator=(const ScopedNpc&) = delete;

    void reset(CNpc* npc) { m_npc = npc; }

    // 现在就销毁（模拟 NPC 从世界消失），析构时不再重复处理。
    void discard()
    {
        if (!m_npc) return;
        CNpc* npc = m_npc;
        m_npc     = nullptr;
        npc->set_heart_beat(0);
        npc->move(nullptr);
        g_npc_list.erase(npc->object_id());
        delete npc;
    }

private:
    CNpc* m_npc;
};

// 六键是否等于给定值
bool map_keys_are(CContainer* obj, int bx, int by, int ex, int ey, int dir,
                  int active)
{
    return obj->query("map/bx") == bx && obj->query("map/by") == by
           && obj->query("map/ex") == ex && obj->query("map/ey") == ey
           && obj->query("map/dir") == dir
           && obj->query("map/active") == active;
}

}  // namespace

class RoomAddNpcTest : public ::testing::Test {
protected:
    CRoom room;
    ScopedNpc npc_guard;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        room.set_name("摆放房", "room_add_npc");
        room.set("base_name", "room_add_npc");
    }

    void TearDown() override { npc_guard.discard(); }
};

// ---------------------------------------------------------------------
// 契约 1：add_npc 写全六键与 start_city
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcTest, AddNpc_WritesAllSixMapKeysAndStartCity)
{
    CNpc* npc = room.add_npc("rnd_npc_keys", 11, 12, 13, 14, 5, 1);
    ASSERT_NE(npc, nullptr);
    npc_guard.reset(npc);

    EXPECT_TRUE(map_keys_are(npc, 11, 12, 13, 14, 5, 1));
    EXPECT_STREQ("room_add_npc", npc->querystr("start_city"));
    // 摆放坐标写在 dbase（可存档），不是 temp
    EXPECT_EQ(0, npc->query_temp("map/bx"));
}

// ---------------------------------------------------------------------
// 契约 2：interval 是复位节流——>0 期间 NPC 消失也不复生，归零那次才复生
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcTest, Reset_ThrottlesRespawnByInterval)
{
    const char* name = "rnd_npc_throttle";
    CNpc* npc        = room.add_npc(name, 21, 22, 23, 24, 2, 0, 3);
    ASSERT_NE(npc, nullptr);
    npc_guard.reset(npc);

    npc_guard.discard();
    ASSERT_EQ(nullptr, find_npc_by_base_name(name));

    // interval 3 -> 2 -> 1 -> 0：三次都只递减，不复生
    room.reset();
    room.reset();
    room.reset();
    EXPECT_EQ(nullptr, find_npc_by_base_name(name));

    // 第四次 interval 归零并复位节流，NPC 复生且坐标由节点回填
    room.reset();
    CNpc* reborn = find_npc_by_base_name(name);
    ASSERT_NE(nullptr, reborn);
    npc_guard.reset(reborn);
    EXPECT_TRUE(map_keys_are(reborn, 21, 22, 23, 24, 2, 0));
    EXPECT_EQ(static_cast<CContainer*>(&room), reborn->environment());
}

// ---------------------------------------------------------------------
// 契约 1+2：无节流（interval=0）时，NPC 消失后 reset 立刻复生并回填六键
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcTest, Reset_RespawnsGoneNpcAndRestoresMapKeys)
{
    const char* name = "rnd_npc_respawn";
    CNpc* npc        = room.add_npc(name, 31, 32, 33, 34, 6, 1);
    ASSERT_NE(npc, nullptr);
    npc_guard.reset(npc);

    npc_guard.discard();
    ASSERT_EQ(nullptr, find_npc_by_base_name(name));

    room.reset();
    CNpc* reborn = find_npc_by_base_name(name);
    ASSERT_NE(nullptr, reborn);
    npc_guard.reset(reborn);
    EXPECT_TRUE(map_keys_are(reborn, 31, 32, 33, 34, 6, 1));
}

// ---------------------------------------------------------------------
// 契约 3：NPC 健在但身在别处 -> 领回原房间，不重复写坐标
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcTest, Reset_BringsLivingNpcHomeWithoutRewritingKeys)
{
    const char* name = "rnd_npc_home";
    CNpc* npc        = room.add_npc(name, 41, 42, 43, 44, 3, 0);
    ASSERT_NE(npc, nullptr);
    ScopedNpc home_guard(npc);

    CRoom elsewhere;
    elsewhere.set_name("别处", "room_add_npc_elsewhere");
    ASSERT_EQ(1, npc->move(&elsewhere));
    ASSERT_EQ(static_cast<CContainer*>(&elsewhere), npc->environment());

    // 别处的坐标是 NPC 自己走出来的，复位不该覆盖它
    npc->set("map/bx", (int32_t)99);

    room.reset();
    EXPECT_EQ(static_cast<CContainer*>(&room), npc->environment());
    EXPECT_EQ(99, npc->query("map/bx"));
    EXPECT_EQ(42, npc->query("map/by"));

    home_guard.discard();
}

// ---------------------------------------------------------------------
// 契约 4（红色锚点）：add_npc 必须可多态分派。
// 经 CRoom* 调用副本房间，副本约束（NPC 不可拾取）同样生效。
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcTest, AddNpc_PolymorphicFromBasePointer_AppliesFbRule)
{
    CFBRoom fbr;
    fbr.set_name("副本房", "room_add_npc_fb");
    fbr.set("base_name", "room_add_npc_fb");

    CRoom* as_base = &fbr;
    CNpc* npc      = as_base->add_npc("rnd_npc_fb", 51, 52, 53, 54, 1, 0);
    ASSERT_NE(npc, nullptr);
    ScopedNpc fb_guard(npc);

    EXPECT_TRUE(map_keys_are(npc, 51, 52, 53, 54, 1, 0));
    EXPECT_STREQ("room_add_npc_fb", npc->querystr("start_city"));
    // 副本房间里的 NPC 不允许被拾起
    EXPECT_NE('\0', npc->querystr("no_get")[0]);
}

class RoomAddNpcBindTest : public BindEngineTest {
protected:
    CRoom room;
    CFBRoom fbr;
    ScopedNpc npc_guard;

    void SetUp() override
    {
        BindEngineTest::SetUp();
        room.set_name("摆放房", "bind_add_npc");
        room.set("base_name", "bind_add_npc");
        fbr.set_name("副本房", "bind_add_npc_fb");
        fbr.set("base_name", "bind_add_npc_fb");
    }
    void TearDown() override
    {
        npc_guard.discard();
        BindEngineTest::TearDown();
    }
};

// ---------------------------------------------------------------------
// 契约 5：绑定正向路径返回 NPC，六键按参数写入
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcBindTest, BindAddNpc_ReturnsNpcWithMapKeys)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "bind_npc_full");
    for (int v : {61, 62, 63, 64, 7, 1, 0}) lua_pushinteger(L, v);

    int k = call(l_add_npc, 9);
    ASSERT_EQ(1, k);
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    CNpc* npc = static_cast<CNpc*>(lua_bind_check_object(L, -1));
    lua_pop(L, k);
    ASSERT_NE(nullptr, npc);
    npc_guard.reset(npc);

    EXPECT_TRUE(map_keys_are(npc, 61, 62, 63, 64, 7, 1));
    EXPECT_STREQ("bind_add_npc", npc->querystr("start_city"));
}

// ---------------------------------------------------------------------
// 契约 4+5：绑定层不得自带副本知识——经 self:add_npc 落到副本房间时，
// 不可拾取约束由多态分派产生。
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcBindTest, BindAddNpc_OnFbRoom_AppliesFbRule)
{
    lua_bind_push_object(L, &fbr);
    lua_pushstring(L, "bind_npc_fb");
    for (int v : {71, 72, 73, 74, 4, 0, 0}) lua_pushinteger(L, v);

    int k = call(l_add_npc, 9);
    ASSERT_EQ(1, k);
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    CNpc* npc = static_cast<CNpc*>(lua_bind_check_object(L, -1));
    lua_pop(L, k);
    ASSERT_NE(nullptr, npc);
    npc_guard.reset(npc);

    EXPECT_TRUE(map_keys_are(npc, 71, 72, 73, 74, 4, 0));
    EXPECT_NE('\0', npc->querystr("no_get")[0]);
}

// ---------------------------------------------------------------------
// 契约 5：坐标参数全部可缺省（房间脚本 self:add_npc("name") 的常见写法）
// ---------------------------------------------------------------------
TEST_F(RoomAddNpcBindTest, BindAddNpc_OptionalArgsDefaultToZero)
{
    lua_bind_push_object(L, &room);
    lua_pushstring(L, "bind_npc_bare");

    int k = call(l_add_npc, 2);
    ASSERT_EQ(1, k);
    ASSERT_EQ(LUA_TUSERDATA, lua_type(L, -1));
    CNpc* npc = static_cast<CNpc*>(lua_bind_check_object(L, -1));
    lua_pop(L, k);
    ASSERT_NE(nullptr, npc);
    npc_guard.reset(npc);

    EXPECT_TRUE(map_keys_are(npc, 0, 0, 0, 0, 0, 0));
}
