// test_cmd_user_social.cpp - Unit tests for CmdUserSocial.cpp
//
// Tests social/game state underlying the command handlers.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "Npc.h"
#include "CmdUser.h"
#include "Channel.h"
#include "Global.h"  // g_channel / MAP2USER
#include "Mapping.h"
#include "test_state_recorder.h"
#include "test_lua_env.h"
#include "mock/mock_database.h"

extern MAP2USER g_user_list;
extern int g_current_time;
extern char g_notify_fail_msg[512];

// 暴露 protected set_inquiry：让测试 NPC 携带对话应答，使 do_talk 的
// 命中/未命中分支可观察（返回 1 vs notify_fail 返回 0）。
class TestTalkNpc : public CNpc {
public:
    using CNpc::set_inquiry;
};

class CmdUserSocialTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallTalk(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"talk", me, target, arg);
    }
    static int CallVote(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"vote", me, target, arg);
    }
    static int CallNickName(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"nickname", me, target, arg);
    }
    static int CallInvite(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"invite", me, target, arg);
    }
    static int CallAccept(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"accept", me, target, arg);
    }
    static int CallRecruitExp(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"rexp", me, target, arg);
    }
    static int CallMv(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"mv", me, target, arg);
    }
    static int CallLoc(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"loc", me, target, arg);
    }
    static int CallWho(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"who", me, target, arg);
    }
    // private static 仅夹具类可见（TEST_F 派生类不继承友元）
    static int CallDetermineTotal() { return CCmdUser::determine_total(); }
    static int CallCounting(CMapping* players, const char* myvariable)
    {
        return CCmdUser::counting(players, myvariable);
    }
    static void CallNullifyVote(CContainer* ob, int32_t vote_id)
    {
        CCmdUser::nullify_vote(ob, vote_id, 0);
    }

    CChar* me;
    CChar* other;  // 与 me 对话/交互的玩家（非 NPC）
    CNpc* npc;
    CContainer* room;
    MockDatabase* db = nullptr;
    std::vector<StateRecordingUser*> online_users;

    // 注册在线 CUser（find_player/users 走 g_user_list）；TearDown 统一摘除
    StateRecordingUser* MakeOnline(const char* id, const char* name)
    {
        StateRecordingUser* u = new StateRecordingUser();
        u->set_name(name, id);
        u->set("level", (long)50);
        u->set("hp", (long)100);
        u->move(room);
        u->delete_temp("client/loading");
        g_user_list[u->object_id()] = u;
        online_users.push_back(u);
        return u;
    }

    void SetUp() override
    {
        ensure_lua_env();
        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("age", (long)20);
        me->set("combat_exp", (long)100000);
        me->move(room);

        other = new CChar();
        other->set("name", "路人");
        other->set("id", "passer1");
        other->set("level", (long)50);
        other->set("hp", (long)100);
        other->set("age", (long)20);
        other->move(room);

        npc = new CNpc();
        npc->set("name", "测试NPC");
        npc->set("id", "test_npc");
        npc->move(room);

        db   = new MockDatabase();
        g_db = db;
    }

    void TearDown() override
    {
        for (auto* u : online_users) {
            g_user_list.erase(u->object_id());
            u->move(nullptr);
            delete u;
        }
        online_users.clear();
        // vote 测试残留：按用到的键清理全局频道状态
        static const char* vars[]
            = {"close_chat_votetarget1", "open_chat_votetarget1",
               "close_chat_votetarget2", "open_chat_votetarget2", "myvar_x"};
        for (auto* v : vars) {
            g_channel.voted_players.del(v);
            g_channel.votes_casted.del(v);
        }
        g_channel.del("cancel_vote601/myvariable");
        g_channel.del("cancel_vote601/choicename");
        g_channel.del("cancel_vote601/playername");
        g_db = nullptr;
        delete db;
        npc->move(nullptr);
        other->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete npc;
        delete other;
        delete me;
        delete room;
    }
};

