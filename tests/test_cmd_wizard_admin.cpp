// test_cmd_wizard_admin.cpp - Unit tests for CmdWizardAdmin.cpp
//
// Tests game state underlying Promote, Whois, Kick, ban, etc.
//
#include "pch.h"

#include <mutex>
#include <string>
#include <vector>

#include "Channel.h"
#include "Char.h"
#include "CmdWizard.h"
#include "HeartBeat.h"  // g_destruct_list：kickout 自毁后回收
#include "Login.h"      // find_online
#include "LoginAuth.h"  // get_ip
#include "Room.h"
#include "User.h"
#include "mock/mock_database.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

class CmdWizardAdminTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallPromote(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"promote", me, target, arg);
    }
    static int CallWhois(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"whois", me, target, arg);
    }
    static int CallKick(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"kickout", me, target, arg);
    }
    static int CallBan(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"ban", me, target, arg);
    }
    static int CallUnban(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"unban", me, target, arg);
    }
    static int CallFreeze(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"freeze", me, target, arg);
    }
    static int CallUnfreeze(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"unfreeze", me, target, arg);
    }
    static int CallChpass(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"chpass", me, target, arg);
    }
    static int CallChverifypass(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"chverifypass", me, target, arg);
    }
    static int CallWholist(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"wholist", me, target, arg);
    }
    static int CallRecover(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"recover", me, target, arg);
    }
    static int CallSnoop(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"snoop", me, target, arg);
    }
    static int CallChblk(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"chblk", me, target, arg);
    }

    CChar* me;
    CContainer* room;
    MockDatabase* db = nullptr;
    // SetUp 时快照：TearDown 只回收本用例新增的在线用户与自毁对象
    std::vector<int32_t> m_live_users;
    std::vector<CContainer*> m_live_at_setup;

    // 登记一名录制在线用户（find_player/find_online 走 g_user_list）
    StateRecordingUser* AddUser(const char* id, const char* name, long grant,
                                const char* ip = "9.9.9.9")
    {
        auto* u = new StateRecordingUser();
        u->set_name(name, id);
        u->set("grant", grant);
        u->set("hp", (long)100);
        u->set_temp("client/ip_address", ip);
        u->set_temp("client/ip_local", "8.8.8.8");
        g_user_list[u->object_id()] = u;
        return u;
    }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        for (auto& kv : g_user_list) m_live_users.push_back(kv.first);

        db   = new MockDatabase();
        g_db = db;

        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试巫师");
        me->set("id", "wiz1");
        me->set("level", (long)100);
        me->set("grant", (long)200);
        me->set("hp", (long)1000);
        me->move(room);
    }

    void TearDown() override
    {
        // 1) 本用例新增的在线用户：停心跳、脱房、摘延迟列表后删除
        std::vector<CUser*> new_users;
        for (auto& kv : g_user_list) {
            bool known = false;
            for (auto id : m_live_users) {
                if (id == kv.first) known = true;
            }
            if (!known) new_users.push_back(kv.second);
        }
        for (auto* u : new_users) {
            g_user_list.erase(u->object_id());
            u->set_heart_beat(0);
            u->move(nullptr);
            {
                std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
                g_destruct_list.remove(u);
            }
            delete u;
        }
        // 2) 延迟析构列表：只回收本用例新增的对象（kickout 自毁的用户）
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete ob;
            }
        }
        // 3) 频道监听/黑名单复位（按玩家拉黑会写入拼装地址键）
        g_channel.snoop(me);
        g_channel.banlist.del("M:9.9.9.9 L:8.8.8.8");
        // 4) 摘 db，回收基础角色与房间
        g_db = nullptr;
        delete db;
        db = nullptr;
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }
};

