// test_fightroom_team.cpp - CFightRoom 双队（team0/team1）分支的团队操作测试
//
// 覆盖 GetTeamMsg / getTeamList / Do_LeaveTeam / Join_Team / heart_beat 中
// 按 team 下标复制的两队分支，作为把这些分支抽成公共函数时的行为基线。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include <cstring>
#include <string>
#include <vector>

#include "Char.h"
#include "Container.h"
#include "FightRoom.h"
#include "HeartBeat.h"
#include "Room.h"
#include "Npc.h"  // CNpc（mine 房销毁用例）
#include "User.h"

extern MAP2USER g_user_list;  // find_online 命中注册
#include "test_lua_env.h"

// 记录被投递到客户端的命令，用于验证“描述真的发到了玩家”
class RecordingUser : public CUser {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        sent++;
        if (msg) msgs.push_back(msg);
    }
    int sent = 0;
    std::vector<std::string> msgs;
};

// expose protected members under test (advance_ap / run_team_turn)
struct FightExposed : CFightRoom {
    using CFightRoom::advance_ap;
    using CFightRoom::m_tick;
    using CFightRoom::run_team_turn;
};

class FightRoomTeamTest : public ::testing::Test {
protected:
    CRoom room;
    FightExposed fr;
    CChar alpha;
    CChar beta;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "rteam");
        fr.set_name("fight", "frteam");
        alpha.set_name("alpha", "alpha1");
        beta.set_name("beta", "beta1");
        alpha.set("hp", (long)10);
        alpha.set("max_hp", (long)100);
        beta.set("hp", (long)100);
        beta.set("max_hp", (long)100);
        alpha.move(&room);
        beta.move(&room);
        fr.move(&room);
    }

    // 比试用例经 Begin_Fight 注册全局心跳，摘除后再析构，避免悬挂指针
    void TearDown() override { set_heart_beat_obj(&fr, 0); }

    void PlaceTeam(int team, CChar* c)
    {
        fr.m_Team[team].obj[0]      = c;
        fr.m_Team[team].fighter_num = 1;
    }
};

// GetTeamMsg: TA/TB 前缀与成员名列表（GetInfo 协议串的队伍字段）
TEST_F(FightRoomTeamTest, GetTeamMsgPrefixesBothTeams)
{
    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);

    char msg[512] = "";
    fr.get_team_msg(msg, 0);
    EXPECT_NE(std::strstr(msg, "TA=Team 0"), nullptr);
    EXPECT_NE(std::strstr(msg, "alpha"), nullptr);
    EXPECT_EQ(std::strstr(msg, "TB="), nullptr);

    char msgB[512] = "";
    fr.get_team_msg(msgB, 1);
    EXPECT_NE(std::strstr(msgB, "TB=Team 1"), nullptr);
    EXPECT_NE(std::strstr(msgB, "beta"), nullptr);
    EXPECT_EQ(std::strstr(msgB, "TA="), nullptr);
}

// getTeamList: team 非法时按 target 所在队伍解析；target 不在任何队伍返回空
TEST_F(FightRoomTeamTest, GetTeamListResolvesTeamFromTarget)
{
    PlaceTeam(0, &alpha);
    CChar gamma;
    gamma.set_name("gamma", "gamma1");
    fr.m_Team[1].obj[0]      = &beta;
    fr.m_Team[1].obj[1]      = &gamma;
    fr.m_Team[1].fighter_num = 2;

    auto list = fr.get_team_list(-1, &beta, 4);
    ASSERT_NE(list, nullptr);
    ASSERT_EQ((int)list->size(), 2);
    EXPECT_EQ((*list)[0], &beta);
    EXPECT_EQ((*list)[1], &gamma);

    CChar stranger;
    stranger.set_name("stranger", "s1");
    EXPECT_EQ(fr.get_team_list(-1, &stranger, 4), nullptr);
}