TEST_F(CmdUserSocialTest, Talk_NoTarget)
{
    int result = CallTalk(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdUserSocialTest, Talk_ValidNpc)
{
    int result = CallTalk(me, npc->object_id(), (char*)"");
    // talk should succeed or at least not crash
    EXPECT_NE(result, -1);
}

TEST_F(CmdUserSocialTest, Vote_NoArgs)
{
    int result = CallVote(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdUserSocialTest, NpcIsLiving) { EXPECT_TRUE(living(npc)); }

TEST_F(CmdUserSocialTest, PlayerIsLiving) { EXPECT_TRUE(living(me)); }

TEST_F(CmdUserSocialTest, NickName_Temp)
{
    me->set_temp("nickname", "江湖侠客");
    EXPECT_STREQ(me->querystr_temp("nickname"), "江湖侠客");
}

// =====================================================================
// talk
// =====================================================================

TEST_F(CmdUserSocialTest, Talk_NoTalkEnvRejected)
{
    room->set("no_talk", (long)1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallTalk(me, npc->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里禁止和人说话。");
    room->del("no_talk");
}

TEST_F(CmdUserSocialTest, Talk_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallTalk(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要和谁讲话，看清楚点！");
}

TEST_F(CmdUserSocialTest, Talk_SelfRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallTalk(me, me->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方显然无法和你讲话。");
}

TEST_F(CmdUserSocialTest, Talk_NpcDispatchesToDoTalk)
{
    // NPC 无 inquiry 时 do_talk 返回 notify_fail（0）；有 inquiry 时返回 1
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallTalk(me, npc->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。");
}

TEST_F(CmdUserSocialTest, Talk_NpcWithInquiryListsTopics)
{
    TestTalkNpc* wise = new TestTalkNpc();
    wise->set_name("智者", "wise1");
    wise->set_inquiry("武学", "年轻人也要习武。", "请教武学");
    wise->move(room);

    char arg[256] = "";
    EXPECT_EQ(CallTalk(me, wise->object_id(), arg), 1);

    wise->move(nullptr);
    delete wise;
}

TEST_F(CmdUserSocialTest, Talk_FightingRejectsNonShuangErTarget)
{
    // 双方战斗中：非"霜儿"NPC/玩家一律拒谈
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    me->set_temp("fight/is_fighting", (long)1);
    EXPECT_EQ(CallTalk(me, npc->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。");
    me->delete_temp("fight/is_fighting");
}

TEST_F(CmdUserSocialTest, Talk_PlayerMenus_NoPartyGivesInviteMenu)
{
    // 玩家间对话：无组队关系 → 菜单含"邀请加入队伍"
    StateRecordingUser* speaker = MakeOnline("speaker1", "说话者");
    speaker->clear();

    char arg[256] = "";
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("邀请")) << "默认应给出邀请组队菜单项";
    EXPECT_TRUE(speaker->has("$0party $1"));
}

TEST_F(CmdUserSocialTest, Talk_PlayerMenus_PartyLeaderShowsFollowToggle)
{
    StateRecordingUser* speaker = MakeOnline("speaker2", "队员说话者");
    // other 是队长（speaker 的 party/master）→ 菜单变"解除组队"+ 跟随开关
    speaker->set_temp("party/master", "passer1");
    char arg[256] = "";

    // follow 关：给"组队跟随"项
    speaker->clear();
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0partyleave $1"));
    EXPECT_TRUE(speaker->has("组队跟随"));
    EXPECT_FALSE(speaker->has("取消组队跟随"));

    // follow 开：变"取消组队跟随"
    speaker->set_temp("party/follow", (long)1);
    speaker->clear();
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("取消组队跟随"));
}

TEST_F(CmdUserSocialTest, Talk_PlayerMenus_MemberShowsKickMenu)
{
    // 我持有队长 other 的 friend 键 → 踢除+查看队员菜单
    StateRecordingUser* speaker = MakeOnline("speaker3", "队员说话者");
    speaker->set_temp("party/friend_passer1", (long)1);
    speaker->clear();

    char arg[256] = "";
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0partykick $1"));
    EXPECT_TRUE(speaker->has("$0partylist $1"));
}

TEST_F(CmdUserSocialTest, Talk_ResidentBranchesInviteVsKickout)
{
    StateRecordingUser* speaker = MakeOnline("speaker4", "有房者");
    speaker->set("resident", "somewhere");
    speaker->set("gender", "男性");
    other->set("gender", "女性");
    char arg[256] = "";

    // 在自己家（owner==speaker）→ 赶出家门菜单
    room->set("owner", "speaker4");
    speaker->clear();
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0kickout 0 passer1"));

    // 在别人家 → 邀请做客菜单
    room->set("owner", "someone_else");
    speaker->clear();
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0invite $1"));
    room->del("owner");
}

TEST_F(CmdUserSocialTest, Talk_SleepRoomMenuForOppositeGender)
{
    StateRecordingUser* speaker = MakeOnline("speaker5", "睡眠者");
    speaker->set("gender", "男性");
    other->set("gender", "女性");
    room->set("sleep_room", (long)1);
    speaker->clear();

    char arg[256] = "";
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0sleep $1"));
    room->del("sleep_room");
}

TEST_F(CmdUserSocialTest, Talk_CoupleAndRecruitMenus)
{
    StateRecordingUser* speaker = MakeOnline("speaker6", "师父说话者");
    // 互为夫妻 → 技能/授艺菜单
    speaker->set("couple/id", "passer1");
    other->set("couple/id", "speaker6");
    // 双向收徒经验 → 招徒/拜师菜单
    speaker->set("recruit_exp", (long)10);
    other->set("recruit_exp", (long)10);
    speaker->clear();

    char arg[256] = "";
    ASSERT_EQ(CallTalk(speaker, other->object_id(), arg), 1);
    EXPECT_TRUE(speaker->has("$0skill $1"));
    EXPECT_TRUE(speaker->has("$0teach $1"));
    EXPECT_TRUE(speaker->has("$0recruit $1"));
    EXPECT_TRUE(speaker->has("$0apprentice $1"));
    EXPECT_EQ(speaker->query("recruit_exp"), 10);
}

// =====================================================================
// vote / determine_total / counting / nullify_vote
// =====================================================================

TEST_F(CmdUserSocialTest, Vote_BadFormatRejected)
{
    char arg[]           = "foo bar";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "用指令/vote <close|open> <频道>");
}

TEST_F(CmdUserSocialTest, Vote_MissingChannelRejected)
{
    char arg[]           = "close";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "用指令/vote <close|open> <频道>");
}

TEST_F(CmdUserSocialTest, Vote_TooYoungRejected)
{
    me->set("age", (long)10);
    char arg[]           = "close chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你年龄不满15岁，没有投票权。");
}

TEST_F(CmdUserSocialTest, Vote_LevelTooLowRejected)
{
    me->set("level", (long)1);
    char arg[]           = "close chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你等级不到5级，没有投票权。");
}

TEST_F(CmdUserSocialTest, Vote_UnknownChannelRejected)
{
    char arg[]           = "close bogus";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "不能关闭bogus这个频道。");
}

TEST_F(CmdUserSocialTest, Vote_UnknownTargetRejected)
{
    char arg[]           = "close chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "目前找不到这个人。");
}

TEST_F(CmdUserSocialTest, Vote_AlreadyClosedRejected)
{
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    target->set("chblk/chat", g_current_time + 86400);
    char arg[]           = "close chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, target->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "被投者的交谈频道已经被关闭了。");
}

TEST_F(CmdUserSocialTest, Vote_AlreadyOpenRejected)
{
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    // chblk 缺省 0，g_current_time 必须 > 0 才走"已打开"分支
    const int saved      = g_current_time;
    g_current_time       = 1000;
    char arg[]           = "open chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, target->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "被投者的交谈频道已经被打开了。");
    g_current_time = saved;
}

TEST_F(CmdUserSocialTest, Vote_DuplicateVoteRejected)
{
    // open 路径要求 g_current_time <= chblk(0)，固定时钟使判定确定
    g_current_time             = 0;
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    // 预置 myvariable 映射：open_chat_votetarget1 → 含 tester1 键
    CMapping voted;
    voted.set("tester1", (long)1);
    g_channel.voted_players.set("open_chat_votetarget1", voted);

    char arg[]           = "open chat";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVote(me, target->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你已经投过相同的票了。");
    g_channel.voted_players.del("open_chat_votetarget1");
}

TEST_F(CmdUserSocialTest, Vote_FirstVoteAnnouncesShortfall)
{
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    char arg[]                 = "close chat";
    // 首票：进入 votes_casted 登记分支，票数不足 → 播报"还差N票"
    ASSERT_EQ(CallVote(me, target->object_id(), arg), 1);
    // 票已记录到 voted_players
    EXPECT_FALSE(g_channel.voted_players.undefinedp("close_chat_votetarget1"));
    g_channel.voted_players.del("close_chat_votetarget1");
    g_channel.votes_casted.del("close_chat_votetarget1");
}

TEST_F(CmdUserSocialTest, Vote_NewChannelUsesNewbieLabel)
{
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    char arg[]                 = "close new";
    EXPECT_EQ(CallVote(me, target->object_id(), arg), 1);
    g_channel.voted_players.del("close_new_votetarget1");
    g_channel.votes_casted.del("close_new_votetarget1");
}

TEST_F(CmdUserSocialTest, Vote_PartyChannelUsesSectLabel)
{
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    char arg[]                 = "close party";
    EXPECT_EQ(CallVote(me, target->object_id(), arg), 1);
    g_channel.voted_players.del("close_party_votetarget1");
    g_channel.votes_casted.del("close_party_votetarget1");
}

TEST_F(CmdUserSocialTest, Vote_MilestoneReachedAppliesClose)
{
    // 计票达标：预填 2 张在线票，本次在线投票者再投 1 张 → votes >= total
    // → 执行关闭频道。投票者必须在线（counting 按 find_player 计票）。
    StateRecordingUser* voter = MakeOnline("voter_me", "在线投票者");
    voter->set("age", (long)20);
    voter->set_temp("client/ip_address", "4.4.4.4");
    StateRecordingUser* v1     = MakeOnline("voter1", "投票甲");
    StateRecordingUser* v2     = MakeOnline("voter2", "投票乙");
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    v1->set_temp("client/ip_address", "1.1.1.1");
    v2->set_temp("client/ip_address", "2.2.2.2");
    target->set_temp("client/ip_address", "3.3.3.3");

    CMapping existing;
    CMapping e1;
    e1.set("obj", (long)v1->object_id());
    e1.set("ip", "1.1.1.1");
    existing.set("voter1", e1);
    CMapping e2;
    e2.set("obj", (long)v2->object_id());
    e2.set("ip", "2.2.2.2");
    existing.set("voter2", e2);
    g_channel.voted_players.set("close_chat_votetarget1", existing);

    // total = 合格在线投票者/3 下限 3；预填 2 票 + 本票 = 3 → 达标
    char arg[] = "close chat";
    ASSERT_EQ(CallVote(voter, target->object_id(), arg), 1);
    // 关闭生效：目标频道被封锁 1 天
    EXPECT_EQ(target->query("chblk/chat"), g_current_time + 86400);
    // 清算后映射被删
    EXPECT_TRUE(g_channel.voted_players.undefinedp("close_chat_votetarget1"));
}

TEST_F(CmdUserSocialTest, Vote_MilestoneReachedAppliesOpen)
{
    StateRecordingUser* voter = MakeOnline("voter_me", "在线投票者");
    voter->set("age", (long)20);
    voter->set_temp("client/ip_address", "4.4.4.4");
    StateRecordingUser* v1     = MakeOnline("voter1", "投票甲");
    StateRecordingUser* v2     = MakeOnline("voter2", "投票乙");
    StateRecordingUser* target = MakeOnline("votetarget1", "被投者");
    v1->set_temp("client/ip_address", "1.1.1.1");
    v2->set_temp("client/ip_address", "2.2.2.2");
    target->set_temp("client/ip_address", "3.3.3.3");
    target->set("chblk/chat", g_current_time + 86400);

    CMapping existing;
    CMapping e1;
    e1.set("obj", (long)v1->object_id());
    e1.set("ip", "1.1.1.1");
    existing.set("voter1", e1);
    CMapping e2;
    e2.set("obj", (long)v2->object_id());
    e2.set("ip", "2.2.2.2");
    existing.set("voter2", e2);
    g_channel.voted_players.set("open_chat_votetarget1", existing);

    char arg[] = "open chat";
    ASSERT_EQ(CallVote(voter, target->object_id(), arg), 1);
    EXPECT_EQ(target->query("chblk/chat"), 0) << "开放投票达标应解封频道";
}

TEST_F(CmdUserSocialTest, DetermineTotal_MinimumThree)
{
    // 空在线表 → 下限 3
    EXPECT_EQ(CallDetermineTotal(), 3);
}

TEST_F(CmdUserSocialTest, DetermineTotal_CountsUniqueIpAndCaps)
{
    // 注册 3 个合格投票者（age>15 且 level>5），两个同 IP → 计 2 → /3 = 0 → 3
    StateRecordingUser* a = MakeOnline("vp1", "投票人一");
    StateRecordingUser* b = MakeOnline("vp2", "投票人二");
    StateRecordingUser* c = MakeOnline("vp3", "投票人三");
    a->set("age", (long)20);
    b->set("age", (long)20);
    c->set("age", (long)20);
    a->set_temp("client/ip_address", "9.9.9.1");
    b->set_temp("client/ip_address", "9.9.9.1");  // 同 IP 只计一票
    c->set_temp("client/ip_address", "9.9.9.2");
    EXPECT_EQ(CallDetermineTotal(), 3);
}

TEST_F(CmdUserSocialTest, Counting_TalliesOnlineUniqueIpVoters)
{
    StateRecordingUser* v = MakeOnline("voter_cnt", "计票者");
    v->set_temp("client/ip_address", "5.5.5.5");

    CMapping players;
    CMapping m;
    m.set("obj", (long)v->object_id());
    m.set("ip", "5.5.5.5");
    players.set("voter_cnt", m);

    // 1 张在线票、IP 唯一 → 计 1
    EXPECT_EQ(CallCounting(&players, "close_chat_x"), 1);
}

TEST_F(CmdUserSocialTest, Counting_PrunesLoggedOutVoters)
{
    CMapping players;
    CMapping m;
    m.set("obj", (long)999999);  // 已下线
    m.set("ip", "5.5.5.5");
    players.set("ghost", m);

    // 下线者被清出映射且不计票
    EXPECT_EQ(CallCounting(&players, "close_chat_x"), 0);
    EXPECT_TRUE(players.undefinedp("ghost"));
}

TEST_F(CmdUserSocialTest, NullifyVote_RemovesPendingVoteAndAnnounces)
{
    // 预置一个挂起的动议：voted_players 有键 → 取消并播报
    CMapping pending;
    pending.set("voter1", (long)1);
    g_channel.voted_players.set("myvar_x", pending);

    g_channel.set("cancel_vote601/myvariable", "myvar_x");
    g_channel.set("cancel_vote601/choicename", "关闭");
    g_channel.set("cancel_vote601/playername", "某人");

    CallNullifyVote(nullptr, 601);

    EXPECT_TRUE(g_channel.voted_players.undefinedp("myvar_x"));
    EXPECT_TRUE(g_channel.votes_casted.undefinedp("myvar_x"));
}

TEST_F(CmdUserSocialTest, NullifyVote_UnknownVoteIdLeavesPendingUntouched)
{
    // 负控制：动议 999 不存在时，真实挂起的动议 myvar_x 不受影响
    CMapping pending;
    pending.set("voter1", (long)1);
    g_channel.voted_players.set("myvar_x", pending);

    CallNullifyVote(nullptr, 999);

    EXPECT_FALSE(g_channel.voted_players.undefinedp("myvar_x"));
    g_channel.voted_players.del("myvar_x");
}

// =====================================================================
// nick_name
// =====================================================================

TEST_F(CmdUserSocialTest, NickName_TooShortRejected)
{
    char arg[]           = "a";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallNickName(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "对不起，你的中文名字必须是 1 到 6 个中文字。");
}

TEST_F(CmdUserSocialTest, NickName_TooLongRejected)
{
    char arg[]           = "abcdefghijklm";  // 13 字节 > Lua 上限 12
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallNickName(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "对不起，你的中文名字必须是 1 到 6 个中文字。");
}

TEST_F(CmdUserSocialTest, NickName_Success)
{
    char arg[] = "hello";
    ASSERT_EQ(CallNickName(me, 0, arg), 1);
    EXPECT_STREQ(me->querystr("nickname"), "hello");
}

// =====================================================================
// invite / accept
// =====================================================================

TEST_F(CmdUserSocialTest, Invite_NoInviteEnvRejected)
{
    room->set("no_invite", (long)1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallInvite(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里禁止邀请他人。");
    room->del("no_invite");
}

TEST_F(CmdUserSocialTest, Invite_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallInvite(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要邀请谁去你的家做客？");
}

TEST_F(CmdUserSocialTest, Invite_NotSettledRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallInvite(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你还没有定居，怎么邀请人家？");
}

TEST_F(CmdUserSocialTest, Invite_SelfRejected)
{
    me->set("resident", "my_house");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallInvite(me, me->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你对自己说：算了还是自己邀请自己吧。");
}

TEST_F(CmdUserSocialTest, Invite_AlreadyAcceptedRejected)
{
    me->set("resident", "my_house");
    other->set_temp("invite", "tester1");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallInvite(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方已经接受了你的邀请。");
    other->del("invite");
}

TEST_F(CmdUserSocialTest, Invite_SuccessRecordsPending)
{
    me->set("resident", "my_house");
    char arg[256] = "";
    ASSERT_EQ(CallInvite(me, other->object_id(), arg), 1);
    EXPECT_STREQ(me->querystr_temp("pending/invite"), "passer1");
}

TEST_F(CmdUserSocialTest, Accept_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAccept(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要接受谁的邀请？");
}

TEST_F(CmdUserSocialTest, Accept_SelfRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAccept(me, me->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你对自己说：算了还是自己邀请自己吧。");
}

TEST_F(CmdUserSocialTest, Accept_AlreadyAcceptedRejected)
{
    me->set_temp("invite", "passer1");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAccept(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你已经接受了对方的邀请。");
    me->del("invite");
}

TEST_F(CmdUserSocialTest, Accept_NotInvitedRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAccept(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方并没有邀请你。");
}

TEST_F(CmdUserSocialTest, Accept_SuccessRecordsInvite)
{
    other->set_temp("pending/invite", "tester1");
    char arg[256] = "";
    ASSERT_EQ(CallAccept(me, other->object_id(), arg), 1);
    EXPECT_STREQ(me->querystr_temp("invite"), "passer1");
    EXPECT_EQ(other->querystr_temp("pending/invite")[0], '\0')
        << "邀请方的 pending 应被清除";
}

// =====================================================================
// recruit_exp
// =====================================================================

TEST_F(CmdUserSocialTest, RecruitExp_NoApprenticesRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRecruitExp(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你还没有收徒经验！");
}

TEST_F(CmdUserSocialTest, RecruitExp_DbFailRejected)
{
    db->SetShouldFail(true);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRecruitExp(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你还没有收徒经验！");
}

TEST_F(CmdUserSocialTest, RecruitExp_SumsAndClampsToFloor)
{
    // SQL: select ... from recruit_exp where re_master=tester1（无引号）
    DbResultSet rs;
    DbRow r;
    r.columns = {"app1", "徒弟甲", "3000", "1"};
    rs.rows.push_back(r);
    db->SetQueryResultMatching("re_master=tester1", rs);

    char arg[256] = "";
    ASSERT_EQ(CallRecruitExp(me, 0, arg), 1);
    // 3000/100 = 30 < 50 → 下限 50
    EXPECT_EQ(me->query("recruit_exp"), 50);
}

TEST_F(CmdUserSocialTest, RecruitExp_SumsAboveFloor)
{
    DbResultSet rs;
    DbRow r1;
    r1.columns = {"app1", "徒弟甲", "6000", "1"};
    DbRow r2;
    r2.columns = {"app2", "徒弟乙", "2000", "0"};
    rs.rows.push_back(r1);
    rs.rows.push_back(r2);
    db->SetQueryResultMatching("re_master=tester1", rs);

    char arg[256] = "";
    ASSERT_EQ(CallRecruitExp(me, 0, arg), 1);
    // 6000/100 + 2000/100 = 80 > 50 → 80
    EXPECT_EQ(me->query("recruit_exp"), 80);
}

TEST_F(CmdUserSocialTest, RecruitExp_WizardQueriesTargetMaster)
{
    me->set("grant", (long)CChar::GRANT_WIZARD);
    StateRecordingUser* target = MakeOnline("recruit_tgt", "徒弟丙");
    DbResultSet rs;
    DbRow r;
    r.columns = {"app3", "徒弟丙", "9000", "1"};
    rs.rows.push_back(r);
    // 巫师 + arg 指定师傅 id
    db->SetQueryResultMatching("re_master=recruit_tgt", rs);

    char arg[] = "recruit_tgt";
    ASSERT_EQ(CallRecruitExp(me, target->object_id(), arg), 1);
    EXPECT_EQ(me->query("recruit_exp"), 90);
    me->del("grant");
}

// =====================================================================
// mv / loc / who
// =====================================================================

TEST_F(CmdUserSocialTest, Mv_SetsMapTempsAndSkipsSelf)
{
    // 房内另一玩家收到定位广播（StateRecordingUser 记录）
    StateRecordingUser* peer = MakeOnline("mv_peer", "同行者");
    peer->clear();

    char arg[] = "10 20 30 40 1";
    ASSERT_EQ(CallMv(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("map/bx"), 10);
    EXPECT_EQ(me->query_temp("map/by"), 20);
    EXPECT_EQ(me->query_temp("map/ex"), 30);
    EXPECT_EQ(me->query_temp("map/ey"), 40);
    EXPECT_EQ(me->query_temp("map/ac"), 1);
}

TEST_F(CmdUserSocialTest, Loc_NonUserReturnsZero)
{
    char arg[] = "1 2";
    EXPECT_EQ(CallLoc(me, 0, arg), 0);
}

TEST_F(CmdUserSocialTest, Loc_UserSetsLivePointAndClearsLoading)
{
    StateRecordingUser* u = MakeOnline("loc_user", "定位者");
    u->set_temp("client/loading", (long)1);
    u->clear();

    char arg[] = "7 8";
    ASSERT_EQ(CallLoc(u, 0, arg), 1);
    EXPECT_EQ(u->query_temp("map/bx"), 7);
    EXPECT_EQ(u->query_temp("map/by"), 8);
    EXPECT_EQ(u->query_temp("client/loading"), 0);
}

TEST_F(CmdUserSocialTest, Who_SendsUserListPage)
{
    StateRecordingUser* u = MakeOnline("who_user", "查询者");
    u->clear();
    char arg[] = "0 0";
    ASSERT_EQ(CallWho(u, 0, arg), 1);
    // send_user_online 给自己发 USERLIST 包
    EXPECT_TRUE(u->sent > 0) << "who 应向调用者回发在线列表";
}
