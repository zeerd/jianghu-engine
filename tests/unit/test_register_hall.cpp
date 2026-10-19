// test_register_hall.cpp — src/net/RegisterHall.cpp 注册大厅副本化
//
// 契约：enter_register_hall(CChar*) 每次调用把玩家送进一个**独立**的
// rk_fb 副本实例（实例房 base_name 仍为 rk，脚本仍是 rk.lua，故注册白名单、
// 注册专员、update_age 的大厅判定全部照旧生效），实例空置后由心跳回收；
// 实例不可用时回落全局 rk 大厅，且不得残留半成品实例。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <algorithm>
#include <string>
#include <vector>

#include "FB.h"
#include "GameInit.h"   // g_current_time
#include "Global.h"     // g_item_list / g_npc_list / g_room_list
#include "HeartBeat.h"  // g_destruct_list：延迟销毁队列
#include "Npc.h"
#include "RegisterHall.h"
#include "Room.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_register_fb.h"
#include "test_state_recorder.h"

extern IDatabase* g_db;

namespace {

constexpr int kNow = 1700000000;

// 指定房间内的注册专员（实例房与全局大厅各有一份，按环境区分）
CNpc* FindClerkIn(CContainer* room)
{
    for (auto& kv : g_npc_list) {
        if (EQUALSTR(kv.second->querystr("base_name"), "register_clerk")
            && kv.second->environment() == room)
            return kv.second;
    }
    return nullptr;
}

}  // namespace

class RegisterHallTest : public ::testing::Test {
protected:
    MockDatabase* db = nullptr;
    RegisterFbItemMgr item_mgr;
    IItemMgr* saved_mgr    = nullptr;
    int saved_current_time = 0;
    std::vector<StateRecordingUser*> m_users;

    // SetUp 时快照：TearDown 只回收本用例新增的对象
    std::vector<CContainer*> m_live_at_setup;
    std::vector<int32_t> m_live_npcs;
    std::vector<std::string> m_live_rooms;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        db   = new MockDatabase();
        g_db = db;

        saved_mgr             = g_item_mgr;
        g_item_mgr            = &item_mgr;
        item_mgr.real_load_fb = true;  // 缺省走实例支，兜底支用例再关掉

        saved_current_time = g_current_time;
        g_current_time     = kNow;

        for (auto& kv : g_npc_list) m_live_npcs.push_back(kv.first);
        for (auto& kv : g_room_list) m_live_rooms.push_back(kv.first);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup.assign(g_destruct_list.begin(),
                                   g_destruct_list.end());
        }
    }

    void TearDown() override
    {
        // 1) 副本：清场后副本房/专员/副本本体进延迟队列并被就地回收
        recycle_fbs();
        // 2) 本用例的玩家（已被 end_fb 甩到全局大厅，环境仍存活）
        for (StateRecordingUser* user : m_users) {
            user->set_heart_beat(0);
            user->move(nullptr);
            delete user;
        }
        m_users.clear();
        // 3) 本用例入队的延迟销毁对象（心跳已自毁的副本等）
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob = *it;
                bool known     = std::find(m_live_at_setup.begin(),
                                           m_live_at_setup.end(), ob)
                                 != m_live_at_setup.end();
                if (known) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete ob;
            }
        }
        // 4) 全局大厅的注册专员，5) 全局大厅/fb_registry 等房间
        discard_new_npcs(m_live_npcs);
        discard_new_rooms(m_live_rooms);

        g_item_mgr = saved_mgr;
        g_db       = nullptr;
        delete db;
        db             = nullptr;
        g_current_time = saved_current_time;
    }

    // 造一个玩家并送进注册大厅
    StateRecordingUser* EnterHall()
    {
        StateRecordingUser* user = new StateRecordingUser();
        user->set_name("过客", "reg_hall_guest");
        m_users.push_back(user);
        enter_register_hall(user);
        return user;
    }
};

// =====================================================================
// 实例创建：每次进入都是一个独立副本房
// =====================================================================
TEST_F(RegisterHallTest, Enter_CreatesFbInstanceOfHall)
{
    StateRecordingUser* me = EnterHall();

    CContainer* room = me->environment();
    ASSERT_NE(nullptr, room) << "玩家必须落地";
    EXPECT_EQ(1, room->query_attr(CMainObject::IS_FBROOM));
    EXPECT_STREQ(REGISTER_HALL_NAME, room->querystr("base_name"));
    EXPECT_NE(nullptr, FindClerkIn(room)) << "实例房应载入注册专员";

    std::vector<CFB*> fbs = live_register_fbs();
    ASSERT_EQ(1, (int)fbs.size());
    EXPECT_STREQ(REGISTER_HALL_NAME, fbs[0]->querystr("source_map"))
        << "回收时把人送回全局大厅";
    EXPECT_EQ(kNow, fbs[0]->query("begin_time")) << "空置倒计时已起";
}