// Do_LeaveTeam: 清槽位、扣人数、删战斗临时变量，另一队不受影响
TEST_F(FightRoomTeamTest, DoLeaveTeamClearsSlotAndTemp)
{
    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);
    alpha.set_temp("fight/is_fighting", 1);
    alpha.set_temp("fight/fight_team", 0);
    alpha.set_temp("fight/ap", 300);
    alpha.set_temp("fight/join_time", 100);
    alpha.set_temp("fight/join_exp", 400);
    alpha.set_temp("pending/PK", 1);

    fr.do_leave_team(&alpha);

    EXPECT_EQ(fr.m_Team[0].fighter_num, 0);
    EXPECT_EQ(fr.m_Team[0].obj[0], nullptr);
    EXPECT_EQ(alpha.query_temp("fight/is_fighting"), 0);
    EXPECT_EQ(alpha.query_temp("fight/fight_team"), 0);
    EXPECT_EQ(alpha.query_temp("fight/ap"), 0);
    EXPECT_EQ(alpha.query_temp("fight/join_time"), 0);
    EXPECT_EQ(alpha.query_temp("fight/join_exp"), 0);
    EXPECT_EQ(alpha.query_temp("pending/PK"), 0);
    EXPECT_EQ(fr.m_Team[1].fighter_num, 1);
    EXPECT_EQ(fr.m_Team[1].obj[0], &beta);

    fr.do_leave_team(&beta);
    EXPECT_EQ(fr.m_Team[1].fighter_num, 0);
    EXPECT_EQ(fr.m_Team[1].obj[0], nullptr);
}

// Join_Team: 两队分别入队并写入各自战斗临时变量
TEST_F(FightRoomTeamTest, JoinTeamAssignsBothSides)
{
    EXPECT_EQ(fr.join_team(&alpha, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 1);
    EXPECT_EQ(fr.m_Team[0].obj[0], &alpha);
    EXPECT_EQ(alpha.query_temp("fight/fight_team"), 0);
    EXPECT_EQ(alpha.query_temp("fight/is_fighting"), 1);

    EXPECT_EQ(fr.join_team(&beta, CFightRoom::FIGHT_TEAM_B), 1);
    EXPECT_EQ(fr.m_Team[1].fighter_num, 1);
    EXPECT_EQ(fr.m_Team[1].obj[0], &beta);
    EXPECT_EQ(beta.query_temp("fight/fight_team"), 1);
    EXPECT_EQ(beta.query_temp("fight/is_fighting"), 1);
}

// Join_Team: 非法队伍号不入队，但战斗临时变量照常写入
TEST_F(FightRoomTeamTest, JoinTeamInvalidTeamKeepsRoster)
{
    EXPECT_EQ(fr.join_team(&alpha, 5), 1);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 0);
    EXPECT_EQ(fr.m_Team[1].fighter_num, 0);
    EXPECT_EQ(alpha.query_temp("fight/is_fighting"), 1);
    EXPECT_EQ(alpha.query_temp("fight/fight_team"), 5);
}

// Join_Team: 队伍满员拒绝入队，被拒者不写战斗临时变量
TEST_F(FightRoomTeamTest, JoinTeamRejectsWhenFull)
{
    CChar c1, c2, c3, c4, extra;
    c1.set_name("c1", "c1");
    c2.set_name("c2", "c2");
    c3.set_name("c3", "c3");
    c4.set_name("c4", "c4");
    extra.set_name("extra", "ex1");

    EXPECT_EQ(fr.join_team(&c1, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c2, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c3, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c4, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 4);

    EXPECT_EQ(fr.join_team(&extra, CFightRoom::FIGHT_TEAM_A), 0);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 4);
    EXPECT_EQ(extra.query_temp("fight/is_fighting"), 0);
}

// heart_beat: AP 累加覆盖两队；team0 比试认输提前返回，回合不推进
TEST_F(FightRoomTeamTest, HeartBeatAdvancesApForBothTeams)
{
    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);
    fr.begin_fight(1, 100);  // 比试：血量百分比 <= 100 即认输
    fr.heart_beat();         // tick 门控：首个心跳只递增 tick
    fr.heart_beat();         // AP 累加 -> team0 认输 -> 提前 return
    EXPECT_GT(alpha.query_temp("fight/ap"), 0);
    EXPECT_GT(beta.query_temp("fight/ap"), 0);
    EXPECT_EQ(fr.turn, 1);
}

// heart_beat: 回合落在 team1 侧时由 team1 认输，回合同样不推进
TEST_F(FightRoomTeamTest, HeartBeatYieldsOnTeam1Side)
{
    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);
    fr.begin_fight(1, 100);
    fr.turn = 2;  // 偶数回合 -> team1 行动
    fr.heart_beat();
    fr.heart_beat();
    EXPECT_EQ(fr.turn, 2);
}

