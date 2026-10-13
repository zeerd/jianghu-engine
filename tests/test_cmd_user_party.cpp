// test_cmd_user_party.cpp - Unit tests for CmdUserParty.cpp
//
// Tests party system game state.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "CmdUser.h"
#include "PartyConfig.h"
#include "test_state_recorder.h"
#include "test_lua_env.h"

extern MAP2USER g_user_list;
extern char g_notify_fail_msg[512];

class CmdUserPartyTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallParty(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"party", me, target, arg);
    }
    static int CallAcceptParty(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"accept_party", me, target, arg);
    }
    static int CallPartyleave(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"partyleave", me, target, arg);
    }
    static int CallPartykick(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"partykick", me, target, arg);
    }
    static int CallPartyfollow(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"partyfollow", me, target, arg);
    }
    static int CallPartylist(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"partylist", me, target, arg);
    }
    static int CallPair(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"pair", me, target, arg);
    }
    static int CallAcceptPair(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"accept_pair", me, target, arg);
    }
    static int CallPairLeave(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"pairleave", me, target, arg);
    }
    static int CallPairKick(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"pairkick", me, target, arg);
    }
    static int CallPairFollow(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"pairfollow", me, target, arg);
    }
    static int CallPairList(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"pairlist", me, target, arg);
    }

    CChar* me;
    CChar* other;
    CContainer* room;
    std::vector<StateRecordingUser*> online_users;

    // 注册在线 CUser（find_player/find_online 走 g_user_list）；
    // 进房后摘 client/loading，否则 get_room_char 找不到人
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
        me->move(room);

        other = new CChar();
        other->set("name", "队友");
        other->set("id", "teammate1");
        other->set("level", (long)50);
        other->set("hp", (long)100);
        other->move(room);
    }

    void TearDown() override
    {
        for (auto* u : online_users) {
            g_user_list.erase(u->object_id());
            u->move(nullptr);
            delete u;
        }
        online_users.clear();
        other->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete other;
        delete me;
        delete room;
    }
};