// =====================================================================
// 隔离：两名玩家各占一个实例，互不挤同一房间
// =====================================================================
TEST_F(RegisterHallTest, Enter_TwoPlayers_LandInSeparateInstances)
{
    StateRecordingUser* first  = EnterHall();
    StateRecordingUser* second = EnterHall();

    ASSERT_NE(nullptr, first->environment());
    ASSERT_NE(nullptr, second->environment());
    EXPECT_NE(first->environment(), second->environment())
        << "两名玩家不得共用同一实例";
    EXPECT_EQ(2, (int)live_register_fbs().size());
}

// =====================================================================
// 生命周期：房内有人不回收，空置后由心跳回收
// =====================================================================
TEST_F(RegisterHallTest, HeartBeat_KeepsOccupiedAndRecyclesVacantInstance)
{
    StateRecordingUser* me = EnterHall();
    std::vector<CFB*> fbs  = live_register_fbs();
    ASSERT_EQ(1, (int)fbs.size());
    CFB* fb = fbs[0];

    for (int i = 0; i < 20; ++i) fb->heart_beat();
    EXPECT_FALSE(fb->is_destructed()) << "房内有人时不得回收";

    CRoom* out = new CRoom();
    out->set_name("测试房", "register_hall_test_out");
    ASSERT_EQ(1, me->move(out));

    for (int i = 0; i < 40 && !fb->is_destructed(); ++i) fb->heart_beat();
    EXPECT_TRUE(fb->is_destructed()) << "空置后由心跳回收";

    me->move(nullptr);
    delete out;
}

// =====================================================================
// 兜底：实例不可用时回落全局大厅，且不残留半成品实例
// =====================================================================
TEST_F(RegisterHallTest, Enter_InstanceUnavailable_FallsBackToGlobalHall)
{
    item_mgr.real_load_fb = false;

    StateRecordingUser* me = EnterHall();

    CContainer* room = me->environment();
    ASSERT_NE(nullptr, room);
    EXPECT_EQ(0, room->query_attr(CMainObject::IS_FBROOM));
    EXPECT_STREQ(REGISTER_HALL_NAME, room->querystr("base_name"));
    EXPECT_TRUE(live_register_fbs().empty()) << "不得残留半成品实例";
}

TEST_F(RegisterHallTest, Enter_RoomlessInstance_RecycledThenFallsBack)
{
    item_mgr.roomless_fb = true;  // 副本建出来了但接不住玩家

    StateRecordingUser* me = EnterHall();

    CContainer* room = me->environment();
    ASSERT_NE(nullptr, room);
    EXPECT_EQ(0, room->query_attr(CMainObject::IS_FBROOM));
    EXPECT_STREQ(REGISTER_HALL_NAME, room->querystr("base_name"));
    EXPECT_TRUE(live_register_fbs().empty()) << "半成品实例就地回收";
    EXPECT_EQ(1, item_mgr.remove_item_call_count)
        << "只有副本本体经 destruct 摘出 g_item_list";
}

// =====================================================================
// 实例房继承大厅脚本：注册白名单照常生效
// =====================================================================
TEST_F(RegisterHallTest, FbInstance_RoomHonorsRegisterWhitelist)
{
    StateRecordingUser* me = EnterHall();
    CRoom* room            = dynamic_cast<CRoom*>(me->environment());
    ASSERT_NE(nullptr, room);
    me->clear();

    char look[]     = "look";
    char password[] = "password";
    char reg[]      = "register";
    char empty[]    = "";
    char tar[]      = "";

    EXPECT_EQ(1, room->handle_action(look, me, empty, tar));
    EXPECT_TRUE(me->has("没有信号")) << "白名单外命令被大厅拦截";
    EXPECT_EQ(0, room->handle_action(password, me, empty, tar))
        << "password 放行";
    EXPECT_EQ(1, room->handle_action(reg, me, empty, tar));
    EXPECT_EQ(1, me->query_temp("regclerk/step")) << "register 进入注册流程";
}

// =====================================================================
// 实例房与全局大厅同名同脚本：年龄判定一并照旧生效
// =====================================================================
TEST_F(RegisterHallTest, FbInstance_RoomCountsAsHallForAge)
{
    StateRecordingUser* me = EnterHall();
    ASSERT_NE(nullptr, me->environment());

    me->update_age();                // 首次调用只记时间戳
    g_current_time = kNow + 172800;  // 两天后再算一次
    me->update_age();

    EXPECT_EQ(0, me->query("mud_age")) << "大厅（含实例）里不涨年龄";
    EXPECT_EQ(0, me->query("age")) << "年龄字段在大厅里原样不动";

    CRoom* out = new CRoom();
    out->set_name("测试房", "register_hall_age_out");
    ASSERT_EQ(1, me->move(out));
    me->update_age();
    EXPECT_EQ(172800, me->query("mud_age")) << "离开大厅后恢复增长";
    EXPECT_EQ(16, me->query("age")) << "24 小时一岁";

    me->move(nullptr);
    delete out;
}
