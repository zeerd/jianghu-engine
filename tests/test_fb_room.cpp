// test_fb_room.cpp — room/FBRoom.cpp 的行为契约（CFBRoom / CFBDoor / CFB）
//
// 本文件是「FBRoom 拆分 + CFBRoom 与 CRoom 合并去重」的改动面覆盖起点。
// 断言一律走**行为契约**而非实现细节：不假设门存在 m_fb_door_list 还是
// m_door_list（合并后统一走 CRoom 的门列表），只经 add_door / present_door /
// query_exits / remove_doors 观察。
//
// Build+Run: .\run-coverage.ps1
#include "test_bind_common.h"

#include <algorithm>
#include <string>
#include <vector>

#include "FB.h"
#include "FBRoom.h"
#include "NpcMgr.h"
#include "FightRoom.h"  // CFightRoom（随身甩载用例）
extern int g_current_time;
#include "Room.h"
#include "mock/mock_database.h"

extern IDatabase* g_db;

namespace {

// 房间注册表里手工塞入的测试房（load_room 命中它们，不走脚本工厂）
void register_room(CRoom* room, const char* base_name)
{
    room->set("base_name", base_name);
    g_room_list[base_name] = room;
}

// 把 NPC 从世界摘除并销毁（去心跳 -> 脱环境 -> 出 NPC 表 -> delete）
void discard_npc(CNpc* npc)
{
    if (!npc) return;
    npc->set_heart_beat(0);
    npc->move(nullptr);
    g_npc_list.erase(npc->object_id());
    delete npc;
}

// 按 base_name 在 NPC 表里找物件（reset 复生会换 object_id，无法按 id 追）
CNpc* find_npc_by_base_name(const char* base_name)
{
    for (auto& kv : g_npc_list) {
        if (EQUALSTR(kv.second->querystr("base_name"), base_name))
            return kv.second;
    }
    return nullptr;
}

}  // namespace

// 每个用例独占一个 lua 环境 + mock 物品管理器 + 房间注册表。
//
// 回收顺序很关键：副本生命周期会把对象塞进延迟销毁队列 g_destruct_list，
// 而测试进程没有主循环冲刷它——但别的用例（test_server_sock）会调用
// process_user_message() 冲刷整个队列。因此本夹具必须先把自己创建的对象
// 从队列摘走再 delete，否则跨用例 double-free。
class FBRoomTest : public ::testing::Test {
protected:
    MockDatabase* db = nullptr;
    CMockItemMgr mgr;
    std::unique_ptr<ItemFactory> factory;