TEST_F(CmdUserPartyTest, Party_NoTarget)
{
    int result = CallParty(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdUserPartyTest, Party_Self)
{
    int result = CallParty(me, me->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdUserPartyTest, Party_ValidTarget)
{
    int result = CallParty(me, other->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserPartyTest, AcceptParty_NoTarget)
{
    int result = CallAcceptParty(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdUserPartyTest, Partyleave_NotInParty)
{
    int result = CallPartyleave(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

// ---------------- party：拒绝分支 ----------------

TEST_F(CmdUserPartyTest, Party_NoInviteEnvRejected)
{
    room->set("no_invite", (long)1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里禁止邀请他人。");

    // 巫师豁免
    me->set("grant", (long)CChar::GRANT_WIZARD);
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 1);
    me->del("grant");
    room->del("no_invite");
}

TEST_F(CmdUserPartyTest, Party_NoAllEnvRejected)
{
    room->set("no_all", (long)1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里禁止邀请他人。");
    room->del("no_all");
}

TEST_F(CmdUserPartyTest, Party_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要邀请谁加入你的队伍？");
}

TEST_F(CmdUserPartyTest, Party_AlreadyLeadingRejected)
{
    // 自己已是队长（party/master 非空）→ 必须先离队
    me->set_temp("party/master", "someone_else");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你现在正在与别人队伍中，要想自己组建队伍需要先行离队。");
}

TEST_F(CmdUserPartyTest, Party_PartyFullRejected)
{
    me->set_temp("party/number", CPartyConfig::max_members());
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "已经邀请了这么多的队友，足够闯荡江湖了吧？");
}

TEST_F(CmdUserPartyTest, Party_TargetAlreadyAcceptedRejected)
{
    other->set_temp("party/master", "tester1");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方已经接受了你的邀请。");
}

TEST_F(CmdUserPartyTest, Party_SuccessRecordsInvite)
{
    char arg[256] = "";
    ASSERT_EQ(CallParty(me, other->object_id(), arg), 1);
    EXPECT_STREQ(me->querystr_temp("party/invite"), "teammate1");
}

// ---------------- accept_party ----------------

TEST_F(CmdUserPartyTest, AcceptParty_SelfRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAcceptParty(me, me->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你对自己说：算了还是自己邀请自己吧。");
}

TEST_F(CmdUserPartyTest, AcceptParty_AlreadyInPartyRejected)
{
    me->set_temp("party/master", "someone_else");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAcceptParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你现在已经和别人组队过了。");
}

TEST_F(CmdUserPartyTest, AcceptParty_NotInvitedRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAcceptParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方并没有邀请你。");
}

TEST_F(CmdUserPartyTest, AcceptParty_MyInvitePendingRejected)
{
    // 对方邀请了我，但我已向别人发出邀请 → 不允许接受
    other->set_temp("party/invite", "tester1");
    me->set_temp("party/invite", "someone_else");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAcceptParty(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你已经对其他人发出了组队邀请，暂时不能接受别人的组队邀请。");
}

TEST_F(CmdUserPartyTest, AcceptParty_SuccessJoinsAndAcknowledges)
{
    other->set_temp("party/invite", "tester1");
    char arg[256] = "";
    ASSERT_EQ(CallAcceptParty(me, other->object_id(), arg), 1);
    // 我记下队长；队长记下我、人数 +1、邀请被清
    EXPECT_STREQ(me->querystr_temp("party/master"), "teammate1");
    EXPECT_EQ(other->query_temp("party/friend_tester1"), 1);
    EXPECT_EQ(other->query_temp("party/number"), 1);
    EXPECT_EQ(other->querystr_temp("party/invite")[0], '\0');
}

// ---------------- partyleave ----------------

TEST_F(CmdUserPartyTest, Partyleave_MasterOfflineCleansUp)
{
    me->set_temp("party/master", "ghost_player");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyleave(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "队长已不在线，你可以离队。");
    // 离队状态已清理
    EXPECT_EQ(me->querystr_temp("party/master")[0], '\0');
    EXPECT_EQ(me->querystr_temp("party/follow")[0], '\0');
}

TEST_F(CmdUserPartyTest, Partyleave_UnknownTargetRejected)
{
    StateRecordingUser* leader = MakeOnline("leader1", "队长");
    me->set_temp("party/master", "leader1");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyleave(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要解除与谁的组队关系？");
}

TEST_F(CmdUserPartyTest, Partyleave_SelfRejected)
{
    StateRecordingUser* me_u   = MakeOnline("self1", "自己");
    StateRecordingUser* leader = MakeOnline("leader2", "队长2");
    me_u->set_temp("party/master", "leader2");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyleave(me_u, me_u->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这样做好象不合适吧？");
}

TEST_F(CmdUserPartyTest, Partyleave_NotSameMasterRejected)
{
    StateRecordingUser* leader  = MakeOnline("leader3", "队长3");
    StateRecordingUser* other_u = MakeOnline("bystander1", "路人");
    me->set_temp("party/master", "leader3");
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyleave(me, other_u->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你并没有和对方组队。");
}

TEST_F(CmdUserPartyTest, Partyleave_LeaderLostMyFriendKeyCleansUp)
{
    StateRecordingUser* leader = MakeOnline("leader4", "队长4");
    me->set_temp("party/master", "leader4");
    // 队长侧没有我的 friend 键 → 视为已被踢出，清理本地状态
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyleave(me, leader->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "对方并没有与你组队，或已经将你赶出了队伍。");
    EXPECT_EQ(me->querystr_temp("party/master")[0], '\0');
}

TEST_F(CmdUserPartyTest, Partyleave_SuccessClearsBothSides)
{
    StateRecordingUser* leader = MakeOnline("leader5", "队长5");
    leader->set_temp("party/friend_tester1", 1);
    leader->set_temp("party/number", 1);
    me->set_temp("party/master", "leader5");
    me->set_temp("party/follow", 1);

    char arg[256] = "";
    ASSERT_EQ(CallPartyleave(me, leader->object_id(), arg), 1);
    EXPECT_EQ(me->querystr_temp("party/master")[0], '\0');
    EXPECT_EQ(me->querystr_temp("party/follow")[0], '\0');
    EXPECT_EQ(leader->query_temp("party/friend_tester1"), 0);
    EXPECT_EQ(leader->query_temp("party/number"), 0);
}

// ---------------- partykick ----------------

TEST_F(CmdUserPartyTest, PartyKick_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartykick(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要解除与谁的组队关系？");
}

TEST_F(CmdUserPartyTest, PartyKick_SelfRejected)
{
    StateRecordingUser* me_u = MakeOnline("kickself", "自己");
    char arg[256]            = "";
    g_notify_fail_msg[0]     = '\0';
    EXPECT_EQ(CallPartykick(me_u, me_u->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这样做好象不合适吧？");
}

TEST_F(CmdUserPartyTest, PartyKick_TargetNotMyMemberDecrementsCounter)
{
    StateRecordingUser* stranger = MakeOnline("stranger1", "路人甲");
    me->set_temp("party/number", 1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartykick(me, stranger->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "对方并没有与你组队，或已经自己离开了队伍。");
    // 误踢分支也会把我方人数 -1（源码行为）
    EXPECT_EQ(me->query_temp("party/number"), 0);
}

TEST_F(CmdUserPartyTest, PartyKick_IForgotFriendKeyRejected)
{
    StateRecordingUser* member = MakeOnline("member1", "队员1");
    member->set_temp("party/master", "tester1");
    // 我这边没有 member1 的 friend 键
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartykick(me, member->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你并没有和对方组队。");
}

TEST_F(CmdUserPartyTest, PartyKick_SuccessClearsBothSides)
{
    StateRecordingUser* member = MakeOnline("member2", "队员2");
    member->set_temp("party/master", "tester1");
    me->set_temp("party/friend_member2", 1);
    me->set_temp("party/number", 1);

    char arg[256] = "";
    ASSERT_EQ(CallPartykick(me, member->object_id(), arg), 1);
    EXPECT_EQ(member->querystr_temp("party/master")[0], '\0');
    EXPECT_EQ(me->query_temp("party/friend_member2"), 0);
    EXPECT_EQ(me->query_temp("party/number"), 0);
}

// ---------------- partyfollow ----------------

TEST_F(CmdUserPartyTest, PartyFollow_NoFollowEnvRejected)
{
    room->set("no_follow", (long)1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyfollow(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里禁止跟随他人。");
    room->del("no_follow");
}

TEST_F(CmdUserPartyTest, PartyFollow_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyfollow(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要跟随谁？");
}

TEST_F(CmdUserPartyTest, PartyFollow_NotTeamedRejected)
{
    me->set_temp("party/master", "leader_x");
    me->set_temp("party/follow", 1);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyfollow(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你并没有和对方组队？");
    // follow 标记被清
    EXPECT_EQ(me->querystr_temp("party/follow")[0], '\0');
}

TEST_F(CmdUserPartyTest, PartyFollow_LeaderLostMyFriendKeyCleansUp)
{
    me->set_temp("party/master", "teammate1");
    // other 侧没有我的 friend 键
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartyfollow(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "对方并没有与你组队，或已经将你赶出了队伍。");
    EXPECT_EQ(me->querystr_temp("party/master")[0], '\0');
}

TEST_F(CmdUserPartyTest, PartyFollow_ToggleOnThenOff)
{
    me->set_temp("party/master", "teammate1");
    other->set_temp("party/friend_tester1", 1);
    char arg[256] = "";

    // 首次：开启跟随
    ASSERT_EQ(CallPartyfollow(me, other->object_id(), arg), 1);
    EXPECT_EQ(me->query_temp("party/follow"), 1);

    // 再次：取消跟随
    ASSERT_EQ(CallPartyfollow(me, other->object_id(), arg), 1);
    EXPECT_EQ(me->query_temp("party/follow"), 0);
}

// ---------------- partylist ----------------

TEST_F(CmdUserPartyTest, PartyList_NotInPartyCleansUp)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPartylist(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你并没有处于队伍中。");
}

TEST_F(CmdUserPartyTest, PartyList_LeaderSidePrunesOfflineMembers)
{
    // 我是队长：teammate1（fixture，不在 g_user_list）视为离线 → 剔除
    me->set_temp("party/number", 1);
    me->set_temp("party/friend_teammate1", 1);
    char arg[256] = "";
    ASSERT_EQ(CallPartylist(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("party/friend_teammate1"), 0);
    EXPECT_EQ(me->query_temp("party/number"), 1) << "number=键数2-离线1";
}

TEST_F(CmdUserPartyTest, PartyList_MembershipSideListsLeaderAndMembers)
{
    // 我是队员：队长在线且持有 friend 键 → 列出队长与在线成员
    StateRecordingUser* leader = MakeOnline("listlead", "列表队长");
    StateRecordingUser* member = MakeOnline("listmate1", "在线成员");
    me->set_temp("party/master", "listlead");
    leader->set_temp("party/friend_listmate1", 1);

    char arg[256] = "";
    ASSERT_EQ(CallPartylist(me, 0, arg), 1);
    // 在线成员的 friend 键保留
    EXPECT_EQ(leader->query_temp("party/friend_listmate1"), 1);
}

// ---------------- 夫妻组队：pair 族 ----------------

class CmdUserPairTest : public CmdUserPartyTest {};

TEST_F(CmdUserPairTest, Pair_CoupleIdMismatchRejected)
{
    StateRecordingUser* spouse = MakeOnline("spouse1", "伴侣");
    char arg[256]              = "";
    g_notify_fail_msg[0]       = '\0';
    EXPECT_EQ(CallPair(me, spouse->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这不太合适吧？");
}

TEST_F(CmdUserPairTest, Pair_LevelDiffTooLargeRejected)
{
    StateRecordingUser* spouse = MakeOnline("spouse2", "伴侣2");
    spouse->set("couple/id", "tester1");
    me->set("couple/id", "spouse2");
    me->set("level", (long)50);
    spouse->set("level", (long)1);  // 差 49 > 默认 10
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPair(me, spouse->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这不太合适吧？");
}

TEST_F(CmdUserPairTest, Pair_SuccessDelegatesToPartyAndMarksCouple)
{
    StateRecordingUser* spouse = MakeOnline("spouse3", "伴侣3");
    spouse->set("couple/id", "tester1");
    me->set("couple/id", "spouse3");
    me->set("level", (long)50);
    spouse->set("level", (long)50);

    char arg[256] = "";
    ASSERT_EQ(CallPair(me, spouse->object_id(), arg), 1);
    EXPECT_EQ(me->query_temp("party/couple"), 1);
    EXPECT_STREQ(me->querystr_temp("party/invite"), "spouse3");
}

TEST_F(CmdUserPairTest, AcceptPair_CoupleIdMismatchRejected)
{
    StateRecordingUser* spouse = MakeOnline("spouse4", "伴侣4");
    char arg[256]              = "";
    g_notify_fail_msg[0]       = '\0';
    EXPECT_EQ(CallAcceptPair(me, spouse->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这不太合适吧？");
}

TEST_F(CmdUserPairTest, AcceptPair_SuccessDelegatesToAcceptParty)
{
    StateRecordingUser* spouse = MakeOnline("spouse5", "伴侣5");
    spouse->set("couple/id", "tester1");
    me->set("couple/id", "spouse5");
    spouse->set_temp("party/invite", "tester1");

    char arg[256] = "";
    ASSERT_EQ(CallAcceptPair(me, spouse->object_id(), arg), 1);
    EXPECT_STREQ(me->querystr_temp("party/master"), "spouse5");
    EXPECT_EQ(me->query_temp("party/couple"), 1);
}

TEST_F(CmdUserPairTest, PairLeave_DelegatesToPartyleave)
{
    // 未入队 → partyleave 的"并没有加入"分支
    char arg[256] = "";
    EXPECT_EQ(CallPairLeave(me, 0, arg), 0);
}

TEST_F(CmdUserPairTest, PairKick_DelegatesToPartykick)
{
    // 目标不在线 → partykick 的"解除与谁"分支
    char arg[256] = "";
    EXPECT_EQ(CallPairKick(me, 999999, arg), 0);
}

TEST_F(CmdUserPairTest, PairFollow_DelegatesToPartyfollow)
{
    char arg[256] = "";
    EXPECT_EQ(CallPairFollow(me, 0, arg), 0);
}

TEST_F(CmdUserPairTest, PairList_DelegatesToPartylist)
{
    char arg[256] = "";
    EXPECT_EQ(CallPairList(me, 0, arg), 0);
}
