// test_bind_fightroom.cpp — binds in item/FightRoom.cpp
#include "test_bind_common.h"
#include "FightRoom.h"
#include "User.h"
#include "HeartBeat.h"  // set_heart_beat_obj 收尾
#include "Room.h"

class BindFightRoomTest : public BindTest {
protected:
    CFightRoom fr;
    void SetUp() override
    {
        BindTest::SetUp();
        fr.set_name("fight", "fr1");
    }
};

extern int l_get_fight_team_num(lua_State* L);  // decl name differs from source

// observer user: counts client pushes (broadcast witness)
class BindCountingUser : public CUser {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        (void)msg;
        (void)wait;
        sent++;
    }
    int sent = 0;
};

TEST_F(BindFightRoomTest, IsFightRoom)
{
    lua_bind_push_object(L, &fr);
    EXPECT_TRUE(CallBool(l_is_fight_room, 1));
}

TEST_F(BindFightRoomTest, CanJoin)
{
    // 契约：Can_Join 必返回一个布尔（值依赖门槛配置，不断言具体值）
    lua_bind_push_object(L, &fr);
    lua_pushinteger(L, 0);
    int k = call(l_can_join, 2);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isboolean(L, -1));
    lua_pop(L, k);
}

TEST_F(BindFightRoomTest, SetSingle)
{
    lua_bind_push_object(L, &fr);
    lua_pushboolean(L, 1);
    CallVoid(l_set_single, 2);
    EXPECT_TRUE(fr.m_single);
}

TEST_F(BindFightRoomTest, SelectMemberEmpty)
{
    lua_bind_push_object(L, &fr);
    lua_pushinteger(L, 0);
    int k = call(l_select_member, 2);
    EXPECT_EQ(1, k);
    lua_pop(L, k);
}

TEST_F(BindFightRoomTest, BeginEndFightGuard)
{
    // self not a CFightRoom -> the fr==NULL guard returns 0 (no side effects)
    CChar ch;
    lua_bind_push_object(L, &ch);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 50);
    EXPECT_EQ(0, CallRetCount(l_begin_fight, 3));

    lua_bind_push_object(L, &ch);
    EXPECT_EQ(0, CallRetCount(l_end_fight, 1));
}

TEST_F(BindFightRoomTest, JoinLeaveTeamGuard)
{
    // self is a plain CChar (not CFightRoom) -> fr==NULL guard returns 0
    CChar a;
    a.set_name("a", "a1");
    lua_bind_push_object(L, &a);
    lua_bind_push_object(L, &a);
    lua_pushinteger(L, 0);
    EXPECT_EQ(0, CallRetCount(l_join_team, 3));

    lua_bind_push_object(L, &a);
    lua_bind_push_object(L, &a);
    EXPECT_EQ(0, CallRetCount(l_leave_team, 2));
}

// helpers that require me->environment() to be a CFightRoom
TEST_F(BindFightRoomTest, TeamQueriesWithoutFightRoomEnv)
{
    CChar me;  // no environment -> nil / 0 branches
    lua_bind_push_object(L, &me);
    int k = call(l_get_team_list, 1);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);

    lua_bind_push_object(L, &me);
    EXPECT_EQ(0, CallInt(l_get_fight_team_num, 1));

    lua_bind_push_object(L, &me);
    lua_pushinteger(L, 0);
    k = call(l_get_fight_team_char, 2);
    EXPECT_EQ(1, k);
    EXPECT_TRUE(lua_isnil(L, -1));
    lua_pop(L, k);
}

// 构造函数必须初始化 heart_beat/回合控制依赖的成员（未初始化读到垃圾值）
TEST_F(BindFightRoomTest, ConstructorInitializesState)
{
    EXPECT_EQ(fr.turn, 0);
    EXPECT_EQ(fr.tick, 0);
    EXPECT_FALSE(fr.m_single);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 0);
    EXPECT_EQ(fr.m_Team[1].fighter_num, 0);
}

