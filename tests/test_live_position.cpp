// test_live_position.cpp — 实时位置（map/* temp 键）的写入形状
//
// 客户端地图同步用的临时坐标，四键/五键/两键三种形状：
//   - 完整矩形（bx by ex ey ac 各自独立）：mv（玩家移动回报）
//   - 收拢点 + ac（bx=ex=x, by=ey=y, ac）：loc（重连定位）、drop（丢弃落点）、
//     cmd_hit_ob（开战时战斗精灵继承发起者坐标）
//   - 仅终点（ex ey）：fight（切磋选点，只覆盖目的地、保留当前 bx/by/ac）、
//     do_bishi（比试精灵继承发起者当前终点）
// 这些用例锁定每个入口实际写入的键与值，作为把重复写入抽成公共 helper
// 时的行为基线。
#include "test_lua_env.h"

#include <cstring>
#include <vector>

#include "Char.h"
#include "CmdStandard.h"
#include "CmdUser.h"
#include "FightRoom.h"
#include "GameObject.h"
#include "Global.h"
#include "HeartBeat.h"
#include "Item.h"
#include "Room.h"
#include "User.h"
#include "mock/mock_itemmgr.h"

namespace {

// 战斗精灵工厂记账：cmd_hit_ob / do_bishi 经 load_item("fight_room") 取用的
// CFightRoom 由工厂 new 出并登记，用例结束统一摘心跳、脱离、销毁。
std::vector<CFightRoom*> g_created_fight_rooms;

CContainer* MakeFightRoom(const char* name)
{
    if (name && strcmp(name, "fight_room") == 0) {
        CFightRoom* r = new CFightRoom();
        g_created_fight_rooms.push_back(r);
        return r;
    }
    return nullptr;
}

}  // namespace

class LivePositionTest : public ::testing::Test {
protected:
    CRoom env;
    CChar* attacker;
    CChar* victim;
    CMockItemMgr* mgr = nullptr;
    IItemMgr* saved_mgr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        g_created_fight_rooms.clear();

        env.set_name("定位房", "live_pos_env");
        env.set("base_name", "live_pos_env");
        env.set("max_items", 30);

        attacker = new CChar();
        attacker->set_name("甲", "live_atk");
        attacker->set("id", "live_atk");
        attacker->move(&env);

        victim = new CChar();
        victim->set_name("乙", "live_vic");
        victim->set("id", "live_vic");
        victim->move(&env);

        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &MakeFightRoom;
        g_item_mgr        = mgr;
    }

    void TearDown() override
    {
        attacker->move(nullptr);
        victim->move(nullptr);
        for (CFightRoom* r : g_created_fight_rooms) {
            set_heart_beat_obj(r, 0);
            r->move(nullptr);
            delete r;
        }
        g_created_fight_rooms.clear();
        delete attacker;
        delete victim;

        g_item_mgr = saved_mgr;
        delete mgr;
    }

    static int CallCmdStandard(const char* cmd, CChar* me, int32_t target,
                               char* arg)
    {
        CCmdStandard c;
        return c.handlecommand((char*)cmd, me, target, arg);
    }
    static int CallCmdUser(const char* cmd, CChar* me, int32_t target,
                           char* arg)
    {
        CCmdUser c;
        return c.handlecommand((char*)cmd, me, target, arg);
    }
};

// ---------------- mv：完整矩形，五键各自独立 ----------------
TEST_F(LivePositionTest, Mv_WritesIndependentRect)
{
    char arg[] = "1 2 3 4 5";
    EXPECT_EQ(1, CallCmdUser("mv", attacker, 0, arg));

    EXPECT_EQ(1, attacker->query_temp("map/bx"));
    EXPECT_EQ(2, attacker->query_temp("map/by"));
    EXPECT_EQ(3, attacker->query_temp("map/ex"));
    EXPECT_EQ(4, attacker->query_temp("map/ey"));
    EXPECT_EQ(5, attacker->query_temp("map/ac"));
}