// 普通战斗（非阈值对局）：心跳必须推进回合且不被异常吞掉
TEST_F(FightRoomTeamTest, HeartBeatNormalFightRuns)
{
    alpha.set("hp", (long)100000);
    alpha.set("max_hp", (long)100000);
    beta.set("hp", (long)100000);
    beta.set("max_hp", (long)100000);

    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);
    fr.begin_fight(0);  // 普通战斗：无阈值判负

    fr.heart_beat();  // tick 门控
    fr.heart_beat();  // 第 1 回合：team0 出招

    EXPECT_EQ(fr.turn, 2);  // 回合推进（未被异常吞掉）

    fr.heart_beat();  // tick 门控
    fr.heart_beat();  // 第 2 回合：team1 出招

    EXPECT_EQ(fr.turn, 3);
}

// 战斗房内非参与者玩家也必须收到攻击描述（战斗描述按房内玩家投递）
TEST_F(FightRoomTeamTest, AttackDispatchesToUsersInFightRoom)
{
    alpha.set("race", "妖怪");
    beta.set("race", "妖怪");
    alpha.set("hp", (long)100000);
    alpha.set("max_hp", (long)100000);
    beta.set("hp", (long)100000);
    beta.set("max_hp", (long)100000);

    RecordingUser viewer;
    viewer.set_name("viewer", "vw1");

    alpha.move(&fr);  // 攻击双方与旁观玩家都在战斗房内
    beta.move(&fr);
    viewer.move(&fr);

    PlaceTeam(0, &alpha);
    PlaceTeam(1, &beta);

    alpha.attack(&beta);

    // TYPE_REGULAR 的战斗描述载荷形如
    // &C=..&A=攻击者&V=目标..&T=武器类型&M=描述&
    bool got_combat_payload = false;
    for (const auto& m : viewer.msgs) {
        if (m.find("&A=") != std::string::npos
            && m.find("&T=") != std::string::npos) {
            got_combat_payload = true;
        }
    }
    EXPECT_TRUE(got_combat_payload);  // 战斗描述送到了房内玩家
}