// GetInfo 输出协议串：N/I/P/a/U/W 字段齐全且值为可解析的整数
TEST_F(BindFightRoomTest, GetInfoProtocolFields)
{
    char buf[1024] = "";
    fr.get_info(buf, nullptr);
    std::string s(buf);
    EXPECT_NE(s.find("N="), std::string::npos);
    EXPECT_NE(s.find("&P="), std::string::npos);
    EXPECT_NE(s.find("&a="), std::string::npos);
    EXPECT_NE(s.find("&U="), std::string::npos);
    EXPECT_NE(s.find("&W="), std::string::npos);
}

// 比试认输分支：wimpy 阈值触发后提前 return，回合不推进
TEST_F(BindFightRoomTest, HeartBeatBishiYield)
{
    // 命令分发要求环境实现 IActionHandler（CRoom/CFightRoom），裸 CContainer
    // 会在 CmdHandle::command 的 dynamic_cast 处解引用空指针。
    CRoom room;
    room.set_name("room", "r1");
    CChar a, b;
    a.set_name("a", "a1");
    a.set("hp", (long)10);
    a.set("max_hp", (long)100);
    b.set_name("b", "b1");
    b.set("hp", (long)100);
    b.set("max_hp", (long)100);
    a.move(&room);
    b.move(&room);

    fr.m_Team[0].obj[0]      = &a;
    fr.m_Team[0].fighter_num = 1;
    fr.m_Team[1].obj[0]      = &b;
    fr.m_Team[1].fighter_num = 1;

    fr.begin_fight(1, 100);  // 比试：血量百分比 <= 100 即认输
    fr.heart_beat();         // tick 门控：首个心跳只递增 tick
    fr.heart_beat();         // 进入回合逻辑 -> a 认输 -> 提前 return
    EXPECT_EQ(fr.turn, 1);   // 认输分支 return，未走到 turn++

    set_heart_beat_obj(&fr, 0);  // 摘除全局心跳，避免悬挂指针
}

// ---- l_join_team / l_begin_fight 深路（经 call() pcall 上下文） ----
TEST_F(BindFightRoomTest, JoinTeamBindEnqueuesChar)
{
    CRoom tmp;  // join_team 需要环境（遍历同场成员/广播）
    tmp.set_name("场地", "jbind_room");
    fr.move(&tmp);

    CChar fighter;
    fighter.set_name("队员", "binder1");

    lua_bind_push_object(L, &fr);
    lua_bind_push_object(L, &fighter);
    lua_pushinteger(L, CFightRoom::FIGHT_TEAM_A);
    int k = call(l_join_team, 3);
    if (k > 0) lua_pop(L, k);

    EXPECT_EQ(1, fr.m_Team[0].fighter_num)
        << "Join_Team 接收 userdata 与 team(567-569)";
    fr.move(nullptr);  // 先出房再拆栈房
}

TEST_F(BindFightRoomTest, BeginFightBindSetsThresholdFlag)
{
    lua_bind_push_object(L, &fr);
    lua_pushinteger(L, 1);
    lua_pushinteger(L, 77);
    int k = call(l_begin_fight, 3);
    if (k > 0) lua_pop(L, k);

    EXPECT_TRUE(fr.is_threshold_match()) << "Begin_Fight 的 flag 生效(580)";
    set_heart_beat_obj(&fr, 0);  // begin_fight 注册了心跳 → 收尾摘除
}

// ---- l_end_fight / l_leave_team / l_get_team_list / l_get_team_count /
//      l_get_fight_team_char 深路（均经 call() pcall 上下文） ----
TEST_F(BindFightRoomTest, EndFightBindEvictsMembersAndAnnounces)
{
    CRoom tmp;
    tmp.set_name("场地", "ef_room");
    fr.move(&tmp);

    BindCountingUser witness;  // 外层旁观：收结束广播（观察面）
    witness.set_name("旁观", "ef_wit");
    witness.set("hp", (long)100);
    witness.set("max_hp", (long)100);
    witness.set_temp("client/serial", 1);
    witness.move(&tmp);

    auto* npc = new CNpc();
    npc->set_name("打手", "ef_npc");
    npc->move(&fr);

    lua_bind_push_object(L, &fr);
    int k = call(l_end_fight, 1);
    if (k > 0) lua_pop(L, k);

    EXPECT_GT(witness.sent, 0) << "结束广播送达(604)";
    EXPECT_EQ(static_cast<CContainer*>(&tmp), npc->environment())
        << "非 mine 成员疏散到外层(310)";
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fr);
    }
    fr.move(nullptr);
    npc->move(nullptr);
    delete npc;
}