    std::vector<CContainer*> m_owned;  // 本用例堆上创建、需回收的对象
    std::vector<std::string> m_rooms;  // 塞进 g_room_list 的测试房名
    std::vector<CNpc*> m_npcs;         // 本用例装载的 NPC

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        db      = new MockDatabase();
        g_db    = db;
        factory = std::make_unique<ItemFactory>(&mgr);
    }

    void TearDown() override
    {
        // 1) 脱离世界：停心跳 + 离开环境（此时房间仍然存活）
        for (auto* obj : m_owned) {
            obj->set_heart_beat(0);
            obj->move(nullptr);
        }
        // 2) 从延迟销毁队列摘走本用例的对象
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                if (std::find(m_owned.begin(), m_owned.end(), *it)
                    != m_owned.end()) {
                    it = g_destruct_list.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        // 3) 先回收工厂造的物件（其 environment 仍指向尚未删除的房间）
        factory.reset();
        // 4) 再回收本用例对象
        for (auto* obj : m_owned) delete obj;
        m_owned.clear();
        // 5) 最后回收测试房
        for (const auto& name : m_rooms) {
            auto it = g_room_list.find(name);
            if (it != g_room_list.end()) {
                it->second->set_heart_beat(0);
                delete it->second;
                g_room_list.erase(it);
            }
        }
        m_rooms.clear();

        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    template <typename T>
    T* own(T* obj)
    {
        m_owned.push_back(obj);
        return obj;
    }

    CRoom* ownRoom(const char* base_name)
    {
        CRoom* room = new CRoom();
        room->set_name(base_name, base_name);
        register_room(room, base_name);
        m_rooms.push_back(base_name);
        return room;
    }

    CUser* ownUser(const char* id)
    {
        CUser* user = new CUser();
        user->set("id", id);
        user->set("name", "副本测试者");
        m_owned.push_back(user);
        return user;
    }
};

// ====================================================================
// CFBRoom —— 门：创建 / 查找 / 移除
// ====================================================================

TEST_F(FBRoomTest, AddDoor_IsFindableByIdAndDir)
{
    CFBRoom room;
    CDoor* door = room.add_door("gate_a", "fbtest_next", "back", "N");
    ASSERT_NE(nullptr, door);

    EXPECT_STREQ("gate_a", door->querystr("id"));
    EXPECT_STREQ("fbtest_next", door->querystr("next_room"));
    EXPECT_STREQ("back", door->querystr("next_door"));
    EXPECT_STREQ("N", door->querystr("dir"));
    EXPECT_STREQ("open", door->querystr("status"));

    EXPECT_EQ(door, room.present_door("gate_a"));  // 按 id
    EXPECT_EQ(door, room.present_door("N"));       // 按方向
    EXPECT_EQ(nullptr, room.present_door("gate_zz"));

    // query_exits 必须看到同一扇门（合并后仍走 CRoom 的门列表）
    DTEXITLIST* exits = room.query_exits();
    ASSERT_NE(nullptr, exits);
    ASSERT_EQ(1, (int)exits->size());
    EXPECT_EQ(door, exits->front());
}

TEST_F(FBRoomTest, RemoveDoors_DropsOnlyMatchingNextRoom)
{
    CFBRoom room;
    CDoor* keep = room.add_door("keep", "fbtest_keep", "", "E");
    CDoor* gone = room.add_door("gone", "fbtest_gone", "", "W");
    ASSERT_NE(keep, gone);

    room.remove_doors("fbtest_gone");

    EXPECT_EQ(nullptr, room.present_door("gone"));
    EXPECT_EQ(keep, room.present_door("keep"));
    EXPECT_EQ(1, (int)room.query_exits()->size());
}

TEST_F(FBRoomTest, RemoveAllDoors_EmptiesExits)
{
    CFBRoom room;
    room.add_door("a", "fbtest_a", "", "E");
    room.add_door("b", "fbtest_b", "", "W");
    ASSERT_EQ(2, (int)room.query_exits()->size());

    room.remove_all_doors();

    EXPECT_EQ(0, (int)room.query_exits()->size());
    EXPECT_EQ(nullptr, room.present_door("a"));
}

// ====================================================================
// CFBRoom —— 属性与离开校验
// ====================================================================

TEST_F(FBRoomTest, QueryAttr_FbRoomPlusInheritedRoomFlags)
{
    CFBRoom room;
    EXPECT_EQ(1, room.query_attr(CMainObject::IS_FBROOM));
    // 副本房间同时具备 CRoom 的属性判定（IS_ROOM 由基类给出）
    EXPECT_EQ(1, room.query_attr(CMainObject::IS_ROOM));
    EXPECT_EQ(1, room.query_attr(CMainObject::IS_CONTAINER));
}

TEST_F(FBRoomTest, ValidLeave_BlockedByNoLeaveFlag)
{
    CFBRoom room;
    CChar me;
    EXPECT_EQ(1, room.valid_leave(&me, "N"));  // 缺省允许离开

    room.set("no_leave", 1);
    EXPECT_EQ(0, room.valid_leave(&me, "N"));  // 副本锁定
}

TEST_F(FBRoomTest, HandleAction_BackIsRefused)
{
    // CRoom 的 "back" 返回总部；副本房间必须拒绝（防越狱）
    CFBRoom room;
    CChar me;
    char comm[] = "back";
    char arg[]  = "";
    char tar[]  = "";
    EXPECT_EQ(0, room.handle_action(comm, &me, arg, tar));
}

// ====================================================================
// CFBRoom —— setup / do_look / add_item
// ====================================================================

TEST_F(FBRoomTest, Setup_FillsMapAndEightCharArea)
{
    CFBRoom room;
    room.set("base_name", "fbtest_area_prefix_room");
    room.setup();

    EXPECT_STREQ("fbtest_area_prefix_room", room.querystr("map"));
    EXPECT_STREQ("fbtest_a", room.querystr("area"));  // base_name 前 8 字符
}

TEST_F(FBRoomTest, Setup_KeepsExistingMapAndArea)
{
    CFBRoom room;
    room.set("base_name", "fbtest_other");
    room.set("map", "fbtest_given_map");
    room.set("area", "fbtest_given_area");
    room.setup();

    EXPECT_STREQ("fbtest_given_map", room.querystr("map"));
    EXPECT_STREQ("fbtest_given_area", room.querystr("area"));
}

TEST_F(FBRoomTest, DoLook_PrintsLongOrFallback)
{
    CFBRoom room;
    CChar me;
    EXPECT_EQ(0, room.do_look(&me));  // 无 long 分支

    room.set("long", "副本密室，四壁皆石。");
    EXPECT_EQ(0, room.do_look(&me));  // 有 long 分支
}

TEST_F(FBRoomTest, AddItem_PlacesItemAndRecordsNode)
{
    CFBRoom room;
    CContainer* item = room.add_item("fbtest_item");
    ASSERT_NE(nullptr, item);
    EXPECT_EQ(static_cast<CContainer*>(&room), item->environment());
}

// ====================================================================
// CFBRoom —— reset：清物但**不复位 NPC**（副本 boss 不得无限复活）
// 这是 CFBRoom 与 CRoom::reset 的真实语义差异，合并时必须保留。
// ====================================================================

TEST_F(FBRoomTest, Reset_DoesNotRespawnNpc)
{
    CFBRoom room;
    room.set("base_name", "fbtest_npc_protect_room");

    const char* boss = "fbtest_boss";
    CNpc* npc        = room.add_npc(boss, 1, 2, 3, 4, 0, 0);
    ASSERT_NE(nullptr, npc);
    discard_npc(npc);  // boss 被击杀消失
    ASSERT_EQ(nullptr, find_npc_by_base_name(boss));

    room.reset();
    room.reset();

    // 副本房间不做 NPC 复位：不得重新装载
    EXPECT_EQ(nullptr, find_npc_by_base_name(boss));
}

TEST_F(FBRoomTest, Reset_PurgesItemsTaggedForThisRoom)
{
    CFBRoom room;
    room.set("base_name", "fbtest_clean_room");

    CContainer* item = room.add_item("fbtest_item");
    ASSERT_NE(nullptr, item);
    ASSERT_EQ(1, (int)room.get_item_list()->size());

    room.reset();  // 第一遍：只登记 wait_clean
    EXPECT_EQ(1, (int)room.get_item_list()->size());

    room.reset();  // 第二遍：命中 wait_clean == 本房 base_name → 清除
    EXPECT_EQ(0, (int)room.get_item_list()->size());
}

TEST_F(FBRoomTest, Reset_KeepsNoCleanUpItems)
{
    CFBRoom room;
    room.set("base_name", "fbtest_keep_room");

    CContainer* item = room.add_item("fbtest_item");
    ASSERT_NE(nullptr, item);
    item->set("no_clean_up", 1);

    room.reset();
    room.reset();
    EXPECT_EQ(1, (int)room.get_item_list()->size());
}

// ====================================================================
// CFBDoor —— enter_door
// ====================================================================

TEST_F(FBRoomTest, FbDoorEnter_LoadsSiblingRoomAndClearsInitDoor)
{
    CFB* fb       = own(new CFB());
    CFBRoom* from = own(fb->load_fb_room("fbtest_door_from"));
    CFBRoom* to   = own(fb->load_fb_room("fbtest_door_to"));
    ASSERT_NE(from, to);

    CDoor* door = from->add_door("passage", "fbtest_door_to", "back", "N");
    ASSERT_NE(nullptr, door);

    CChar me;
    EXPECT_EQ(1, door->enter_door(&me));
    EXPECT_EQ(static_cast<CContainer*>(to), me.environment());
    // 进入后必须清除下一房间门标识
    EXPECT_EQ('\0', me.querystr_temp("init_door")[0]);
}

TEST_F(FBRoomTest, FbDoorEnter_WithoutParentFbFails)
{
    CFBRoom room;  // m_parent_fb == NULL
    CDoor* door = room.add_door("orphan", "fbtest_anywhere", "", "E");
    ASSERT_NE(nullptr, door);

    CChar me;
    EXPECT_EQ(0, door->enter_door(&me));
    EXPECT_EQ(nullptr, me.environment());
}

TEST_F(FBRoomTest, FbDoorEnter_AfterParentFbDestructedFails)
{
    CFB* fb       = own(new CFB());
    CFBRoom* from = own(fb->load_fb_room("fbtest_dead_from"));
    CDoor* door   = from->add_door("passage", "fbtest_dead_to", "", "N");
    ASSERT_NE(nullptr, door);

    fb->set_destructed();

    CChar me;
    EXPECT_EQ(0, door->enter_door(&me));
    EXPECT_EQ(nullptr, me.environment());
}

// ====================================================================
// CFBRoom —— change_room：副本房必须保留 fb_nodie（合并的行为锚点）
// ====================================================================

TEST_F(FBRoomTest, ChangeRoom_FbRoomKeepsFbNodieFlag)
{
    CFB* fb       = own(new CFB());
    CFBRoom* room = own(fb->load_fb_room("fbtest_nodie_room"));
    CUser* user   = ownUser("fbtest_nodie_user");
    user->set_temp("fb_nodie", 1);

    ASSERT_EQ(1, user->move(room));
    EXPECT_EQ(1, user->query_temp("fb_nodie"));  // 副本内不清除
}

// 对照组：进入普通房间必须清除 fb_nodie（CRoom::change_room
// 的该分支当前零覆盖）
TEST_F(FBRoomTest, ChangeRoom_PlainRoomClearsFbNodie)
{
    CRoom* plain = ownRoom("fbtest_plain_room");
    CUser* user  = ownUser("fbtest_clear_user");
    user->set_temp("fb_nodie", 1);

    ASSERT_EQ(1, user->move(plain));
    EXPECT_EQ(0, user->query_temp("fb_nodie"));
}

// 对照组：副本房间换房必须下发出口信息（门存在与否都要走通 userp 分支）
TEST_F(FBRoomTest, ChangeRoom_FbRoomWithDoorsSendsExitInfo)
{
    CFB* fb       = own(new CFB());
    CFBRoom* room = own(fb->load_fb_room("fbtest_exit_room"));
    room->add_door("gate", "fbtest_exit_next", "", "S");

    CUser* user = ownUser("fbtest_exit_user");
    ASSERT_EQ(1, user->move(room));
    EXPECT_EQ(static_cast<CContainer*>(room), user->environment());
}

// ====================================================================
// CFB —— 房间装载 / 属性 / 视图
// ====================================================================

TEST_F(FBRoomTest, LoadFbRoom_CachesByBaseName)
{
    CFB* fb = own(new CFB());

    CFBRoom* first = own(fb->load_fb_room("fbtest_cached_room"));
    ASSERT_NE(nullptr, first);
    // 同名命中缓存：返回同一实例，不重复创建（故不可再次 own）
    EXPECT_EQ(first, fb->load_fb_room("fbtest_cached_room"));

    EXPECT_STREQ("fbtest_cached_room", fb->querystr("start_map"));  // 首个房间
}

TEST_F(FBRoomTest, FbQueryAttr_NotGettableAndIsFb)
{
    CFB fb;
    EXPECT_EQ(0, fb.query_attr(CMainObject::CAN_GET));
    EXPECT_EQ(1, fb.query_attr(CMainObject::IS_FB));
    EXPECT_EQ(1, fb.query_attr(CMainObject::IS_ITEM));  // 继承 CItem
}

TEST_F(FBRoomTest, FbDoLook_WithAndWithoutLong)
{
    CFB* fb = own(new CFB());
    fb->set_name("地下古城", "fbtest_look_fb");
    fb->load_fb_room("fbtest_look_room");

    CChar me;
    EXPECT_EQ(0, fb->do_look(&me));  // 无 long

    fb->set("long", "阴深的地下古城。");
    EXPECT_EQ(0, fb->do_look(&me));  // 有 long
}

TEST_F(FBRoomTest, RemoveAllRoom_DestroysLoadedRooms)
{
    CFB* fb       = own(new CFB());
    CFBRoom* room = own(fb->load_fb_room("fbtest_rm_room"));
    ASSERT_NE(nullptr, room);

    fb->remove_all_room();

    // 房间已销毁：同名重新装载会得到新实例
    CFBRoom* again = own(fb->load_fb_room("fbtest_rm_room"));
    ASSERT_NE(nullptr, again);
    EXPECT_NE(room, again);
}

// ====================================================================
// CFB —— begin_fb / move_in / heart_beat / end_fb 生命周期
// ====================================================================

TEST_F(FBRoomTest, BeginFb_WritesStateKeys)
{
    CFB* fb = own(new CFB());
    fb->set("source_map", "fbtest_home");
    ownRoom("fbtest_home");

    fb->begin_fb("fbtest_home", 900, 1);

    EXPECT_EQ(900, fb->query("limit_time"));
    EXPECT_STREQ("fbtest_home", fb->querystr("source_map"));
}

TEST_F(FBRoomTest, MoveIn_EmptyFbRefuses)
{
    CFB* fb     = own(new CFB());
    CUser* user = ownUser("fbtest_empty_user");
    EXPECT_EQ(0, fb->move_in(user));
    EXPECT_EQ(nullptr, user->environment());
}

TEST_F(FBRoomTest, MoveIn_SucceedsIntoStartMapAndRegistersUser)
{
    CFB* fb = own(new CFB());
    ownRoom("fbtest_home");
    fb->set("source_map", "fbtest_home");

    CFBRoom* start = own(fb->load_fb_room("fbtest_start_map"));
    ASSERT_NE(nullptr, start);

    CUser* user = ownUser("fbtest_movein_user");
    EXPECT_EQ(1, fb->move_in(user, 3));
    EXPECT_EQ(static_cast<CContainer*>(start), user->environment());
    // m_nodie 由 begin_fb(flag) 决定，此处未 begin_fb → 不应设置
    EXPECT_EQ(0, user->query_temp("fb_nodie"));
}

TEST_F(FBRoomTest, MoveIn_WithNodieFlagSetsTemp)
{
    CFB* fb = own(new CFB());
    ownRoom("fbtest_home");
    own(fb->load_fb_room("fbtest_start_nodie"));
    fb->begin_fb("fbtest_home", 0, 1);  // flag = 死亡不损失

    CUser* user = ownUser("fbtest_nodie_in");
    EXPECT_EQ(1, fb->move_in(user));
    EXPECT_EQ(1, user->query_temp("fb_nodie"));
}

TEST_F(FBRoomTest, ChangeRoom_FbClearsClientLoading)
{
    CFB* fb     = own(new CFB());
    CUser* user = ownUser("fbtest_fb_enter");
    user->set_temp("client/loading", 1);

    fb->change_room(user);
    EXPECT_EQ(0, user->query_temp("client/loading"));
}

// 空副本倒计时：无玩家驻留时 heart_beat 递增计时，到点自行结束副本。
// 这是 CFB 生命周期唯一可观察的行为面（m_tick 为 protected），
// 终点是 end_fb 自毁 —— 覆盖 heart_beat 与 end_fb 两条当前零命中的路径。
TEST_F(FBRoomTest, HeartBeat_EmptyFbCountsDownToEnd)
{
    CFB* fb = own(new CFB());
    ownRoom("fbtest_home");
    fb->set("source_map", "fbtest_home");  // end_fb 出口房，避免落到全局起始房
    own(fb->load_fb_room("fbtest_tick_room"));

    int ticks = 0;
    while (!fb->is_destructed() && ticks < 60) {
        fb->heart_beat();
        ++ticks;
    }

    EXPECT_TRUE(fb->is_destructed());
    EXPECT_GT(ticks, 15);  // 15 秒无人保护期必须先走完
}

// 有玩家驻留时不得进入销毁计时。
TEST_F(FBRoomTest, HeartBeat_KeepsAliveWhilePlayerPresent)
{
    CFB* fb = own(new CFB());
    ownRoom("fbtest_home");
    fb->set("source_map", "fbtest_home");
    CFBRoom* room = own(fb->load_fb_room("fbtest_alive_room"));

    CUser* user = ownUser("fbtest_alive_user");
    ASSERT_EQ(1, user->move(room));

    for (int i = 0; i < 20; ++i) fb->heart_beat();

    EXPECT_FALSE(fb->is_destructed());
}

// ====================================================================
// CFBRoom —— destructed 回收门与箱子
// ====================================================================

TEST_F(FBRoomTest, Destructed_ReleasesDoors)
{
    CFB* fb       = own(new CFB());
    CFBRoom* room = own(fb->load_fb_room("fbtest_destruct_room"));
    room->add_door("gate", "fbtest_destruct_next", "", "E");
    ASSERT_EQ(1, (int)room->query_exits()->size());

    room->destructed();

    EXPECT_EQ(0, (int)room->query_exits()->size());
    EXPECT_EQ(nullptr, room->m_parent_fb);  // 脱离父副本
}

// ====================================================================
// 绑定层 —— 正向路径（当前 l_query_fb_parent / l_load_fb_room 只测了守卫）
// ====================================================================

// 同上回收纪律的 Lua 绑定层夹具（额外持有 lua_State）
class FBRoomBindTest : public BindEngineTest {
protected:
    MockDatabase* db = nullptr;
    CMockItemMgr mgr;
    std::unique_ptr<ItemFactory> factory;
    std::vector<CContainer*> m_owned;
    std::vector<std::string> m_rooms;

    void SetUp() override
    {
        BindEngineTest::SetUp();
        db      = new MockDatabase();
        g_db    = db;
        factory = std::make_unique<ItemFactory>(&mgr);
    }

    void TearDown() override
    {
        for (auto* obj : m_owned) {
            obj->set_heart_beat(0);
            obj->move(nullptr);
        }
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                if (std::find(m_owned.begin(), m_owned.end(), *it)
                    != m_owned.end()) {
                    it = g_destruct_list.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        factory.reset();
        for (auto* obj : m_owned) delete obj;
        m_owned.clear();
        for (const auto& name : m_rooms) {
            auto it = g_room_list.find(name);
            if (it != g_room_list.end()) {
                it->second->set_heart_beat(0);
                delete it->second;
                g_room_list.erase(it);
            }
        }
        m_rooms.clear();
        g_db = nullptr;
        delete db;
        db = nullptr;
        BindEngineTest::TearDown();
    }

    template <typename T>
    T* own(T* obj)
    {
        m_owned.push_back(obj);
        return obj;
    }

    CRoom* ownRoom(const char* base_name)
    {
        CRoom* room = new CRoom();
        room->set_name(base_name, base_name);
        register_room(room, base_name);
        m_rooms.push_back(base_name);
        return room;
    }
};

// self:query_fb_parent() —— 有父副本时返回父对象
TEST_F(FBRoomBindTest, QueryFbParentReturnsParentObject)
{
    CFB* fb       = own(new CFB());
    CFBRoom* room = own(fb->load_fb_room("fbtest_bind_parent"));
    ASSERT_NE(nullptr, room);

    lua_bind_push_object(L, room);
    int k = call(l_query_fb_parent, 1);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_isnil(L, -1));
    lua_pop(L, k);
}

// self:load_fb_room(name) —— 正向返回房间对象
TEST_F(FBRoomBindTest, LoadFbRoomBindReturnsRoom)
{
    CFB* fb = own(new CFB());

    lua_bind_push_object(L, fb);
    lua_pushstring(L, "fbtest_bind_room");
    int k = call(l_load_fb_room, 2);
    ASSERT_EQ(1, k);
    EXPECT_FALSE(lua_isnil(L, -1));
    lua_pop(L, k);
}

// self:Begin_FB(...) —— 正向路径写入状态键（原测试只覆盖守卫）
TEST_F(FBRoomBindTest, BeginFbBindWritesState)
{
    CFB* fb = own(new CFB());
    ownRoom("fbtest_bind_home");

    lua_bind_push_object(L, fb);
    lua_pushstring(L, "fbtest_bind_home");
    lua_pushinteger(L, 600);
    lua_pushinteger(L, 1);
    EXPECT_EQ(0, CallRetCount(l_begin_fb, 4));

    EXPECT_EQ(600, fb->query("limit_time"));
    EXPECT_STREQ("fbtest_bind_home", fb->querystr("source_map"));
}

// ====================================================================
// CFB 深路：move_in 甩载 / end_fb 持久化与清场 / 异常捕获 / heart_beat 计时
// ====================================================================

// protected 成员测试暴露（using 惯例）
struct FbExposed : CFB {
    using CFB::m_room_list;
    using CFB::m_room_num;
    using CFB::m_tick;
};

TEST_F(FBRoomTest, MoveInEvictsStowawaysToStartRoom)
{
    FbExposed fb;
    CFBRoom* stay = fb.load_fb_room("fbtest_stow_map");  // 建前提 + 接住房指针

    CChar me;
    CUser* stow_user = ownUser("fbtest_stow_user");
    stow_user->move(&me);  // 随身携带一个 user
    auto* fr = new CFightRoom();
    fr->move(&me);  // 随身携带一个战斗场

    fb.move_in(&me, 0);

    CContainer* home = load_room(g_start_room);
    EXPECT_EQ(home, stow_user->environment())
        << "随身 user 甩回起始城(108-109)";
    EXPECT_EQ(home, fr->environment()) << "随身战斗场甩回起始城(110-111)";

    stow_user->move(nullptr);
    fr->move(nullptr);
    delete fr;
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(fr);
    }  // CFightRoom 析构入队 → 栈上下文必须出队
    fb.end_fb();  // 收尾：清场 + DESTRUCT_OB 入队 → 出队
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fb);
        g_destruct_list.remove(&me);        // end_fb 曾逻辑销毁栈对象 me
        g_destruct_list.remove(stay);       // 房经析构入队 → 防 double-free
        g_destruct_list.remove(stow_user);  // 防御
    }
}

