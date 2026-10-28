// test_cmd_party_dispatch.cpp — 组队命令 party 与门派频道撞名的分发修复
//
// 「party」一词身兼两职：组队指令 CCmdUser::party（写 party/invite、发邀请菜单）
// 与门派频道动词（ChatChannel channels["party"]）。CCmdHandle::command 里
// do_channel(273) 先于 handlecommand(275) 执行，而组队邀请靠 /t 目标、无消息体
// —— do_channel 对空消息在 ChatChannel 第 5 步静默 return 1，会吞掉整条组队指令，
// CCmdUser::party 永不执行（原库组队名 pary、频道名 party 本不冲突，jianghu 把
// 组队改名为 party 才撞车）。
//
// 修复：Comm=="party" 且 arg 为空（=组队邀请）时跳过 do_channel，交给
// handlecommand；带消息体的 party 仍是门派发言走频道。
//
// 本测走完整 CCmdHandle::command（含 do_channel，经 CChar::command 入口），
// 断言空消息的 party 真被执行到 CCmdUser::party（party/invite 被写入被邀请者），
// 即不再被频道吞掉——修复的回归锚点。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "CmdHandle.h"
#include "Global.h"
#include "Room.h"
#include "User.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {

class PartyDispatchTest : public ::testing::Test {
protected:
    CRoom* room           = nullptr;
    StateRecordingUser* me = nullptr;
    StateRecordingUser* peer = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        g_notify_fail_msg[0] = '\0';

        room = new CRoom();
        room->set_name("测试场地", "party_disp_room");

        me = new StateRecordingUser();
        me->set_name("队长", "pdisp_me");
        me->set("id", "pdisp_me");
        // 越过 handlecommand 的 g_cmd_user grant 门槛（party 是用户命令）
        me->set("grant", (long)CChar::GRANT_WIZARD);
        me->set("hp", (long)100);
        me->set("eff_hp", (long)100);
        me->set("max_hp", (long)100);
        me->move(room);
        // move() 会置 client/loading=1，使 CCmdHandle::command 第 180 行
        // is_loading() 早退（命令到不了 party）——须在发命令前清掉装入标记
        me->delete_temp("client/loading");

        peer = new StateRecordingUser();
        peer->set_name("队员", "pdisp_peer");
        peer->set("id", "pdisp_peer");
        peer->set("hp", (long)100);
        peer->set("eff_hp", (long)100);
        peer->set("max_hp", (long)100);
        peer->move(room);
        peer->delete_temp("client/loading");
    }

    void TearDown() override
    {
        // StateRecordingUser 入房会登记进 g_user_list，收尾摘除避免跨用例污染
        g_user_list.erase(peer->object_id());
        g_user_list.erase(me->object_id());
        peer->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete peer;
        delete me;
        delete room;
    }

    // 完整命令分发链（CCmdHandle::command：角色目标拦截 → do_channel → handlecommand）
    int Send(const char* line)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s", line);
        return me->command(buf);
    }
};

}  // namespace

// 空消息体的 party = 组队邀请：必须越过门派频道 do_channel、落到 CCmdUser::party，
// 写入 party/invite（证明组队指令真的执行，没被频道吞掉——修复回归锚点）。
TEST_F(PartyDispatchTest, EmptyArgParty_RunsTeamCommand_NotSwallowedByChannel)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "party %ld", peer->object_id());

    int ret = me->command(buf);

    EXPECT_EQ(1, ret) << "组队邀请应成功返回 1";
    EXPECT_STREQ(peer->querystr("id"), me->querystr_temp("party/invite"))
        << "party/invite 应记录被邀请者 id（CCmdUser::party 确已执行）";
    EXPECT_EQ(std::string::npos,
              std::string(g_notify_fail_msg).find("门派"))
        << "不应落进门派频道的「未入门派」失败分支";
}

// 空消息体、目标无效（不在房间）：仍落到 CCmdUser::party，由其自身 notify_fail
//（你要邀请谁）——证明空消息总是进组队命令，而非被频道吞掉。
TEST_F(PartyDispatchTest, EmptyArgParty_MissTarget_FailsInsideTeamCommand)
{
    int ret = me->command((char*)"party 999999");

    EXPECT_EQ(0, ret) << "目标不在房间 → 组队命令 notify_fail 返回 0";
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你要邀请谁"))
        << "失败文案应来自 CCmdUser::party 自身，而非频道分支";
    EXPECT_TRUE(std::string(me->querystr_temp("party/invite")).empty())
        << "未成功邀请时不应写入 party/invite";
}