// Can_Join 准入规则：空缺允许、满员拒绝、阈值对局期间一律拒绝
TEST_F(FightRoomTeamTest, CanJoinGuardsAdmission)
{
    CChar c1, c2, c3, c4;
    c1.set_name("c1", "c1");
    c2.set_name("c2", "c2");
    c3.set_name("c3", "c3");
    c4.set_name("c4", "c4");

    EXPECT_EQ(fr.can_join(0, &alpha), 1);  // 非对局且未满
    EXPECT_EQ(fr.join_team(&c1, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c2, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c3, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c4, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.can_join(0, &alpha), 0);  // 满员
    EXPECT_EQ(fr.can_join(1, &alpha), 1);  // 对方队伍未满

    fr.begin_fight(1, 50);  // 进入阈值对局
    EXPECT_EQ(fr.can_join(1, &alpha), 0);
}

// Query 属性开关：Lua 脚本靠它判定战斗房间与可拾取性
TEST_F(FightRoomTeamTest, QueryFightRoomAttributes)
{
    EXPECT_EQ(fr.query_attr(CMainObject::CAN_GET), 0);
    EXPECT_EQ(fr.query_attr(CMainObject::IS_FIGHTROOM), 1);
    EXPECT_EQ(fr.query_attr(CMainObject::IS_ITEM), 1);  // 回落 CItem::Query
}

// GetInfo: locked 状态必须携带 l=原因 字段
TEST_F(FightRoomTeamTest, GetInfoLockedStatus)
{
    fr.set("status", "locked");
    fr.set("reason", "pk");
    char buf[1024] = "";
    fr.get_info(buf, nullptr);
    std::string s(buf);
    EXPECT_NE(s.find("l=pk&"), std::string::npos);
}

// create: 默认保持 4 人上限，nFlag=1 提升到 8 人
TEST_F(FightRoomTeamTest, CreateSetsFighterCapacity)
{
    CChar c1, c2, c3, c4, c5;
    c1.set_name("c1", "c1");
    c2.set_name("c2", "c2");
    c3.set_name("c3", "c3");
    c4.set_name("c4", "c4");
    c5.set_name("c5", "c5");

    fr.create();  // 默认分支：上限不变
    EXPECT_EQ(fr.join_team(&c1, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c2, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c3, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c4, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.join_team(&c5, CFightRoom::FIGHT_TEAM_A), 0);

    fr.create(1);  // 标志分支：上限提升到 8
    EXPECT_EQ(fr.join_team(&c5, CFightRoom::FIGHT_TEAM_A), 1);
    EXPECT_EQ(fr.m_Team[0].fighter_num, 5);
}

// ====================================================================
// 深路：do_flee / leave_team / select_member / advance_ap / run_team_turn
//       / end_fight(mine) / join_team 随战链 / get_team_list 抽样
// ====================================================================

TEST_F(FightRoomTeamTest, DoFleeMovesMeToOuterRoom)
{
    CChar runner;
    runner.set_name("逃兵", "runner1");
    runner.move(&fr);

    EXPECT_EQ(0, fr.do_flee(&runner)) << "逃走返回 0(94-100)";
    EXPECT_EQ(static_cast<CContainer*>(&room), runner.environment())
        << "逃到战斗场外层(100)";
}

TEST_F(FightRoomTeamTest, LeaveTeamViaPublicWrapper)
{
    fr.join_team(&alpha, CFightRoom::FIGHT_TEAM_A);
    ASSERT_EQ(1, fr.m_Team[0].fighter_num);

    fr.leave_team(&alpha);
    EXPECT_EQ(0, fr.m_Team[0].fighter_num)
        << "leave_team 转 do_leave_team 清位(225)";
}

TEST_F(FightRoomTeamTest, SelectMemberWalksSecondSlotEventually)
{
    fr.m_Team[0].obj[0]      = &alpha;
    fr.m_Team[0].obj[1]      = &beta;
    fr.m_Team[0].fighter_num = 2;

    bool second_hit = false;
    for (int i = 0; i < 24 && !second_hit; i++) {
        CChar* p = fr.select_member(0);
        ASSERT_NE(nullptr, p);
        if (p == &beta) second_hit = true;
    }
    EXPECT_TRUE(second_hit)
        << "随机 idx 落第二人 → 遍历计数(216)（24 轮未中概率 2^-24）";
}

TEST_F(FightRoomTeamTest, AdvanceApHandlesCeilingBranches)
{
    fr.m_Team[0].obj[0]      = &alpha;
    fr.m_Team[0].fighter_num = 1;
    alpha.set("dex", (long)30);

    alpha.set_temp("fight/ap", 490);
    fr.advance_ap();
    EXPECT_EQ(500, alpha.query_temp("fight/ap"))
        << "逼近 500 顶格时按超出量封顶(336)";

    alpha.set_temp("fight/ap", 600);
    fr.advance_ap();
    EXPECT_EQ(600, alpha.query_temp("fight/ap")) << "已满不再累加(338)";
}

TEST_F(FightRoomTeamTest, EmptyEnemyThreeTicksEndFight)
{
    PlaceTeam(0, &alpha);
    fr.m_tick = 0;

    EXPECT_TRUE(fr.run_team_turn(0)) << "敌空首轮仅记 tick";
    EXPECT_EQ(1, fr.m_tick) << "m_tick++(355)";
    EXPECT_TRUE(fr.run_team_turn(0));
    EXPECT_EQ(2, fr.m_tick);
    EXPECT_FALSE(fr.run_team_turn(0)) << "tick>=3 → end_fight(356-357)";

    // end_fight 对自身 DESTRUCT_OB 入队：fixture 栈对象必须出队防悬垂
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fr);
    }
}

TEST_F(FightRoomTeamTest, EndFightMineRoomDestroysNpcsAndAnnounces)
{
    RecordingUser witness;
    witness.set_name("旁观", "witness1");
    witness.set("hp", (long)100);
    witness.set("max_hp", (long)100);
    witness.set_temp("client/serial", 1);  // 已连客户端 → 收广播
    witness.move(&room);

    fr.set("mine", 1);
    auto* minion = new CNpc();
    minion->set_name("打手", "minion1");
    minion->move(&fr);

    fr.end_fight();  // mine 房: NPC 被销毁(310) + 广播结束

    EXPECT_GT(witness.sent, 0) << "结束广播送达房间用户";
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(&fr);
    }
}

TEST_F(FightRoomTeamTest, JoinTeamInvitesPartyFriendsToFollow)
{
    RecordingUser captain;
    captain.set_name("队长", "captain1");
    captain.set("id", "captain1");
    captain.set("hp", (long)100);
    captain.set("max_hp", (long)100);
    captain.set("mp", (long)100);
    captain.set("max_mp", (long)100);
    captain.set("level", (long)10);
    captain.set("combat_exp", (long)1000);
    captain.set("con", (long)20);
    captain.set("dex", (long)10);

    RecordingUser follower;
    follower.set_name("跟班", "follower1");
    follower.set("id", "follower1");
    follower.set("hp", (long)100);
    follower.set("max_hp", (long)100);
    follower.set("mp", (long)100);
    follower.set("max_mp", (long)100);
    follower.set("level", (long)10);
    follower.set("combat_exp", (long)1000);
    follower.set("con", (long)20);
    follower.set("dex", (long)10);

    captain.move(&room);  // join 命令对环境 present(战斗场)：人在外房
    follower.move(&room);
    follower.delete_temp("client/loading");  // 入场加载态挡住 command(178)
    captain.set_temp("party/number", 1);
    captain.set_temp("party/friend_follower1", 1);
    follower.set_temp("party/follow", 1);

    fr.join_team(&captain, CFightRoom::FIGHT_TEAM_A);

    EXPECT_EQ(2, fr.m_Team[0].fighter_num) << "好友随战入队(439-460)";
    EXPECT_GE((long)follower.query_temp("cmd/cnt"), 1)
        << "join_team 对好友发出了随战 join 命令(452-453)";
}