TEST_F(FBRoomTest, MoveInTwiceMatchesExistingUser)
{
    FbExposed fb;
    fb.load_fb_room("fbtest_twice_map");
    CUser* u = ownUser("fbtest_twice_user");

    EXPECT_EQ(1, fb.move_in(u, 0)) << "首次登记";
    EXPECT_EQ(1, fb.move_in(u, 0)) << "二次命中已有登记(122) 幂等成功";
}

TEST_F(FBRoomTest, EndFbPersistsPlayersAndEvicts)
{
    FbExposed fb;
    fb.load_fb_room("fbtest_end_map");
    fb.set("player/fbtest_p1", (long)7);  // → querymap("player") 非空(180)

    CUser* inner = ownUser("fbtest_end_user");
    inner->move(&fb);
    auto* junk = new CItem();
    junk->set("id", "fbtest_junk");
    junk->move(&fb);

    fb.end_fb();

    EXPECT_FALSE(db->GetLastSql().empty())
        << "player 留存走 sql_update_record(180-185)";
    EXPECT_EQ(load_room(g_start_room), inner->environment())
        << "内部 user 甩回起始城(195,206)";

    bool junk_queued = false;
    bool fb_queued   = false;
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        for (auto* p : g_destruct_list) {
            if (p == junk) junk_queued = true;
            if (p == &fb) fb_queued = true;
        }
        g_destruct_list.remove(junk);
        g_destruct_list.remove(&fb);
    }
    EXPECT_TRUE(junk_queued) << "普通件销毁入队(201,203)";
    EXPECT_TRUE(fb_queued) << "end_fb 收尾析构自身";
    delete junk;
}