TEST_F(CmdWizardAdminTest, Promote_NoTarget)
{
    int result = CallPromote(me, 0, (char*)"50");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardAdminTest, Whois_NoTarget)
{
    int result = CallWhois(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardAdminTest, Kick_NoTarget)
{
    int result = CallKick(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardAdminTest, Ban_ListAll)
{
    int result = CallBan(me, 0, (char*)"all");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdWizardAdminTest, Ban_AddIp)
{
    int result = CallBan(me, 0, (char*)"1.2.3.4");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdWizardAdminTest, Unban_Ip)
{
    int result = CallUnban(me, 0, (char*)"1.2.3.4");
    EXPECT_NE(result, -1);
}

TEST_F(CmdWizardAdminTest, Freeze_NoDb)
{
    int result = CallFreeze(me, 0, (char*)"testuser");
    EXPECT_NE(result, -1);
}

TEST_F(CmdWizardAdminTest, Unfreeze_NoDb)
{
    int result = CallUnfreeze(me, 0, (char*)"testuser");
    EXPECT_NE(result, -1);
}

// =====================================================================
// promote：权限升降（低于 PLAYER 拒绝、不可升到自己同级、成功授权）
// =====================================================================
TEST_F(CmdWizardAdminTest, Promote_GrantBelowPlayer_Rejected)
{
    auto* who  = AddUser("p_target", "平民", 30);
    char arg[] = "20";
    EXPECT_EQ(0, CallPromote(me, who->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能改变对方的权限"));
    EXPECT_EQ(30, who->query("grant")) << "拒绝后权限不变";
}

TEST_F(CmdWizardAdminTest, Promote_GrantAtOrAboveOwn_Rejected)
{
    auto* who  = AddUser("p_target2", "平民", 30);
    char arg[] = "200";  // 与自身同级
    EXPECT_EQ(0, CallPromote(me, who->object_id(), arg));
    EXPECT_EQ(30, who->query("grant"));
}

TEST_F(CmdWizardAdminTest, Promote_TargetNotLesser_Rejected)
{
    auto* who  = AddUser("p_target3", "同僚", 200);
    char arg[] = "50";
    EXPECT_EQ(0, CallPromote(me, who->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("无权改变对方"));
    EXPECT_EQ(200, who->query("grant"));
}

TEST_F(CmdWizardAdminTest, Promote_SuccessGrantsAndNotifies)
{
    auto* who  = AddUser("p_ok", "平民", 30);
    char arg[] = "50";
    EXPECT_EQ(1, CallPromote(me, who->object_id(), arg));
    EXPECT_EQ(50, who->query("grant")) << "授权落进 dbase";
    EXPECT_TRUE(who->has("提升")) << "本人应收到升级通知";
}

// =====================================================================
// whois：位置/在线时长/客户端版本回报
// =====================================================================
TEST_F(CmdWizardAdminTest, Whois_ReportsRoomAndClient)
{
    auto* wiz = AddUser("whois_wiz", "巫师", 200);
    auto* who = AddUser("whois_vic", "平民", 30);
    who->move(room);
    who->set("login/time", 100L);
    who->set_temp("client/version", "v1.0");

    EXPECT_EQ(1, CallWhois(wiz, who->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("位于")) << "应回报所在场景";
    EXPECT_TRUE(wiz->has("v1.0")) << "应回报客户端版本";
}

TEST_F(CmdWizardAdminTest, Whois_NoEnvironment_ReportsBare)
{
    auto* wiz = AddUser("whois_wiz2", "巫师", 200);
    auto* who = AddUser("whois_vic2", "平民", 30);  // 不入房

    EXPECT_EQ(1, CallWhois(wiz, who->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("无环境"));
}

// =====================================================================
// kickout：踢人排期 + do_kick 落地（保存、下线、自毁）
// =====================================================================
TEST_F(CmdWizardAdminTest, Kickout_UnknownId_Rejected)
{
    EXPECT_EQ(0, CallKick(me, 0, (char*)"nobody"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("kickout"));
}

TEST_F(CmdWizardAdminTest, Kickout_EqualGrant_Rejected)
{
    auto* who  = AddUser("kick_eq", "同僚", 200);
    char arg[] = "kick_eq";
    EXPECT_EQ(0, CallKick(me, 0, arg));
    EXPECT_NE(nullptr, find_online("kick_eq")) << "同级不可被踢";
}

TEST_F(CmdWizardAdminTest, Kickout_SchedulesAndDoKickDestroys)
{
    auto* wiz = AddUser("kick_wiz", "巫师", 200);
    wiz->move(room);
    wiz->delete_temp("client/loading");  // 代答切图，心跳才不会被挂起
    auto* who = AddUser("kick_vic", "平民", 30);
    who->move(room);

    char arg[] = "kick_vic";
    EXPECT_EQ(1, CallKick(wiz, 0, arg));
    EXPECT_NE(nullptr, find_online("kick_vic")) << "踢出是延迟落地的";

    // 一次心跳把 delay=1 的踢出回调推演落地：存档、下线、自毁
    wiz->heart_beat();
    EXPECT_EQ(nullptr, find_online("kick_vic")) << "被踢者已从在线表摘除";

    // 自毁对象进延迟列表：本用例收尾统一回收（见 TearDown 第 2 步）
}

// =====================================================================
// ban / unban：按人/按地址拉黑与解除
// =====================================================================
TEST_F(CmdWizardAdminTest, Ban_ByPlayerTarget_AddsIp)
{
    auto* who = AddUser("ban_vic", "平民", 30);
    who->move(room);

    EXPECT_EQ(1, CallBan(me, who->object_id(), (char*)""));
    EXPECT_EQ(1, g_channel.banlist.query("M:9.9.9.9 L:8.8.8.8"))
        << "拉黑写入该玩家的拼装地址";
}

TEST_F(CmdWizardAdminTest, Ban_EqualGrant_Rejected)
{
    auto* who  = AddUser("ban_eq", "同僚", 200);
    char arg[] = "";
    EXPECT_EQ(0, CallBan(me, who->object_id(), arg));
    EXPECT_EQ(0, g_channel.banlist.query("M:9.9.9.9 L:8.8.8.8"));
}

TEST_F(CmdWizardAdminTest, Ban_ListAll_ReportsEntries)
{
    auto* wiz = AddUser("ban_wiz", "巫师", 200);
    g_channel.banlist.set("10.0.0.1", 1);

    EXPECT_EQ(1, CallBan(wiz, 0, (char*)"all"));
    EXPECT_TRUE(wiz->has("地址：10.0.0.1"));

    g_channel.banlist.del("10.0.0.1");
}

TEST_F(CmdWizardAdminTest, Unban_ByIp_RemovesOrRejects)
{
    auto* wiz = AddUser("unban_wiz", "巫师", 200);
    g_channel.banlist.set("10.0.0.2", 1);

    char ip[] = "10.0.0.2";
    EXPECT_EQ(1, CallUnban(wiz, 0, ip));
    EXPECT_EQ(0, g_channel.banlist.query("10.0.0.2")) << "地址已解除";

    char unknown[] = "10.0.0.3";
    EXPECT_EQ(0, CallUnban(wiz, 0, unknown));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("并没有加入黑名单"));
}

TEST_F(CmdWizardAdminTest, Unban_ByPlayerTarget)
{
    auto* wiz = AddUser("unban_wiz2", "巫师", 200);
    auto* who = AddUser("unban_vic", "平民", 30);
    g_channel.banlist.set(get_ip(who), 1);

    EXPECT_EQ(1, CallUnban(wiz, who->object_id(), (char*)""));
    EXPECT_EQ(0, g_channel.banlist.query(get_ip(who)));
}

// =====================================================================
// freeze / unfreeze / chpass / chverifypass：档案封禁与密码变更
// =====================================================================
TEST_F(CmdWizardAdminTest, Freeze_DbOk_BroadcastsRumor)
{
    auto* wiz = AddUser("freeze_wiz", "巫师", 200);
    EXPECT_EQ(1, CallFreeze(wiz, 0, (char*)"badguy"));
    EXPECT_FALSE(wiz->has("冻结失败")) << "库写成功不报失败";
    EXPECT_TRUE(db->ExecutedContaining("badguy")) << "封禁落库";
}

TEST_F(CmdWizardAdminTest, Freeze_DbFail_TellsUser)
{
    auto* wiz = AddUser("freeze_wiz2", "巫师", 200);
    db->SetExecuteErrorCode(1);
    EXPECT_EQ(1, CallFreeze(wiz, 0, (char*)"badguy2"));
    EXPECT_TRUE(wiz->has("冻结失败"));
}

TEST_F(CmdWizardAdminTest, Unfreeze_TellsUser)
{
    auto* wiz = AddUser("unfreeze_wiz", "巫师", 200);
    EXPECT_EQ(1, CallUnfreeze(wiz, 0, (char*)"badguy"));
    EXPECT_TRUE(wiz->has("你解冻了帐号"));
}

TEST_F(CmdWizardAdminTest, Chpass_ReportsResult)
{
    auto* wiz  = AddUser("chpass_wiz", "巫师", 200);
    char arg[] = "someid newpass";
    EXPECT_EQ(1, CallChpass(wiz, 0, arg));
    EXPECT_TRUE(wiz->has("修改帐号someid密码为newpass"));

    db->SetExecuteErrorCode(1);
    char arg2[] = "someid newpass";
    wiz->clear();
    EXPECT_EQ(1, CallChpass(wiz, 0, arg2));
    EXPECT_TRUE(wiz->has("修改密码失败"));
}

TEST_F(CmdWizardAdminTest, Chverifypass_ReportsResult)
{
    auto* wiz  = AddUser("chvp_wiz", "巫师", 200);
    char arg[] = "someid vpass";
    EXPECT_EQ(1, CallChverifypass(wiz, 0, arg));
    EXPECT_TRUE(wiz->has("身份校验密码为vpass"));
}

// =====================================================================
// wholist：在线名单与统计（连线/断线计数）
// =====================================================================
TEST_F(CmdWizardAdminTest, Wholist_ListsUsersAndCounts)
{
    auto* wiz = AddUser("who_wiz", "巫师", 200);
    wiz->move(room);
    auto* live = AddUser("who_live", "在线者", 30);
    live->move(room);
    auto* dead = AddUser("who_dead", "断线者", 30);
    dead->set_temp("net_dead", 1L);

    EXPECT_EQ(1, CallWholist(wiz, 0, (char*)""));
    EXPECT_TRUE(wiz->has("当前在线玩家")) << "应报在线统计";
    EXPECT_TRUE(wiz->has("在线者")) << "应列出在线玩家";
    EXPECT_TRUE(wiz->has("断线者")) << "断线者也列入名单";
    // 3 人在线表、2 人连线（断线者不计）
    EXPECT_TRUE(wiz->has("当前在线玩家2(3)人")) << "连线口径与总数分列";
}

// =====================================================================
// recover：死亡损失恢复
// =====================================================================
TEST_F(CmdWizardAdminTest, Recover_RestoresDeathLossAndClears)
{
    auto* wiz = AddUser("rec_wiz", "巫师", 200);
    auto* who = AddUser("rec_vic", "平民", 30);
    who->set("combat_exp", 1000L);
    who->set("death/combat_exp_loss", 500L);
    who->set("death/skill_loss", 1L);

    EXPECT_EQ(1, CallRecover(wiz, who->object_id(), (char*)""));
    EXPECT_EQ(1500, who->query("combat_exp")) << "损失经验补回";
    EXPECT_EQ(nullptr, who->querymap("death")) << "死亡记录清空";
    EXPECT_TRUE(wiz->has("恢复完成"));
}

// =====================================================================
// snoop：查询/监听/解除三态与自身守卫
// =====================================================================
TEST_F(CmdWizardAdminTest, Snoop_QuerySetsAndClears)
{
    auto* wiz = AddUser("snoop_wiz", "巫师", 200);
    auto* who = AddUser("snoop_vic", "平民", 30);

    // 初始无监听
    EXPECT_EQ(1, CallSnoop(wiz, 0, (char*)""));
    EXPECT_TRUE(wiz->has("你没有监听任何人"));

    // 设定监听
    wiz->clear();
    char arg[] = "";
    EXPECT_EQ(1, CallSnoop(wiz, who->object_id(), arg));
    EXPECT_TRUE(wiz->has("你开始监听"));
    EXPECT_EQ(static_cast<CChar*>(who), g_channel.query_snooping(wiz));

    // 查询态
    wiz->clear();
    EXPECT_EQ(1, CallSnoop(wiz, 0, (char*)""));
    EXPECT_TRUE(wiz->has("你当前正在监听"));

    // 解除
    wiz->clear();
    char none[] = "none";
    EXPECT_EQ(1, CallSnoop(wiz, 0, none));
    EXPECT_TRUE(wiz->has("解除监听"));
    EXPECT_EQ(nullptr, g_channel.query_snooping(wiz));
}

TEST_F(CmdWizardAdminTest, Snoop_SelfRejected)
{
    auto* wiz  = AddUser("snoop_wiz2", "巫师", 200);
    char arg[] = "";
    EXPECT_EQ(0, CallSnoop(wiz, wiz->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("snoop none"));
}

// =====================================================================
// chblk：关闭/打开玩家交谈频道
// =====================================================================
TEST_F(CmdWizardAdminTest, Chblk_CloseAndOpen)
{
    auto* wiz = AddUser("chblk_wiz", "巫师", 200);
    auto* who = AddUser("chblk_vic", "平民", 30);

    char close_arg[] = "chblk_vic close";
    EXPECT_EQ(1, CallChblk(wiz, who->object_id(), close_arg));
    EXPECT_NE(0, who->query("chblk/all")) << "关闭频道写入时限";
    EXPECT_TRUE(wiz->has("你关闭了"));

    char open_arg[] = "chblk_vic open";
    EXPECT_EQ(1, CallChblk(wiz, who->object_id(), open_arg));
    EXPECT_EQ(nullptr, who->querymap("chblk")) << "开频道清空封锁";
    EXPECT_TRUE(who->has("打开了你的交谈频道"));
}

TEST_F(CmdWizardAdminTest, Chblk_WizardTarget_Rejected)
{
    auto* wiz  = AddUser("chblk_wiz2", "巫师", 200);
    auto* peer = AddUser("chblk_peer", "巫师同僚", 60);
    char arg[] = "chblk_peer close";
    EXPECT_EQ(0, CallChblk(wiz, peer->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能关闭巫师"));
}