// ---------------- loc：收拢点 + ac=0（重连定位） ----------------
TEST_F(LivePositionTest, Loc_WritesCollapsedPointWithAcZero)
{
    CUser* u = new CUser();
    u->set_name("定位者", "live_loc");
    u->set("id", "live_loc");
    u->move(&env);
    ASSERT_TRUE(u->query_temp("client/loading"));  // 进房挂回执标志

    char arg[] = "10 20";
    EXPECT_EQ(1, CallCmdUser("loc", u, 0, arg));

    EXPECT_EQ(10, u->query_temp("map/bx"));
    EXPECT_EQ(20, u->query_temp("map/by"));
    EXPECT_EQ(10, u->query_temp("map/ex"));
    EXPECT_EQ(20, u->query_temp("map/ey"));
    EXPECT_EQ(0, u->query_temp("map/ac"));
    EXPECT_FALSE(u->query_temp("client/loading"));  // loc 清除加载标志

    u->move(nullptr);
    delete u;
}

// ---------------- drop → do_drop：收拢点 + ac=0（丢弃落点） ----------------
TEST_F(LivePositionTest, Drop_WritesCollapsedPointWithAcZero)
{
    CItem* item = new CItem();
    item->set_name("石头", "live_rock");
    item->set("id", "live_rock");
    item->set("value", (long)100);  // 值钱，避免落地即销毁
    item->move(attacker);           // 先背在身上，drop 才取得到

    char arg[] = "12 34";
    EXPECT_EQ(1, CallCmdStandard("drop", attacker, item->object_id(), arg));

    EXPECT_EQ(12, item->query_temp("map/bx"));
    EXPECT_EQ(34, item->query_temp("map/by"));
    EXPECT_EQ(12, item->query_temp("map/ex"));
    EXPECT_EQ(34, item->query_temp("map/ey"));
    EXPECT_EQ(0, item->query_temp("map/ac"));
    EXPECT_EQ(static_cast<CContainer*>(&env), item->environment());

    item->move(nullptr);
    delete item;
}

// ---------------- fight：仅终点，保留当前 bx/by/ac ----------------
TEST_F(LivePositionTest, Fight_OverridesOnlyDestination)
{
    attacker->set_temp("map/bx", 7);
    attacker->set_temp("map/by", 8);
    attacker->set_temp("map/ac", 2);

    char arg[] = "5 6";
    // target=0 无切磋对象 → 命令在写入选点坐标后仍被拒；坐标已生效
    CallCmdStandard("fight", attacker, 0, arg);

    EXPECT_EQ(5, attacker->query_temp("map/ex"));
    EXPECT_EQ(6, attacker->query_temp("map/ey"));
    EXPECT_EQ(7, attacker->query_temp("map/bx"));  // 未动
    EXPECT_EQ(8, attacker->query_temp("map/by"));  // 未动
    EXPECT_EQ(2, attacker->query_temp("map/ac"));  // 未动
}

// ---------------- cmd_hit_ob：战斗精灵继承发起者坐标（收拢点，无 ac）
// ----------------
TEST_F(LivePositionTest, StartFight_FightRoomInheritsCollapsedPoint)
{
    char arg[] = "7 8";
    EXPECT_EQ(1, attacker->cmd_hit_ob(victim, arg));
    ASSERT_EQ(1u, g_created_fight_rooms.size());

    CFightRoom* fr = g_created_fight_rooms[0];
    EXPECT_EQ(7, fr->query_temp("map/bx"));
    EXPECT_EQ(8, fr->query_temp("map/by"));
    EXPECT_EQ(7, fr->query_temp("map/ex"));
    EXPECT_EQ(8, fr->query_temp("map/ey"));
    EXPECT_EQ(0, fr->query_temp("map/ac"));
}

// ---------------- do_bishi：比试精灵继承发起者当前终点（仅 ex/ey）
// ----------------
TEST_F(LivePositionTest, BiShi_FightRoomInheritsDestination)
{
    attacker->set_temp("map/ex", 30);
    attacker->set_temp("map/ey", 40);

    char name[]    = "比试场";
    CFightRoom* fr = do_bishi(attacker, victim, name, 50);
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(30, fr->query_temp("map/ex"));
    EXPECT_EQ(40, fr->query_temp("map/ey"));
}