TEST_F(BindFightRoomTest, LeaveTeamBindDequeuesChar)
{
    CRoom tmp;
    tmp.set_name("场地", "lt_room");
    fr.move(&tmp);

    CChar stray;
    stray.set_name("队员", "lt_stray");
    fr.join_team(&stray, CFightRoom::FIGHT_TEAM_A);
    ASSERT_EQ(1, fr.m_Team[0].fighter_num);

    lua_bind_push_object(L, &fr);
    lua_bind_push_object(L, &stray);
    int k = call(l_leave_team, 2);
    if (k > 0) lua_pop(L, k);

    EXPECT_EQ(0, fr.m_Team[0].fighter_num) << "Leave_Team 摘位(616-617)";
    fr.move(nullptr);
}

TEST_F(BindFightRoomTest, GetTeamListBindReturnsFilledTable)
{
    CRoom tmp;  // fr 需外层环境（与其余绑定用例一致）
    tmp.set_name("场地", "gtl_room");
    fr.move(&tmp);

    CChar me;
    me.set_name("队长", "gtl_me");
    me.move(&fr);  // 绑定要求 me.environment 为战斗场

    fr.m_Team[0].obj[0]      = &me;
    fr.m_Team[0].fighter_num = 1;

    lua_bind_push_object(L, &me);
    lua_pushinteger(L, 0);  // team
    lua_pushnil(L);         // target 略过
    lua_pushinteger(L, 4);  // num
    int k = call(l_get_team_list, 4);
    ASSERT_EQ(1, k) << "成功返回一张表(652)";
    if (lua_istable(L, -1)) {
        EXPECT_EQ(1u, static_cast<unsigned>(lua_rawlen(L, -1)))
            << "表内 1 名队员(657-666)";
    }
    if (k > 0) lua_pop(L, k);
    me.move(nullptr);
    fr.move(nullptr);
}

TEST_F(BindFightRoomTest, GetTeamCountBindReadsFightTeam)
{
    CRoom tmp;
    tmp.set_name("场地", "gtc_room");
    fr.move(&tmp);

    CChar me;
    me.set_name("统计员", "gtc_me");
    me.move(&fr);
    fr.m_Team[1].obj[0]      = &me;
    fr.m_Team[1].fighter_num = 1;
    me.set_temp("fight/fight_team", 1);

    lua_bind_push_object(L, &me);
    int k = call(l_get_fight_team_num, 1);
    ASSERT_EQ(1, k);
    if (k >= 1) {
        EXPECT_EQ(1, static_cast<int>(lua_tointeger(L, -1)))
            << "读己方队伍人数(684-685)";
        lua_pop(L, k);
    }
    me.move(nullptr);
    fr.move(nullptr);
}

TEST_F(BindFightRoomTest, GetFightTeamCharBindIndexesRoster)
{
    CRoom tmp;
    tmp.set_name("场地", "gfc_room");
    fr.move(&tmp);

    CChar me;
    me.set_name("点名官", "gfc_me");
    me.move(&fr);
    CChar mate;
    mate.set_name("同袍", "gfc_mate");
    fr.m_Team[0].obj[0]      = &me;
    fr.m_Team[0].obj[1]      = &mate;
    fr.m_Team[0].fighter_num = 2;
    me.set_temp("fight/fight_team", 0);

    lua_bind_push_object(L, &me);
    lua_pushinteger(L, 0);  // idx（绑定内 get_team_list 默认 num=1 → 单人表）
    int k = call(l_get_fight_team_char, 2);
    ASSERT_EQ(1, k);
    if (k >= 1) {
        CMainObject* got = lua_bind_check_object(L, -1);
        EXPECT_TRUE(got == static_cast<CMainObject*>(&me)
                    || got == static_cast<CMainObject*>(&mate))
            << "按 idx 回读到本队队员(702-711)";
        lua_pop(L, k);
    }
    me.move(nullptr);
    fr.move(nullptr);
}