TEST_F(FightRoomTeamTest, JoinTeamInvitesPartyMasterChain)
{
    RecordingUser master;
    master.set_name("师父", "master1");
    master.set("id", "master1");
    master.set("hp", (long)100);
    master.set("max_hp", (long)100);
    master.set("mp", (long)100);
    master.set("max_mp", (long)100);
    master.set("level", (long)10);
    master.set("combat_exp", (long)1000);
    master.set("con", (long)20);
    master.set("dex", (long)10);
    g_user_list[master.object_id()] = &master;  // find_online 可命中

    RecordingUser joiner;
    joiner.set_name("弟子", "disciple1");
    joiner.set("id", "disciple1");
    joiner.set("hp", (long)100);
    joiner.set("max_hp", (long)100);
    joiner.set("mp", (long)100);
    joiner.set("max_mp", (long)100);
    joiner.set("level", (long)10);
    joiner.set("combat_exp", (long)1000);
    joiner.set("con", (long)20);
    joiner.set("dex", (long)10);

    master.move(&room);  // 同上：人在外房对战斗场 join
    joiner.move(&room);
    joiner.delete_temp("client/loading");
    master.delete_temp("client/loading");
    joiner.set_temp("party/master", "master1");
    joiner.set_temp("party/follow", 1);

    fr.join_team(&joiner, CFightRoom::FIGHT_TEAM_A);

    EXPECT_EQ(2, fr.m_Team[0].fighter_num) << "师父随战入队(464,467,468)";
    EXPECT_GE((long)master.query_temp("cmd/cnt"), 1)
        << "join_team 对在线师父发出了随战 join 命令(464,467,468)";

    g_user_list.erase(master.object_id());
}

TEST_F(FightRoomTeamTest, GetTeamListRandomSamplingWhenNumShort)
{
    CChar c3, c4, c5, c6, c7, c8;
    c3.set_name("丙", "c3");
    c4.set_name("丁", "c4");
    c5.set_name("戊", "c5");
    c6.set_name("己", "c6");
    c7.set_name("庚", "c7");
    c8.set_name("辛", "c8");
    c3.move(&room);
    c4.move(&room);
    c5.move(&room);
    c6.move(&room);
    c7.move(&room);
    c8.move(&room);

    CChar* roster[8] = {&alpha, &beta, &c3, &c4, &c5, &c6, &c7, &c8};
    for (int i = 0; i < 8; i++) fr.m_Team[0].obj[i] = roster[i];
    fr.m_Team[0].fighter_num = 8;

    // count=8 / num=7：3 轮随机后 sn≤6 的单轮概率 ~28% → 补漏段(189-193)
    // 在 20 轮内几乎必然触发；轮内不截断时 size 可到 8，故断言区间。
    bool any_six = false;
    for (int i = 0; i < 20; i++) {
        auto list = fr.get_team_list(0, nullptr, 7);
        ASSERT_NE(nullptr, list) << "抽样返回名单";
        int sz = static_cast<int>(list->size());
        EXPECT_GE(sz, 7) << "至少补到 num";
        EXPECT_LE(sz, 8) << "不超过队内人数";
        if (sz == 7) any_six = true;
    }
    EXPECT_TRUE(any_six) << "20 轮内出现恰为 7 的抽样(含补漏路径 189-193)";
}

TEST_F(FightRoomTeamTest, GetTeamListResolvesTargetTeam)
{
    fr.m_Team[1].obj[0]      = &alpha;
    fr.m_Team[1].fighter_num = 1;

    auto list = fr.get_team_list(-1, &alpha, 4);
    ASSERT_NE(nullptr, list) << "team=-1 时按 target 反查所在队(147-155)";
    EXPECT_GE(static_cast<int>(list->size()), 1);
}