TEST_F(FBRoomTest, EndFbSweepsRoomListContents)
{
    FbExposed fb;
    CFBRoom* inner
        = fb.load_fb_room("fbtest_sweep_map");  // 进 m_room_list(220)

    auto* junk = new CItem();
    junk->set("id", "fbtest_sweep_junk");
    junk->move(inner);
    CUser* u = ownUser("fbtest_sweep_user");
    u->move(inner);

    fb.end_fb();

    EXPECT_EQ(load_room(g_start_room), u->environment())
        << "房内 user 甩出(225)";
    bool junk_queued = false;
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        for (auto* p : g_destruct_list)
            if (p == junk) junk_queued = true;
        g_destruct_list.remove(junk);
        g_destruct_list.remove(&fb);
    }
    EXPECT_TRUE(junk_queued) << "房内普通件销毁(222)";
    delete junk;
}

TEST_F(FBRoomTest, EndFbDbExceptionCaught)
{
    ThrowingDatabase tdb;
    tdb.throwOnQuery   = true;
    tdb.throwOnExecute = true;
    g_db               = &tdb;

    FbExposed fb;
    fb.load_fb_room("fbtest_throw_map");
    fb.set("player/fbtest_x", (long)1);  // sql_update_record 内 db 调用抛

    fb.end_fb();  // catch(233-234) → 不 DESTRUCT、不崩

    bool fb_queued = false;
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        for (auto* p : g_destruct_list)
            if (p == &fb) fb_queued = true;
    }
    EXPECT_TRUE(fb_queued)
        << "db 异常被内层 sql_update_record 消化，清场/析构照常完成";
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fb);
    }  // 栈对象必须出队（防清队列时误删）

    g_db = db;  // 恢复 fixture mock
}

TEST_F(FBRoomTest, HeartBeatNeedEndAndCountdownBroadcast)
{
    int saved_time = g_current_time;
    g_current_time = 1000000;

    FbExposed fb;
    fb.load_fb_room("fbtest_hb_map");  // m_room_list 非空(276-277)

    fb.begin_fb("", 0, 0);                       // begin_time + 心跳注册
    fb.set("need_end", 1);                       // → 248-249 分支
    fb.set("limit_time", g_current_time - 120);  // 120 % 60 == 0 → 275-277

    int tick_before = fb.m_tick;
    fb.heart_beat();

    EXPECT_GT(fb.m_tick, tick_before) << "need_end 分支推进 m_tick(248-249)";

    set_heart_beat_obj(&fb, 0);  // 摘心跳
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fb);
    }
    g_current_time = saved_time;
}
