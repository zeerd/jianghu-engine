// test_cmd_user_account.cpp - Unit tests for CmdUserAccount.cpp
//
// Tests game state underlying Quit, Save, SetParam commands.
//
#include "pch.h"
#include <mutex>
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "CmdUser.h"
#include "HeartBeat.h"  // g_destruct_list：quit 成功后回收自毁对象
#include "Mapping.h"
#include "test_state_recorder.h"
#include "mock/mock_database.h"

extern char g_notify_fail_msg[512];
extern int g_current_time;
extern bool g_mock_db_succeed;  // mock_db.cpp：verify_user 的开关
extern MAP2USER g_user_list;    // find_player 走在线用户表

class CmdUserAccountTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallSave(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"save", me, target, arg);
    }
    static int CallSetParam(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"set", me, target, arg);
    }
    static int CallQuit(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"quit", me, target, arg);
    }
    static int CallUnset(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"unset", me, target, arg);
    }
    static int CallMudage(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"mudage", me, target, arg);
    }
    static int CallPasswd(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"password", me, target, arg);
    }
    static int CallValidate(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"validate", me, target, arg);
    }
    static int CallVerifyPass(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"verifypass", me, target, arg);
    }
    static int CallUpgrade(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"upgrade", me, target, arg);
    }
    static int CallTune(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"tune", me, target, arg);
    }
    static int CallRefuseTell(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"refusetell", me, target, arg);
    }
    static int CallListen(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"wantlisten", me, target, arg);
    }
    static int CallRefuse(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"refuse", me, target, arg);
    }

    CChar* me;
    CContainer* room;
    MockDatabase* db = nullptr;

    void SetUp() override
    {
        room = new CContainer();
        room->set("name", "test_room");
        room->set("base_name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->move(room);

        db   = new MockDatabase();
        g_db = db;
    }

    void TearDown() override
    {
        g_db = nullptr;
        delete db;
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }

    // quit 成功会 DESTRUCT_OB：对象进 g_destruct_list 延迟释放。
    // 测试进程无主循环 → 从列表收回并自行 delete（ItemFactory 范式）。
    static void ReclaimDestructed(CContainer* ob)
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.remove(ob);
        delete ob;
    }
};

TEST_F(CmdUserAccountTest, Save_InStartRoom)
{
    room->set("valid_startroom", (long)1);
    int result = CallSave(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserAccountTest, Save_InOwnedRoom)
{
    room->set("owner", "tester1");
    int result = CallSave(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserAccountTest, Save_Normal)
{
    int result = CallSave(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserAccountTest, SetParam_NoArgs)
{
    int result = CallSetParam(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserAccountTest, SetParam_SetVar)
{
    // 空格分隔（analyse_string）：set foo 42 → env/foo = 42。
    // analyse_string 就地改写 arg（空格→NUL）→ 必须传可写缓冲而非字面量
    char arg[] = "foo 42";
    int result = CallSetParam(me, 0, arg);
    EXPECT_EQ(result, 1);
    EXPECT_EQ(42, me->query("env/foo"));
}

TEST_F(CmdUserAccountTest, SetParam_SetStr)
{
    // 契约：只接受数字值；字符串值走“非法”分支不落库
    char arg[] = "bar hello";
    int result = CallSetParam(me, 0, arg);
    EXPECT_EQ(result, 1);
    EXPECT_EQ(0, me->query("env/bar"));
}

// ---------------- save：30 秒冷却 ----------------

TEST_F(CmdUserAccountTest, Save_CooldownSkipsSecondCall)
{
    room->set("valid_startroom", (long)1);
    char arg[256] = "";
    // 第一次：正常存盘并记录 start_city
    ASSERT_EQ(CallSave(me, 0, arg), 1);
    EXPECT_STREQ(me->querystr("start_city"), "test_room");

    // 冷却期内再存：early-return，start_city 不随房间变化更新
    room->set("base_name", "changed_room");
    EXPECT_EQ(CallSave(me, 0, arg), 1);
    EXPECT_STREQ(me->querystr("start_city"), "test_room");
}

// ---------------- quit ----------------

TEST_F(CmdUserAccountTest, Quit_FightingRejected)
{
    StateRecordingUser* u = new StateRecordingUser();
    u->set_name("斗士", "quit_fighter");
    u->set("hp", (long)100);
    u->move(room);
    u->set_temp("fight/is_fighting", (long)1);

    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallQuit(u, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "战斗中不能退出。");
    // 未走到离线/自毁
    EXPECT_EQ(u->environment(), static_cast<CContainer*>(room));
    EXPECT_EQ(u->query_temp("net_dead"), 0);

    u->delete_temp("fight/is_fighting");
    u->move(nullptr);
    delete u;
}

TEST_F(CmdUserAccountTest, Quit_BusyRejected)
{
    StateRecordingUser* u = new StateRecordingUser();
    u->set_name("忙碌者", "quit_busy");
    u->set("hp", (long)100);
    u->move(room);
    u->start_busy(3, NULL, NULL);

    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallQuit(u, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "( 你上一个动作还没有完成，不能退出。)");
    EXPECT_EQ(u->environment(), static_cast<CContainer*>(room));

    u->move(nullptr);
    delete u;
}

TEST_F(CmdUserAccountTest, Quit_SuccessOfflineAndDestruct)
{
    StateRecordingUser* u = new StateRecordingUser();
    u->set_name("退出者", "quitter1");
    u->set("hp", (long)100);
    u->set("login/time", (long)1000000);
    u->move(room);

    // offline() 把 g_current_time 写进 net_dead；时间为 0 时标记不可分辨
    const int saved_time = g_current_time;
    g_current_time       = 1700000000;

    char arg[256] = "";
    ASSERT_EQ(CallQuit(u, 0, arg), 1);

    // 离线标记 + 脱离环境 + 进入延迟释放队列
    EXPECT_EQ(u->query_temp("net_dead"), g_current_time);
    EXPECT_EQ(u->environment(), nullptr);
    g_current_time = saved_time;

    bool pending = false;
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        for (auto* p : g_destruct_list)
            if (p == static_cast<CContainer*>(u)) pending = true;
    }
    EXPECT_TRUE(pending) << "quit 成功必须 DESTRUCT_OB 进延迟释放队列";
    ReclaimDestructed(u);
}

// ---------------- set：环境变数 ----------------

TEST_F(CmdUserAccountTest, SetParam_ListsNumberAndStringVars)
{
    me->set("env/num", (long)42);
    me->set("env/str", "hello");
    char arg[256] = "";
    EXPECT_EQ(CallSetParam(me, 0, arg), 1);
    // 两条列出分支（T_NUMBER / T_STRING）执行不崩溃即通过；
    // 可观察面：环境变数本身仍可查询
    EXPECT_EQ(me->query("env/num"), 42);
    EXPECT_STREQ(me->querystr("env/str"), "hello");
}

TEST_F(CmdUserAccountTest, SetParam_TooManyVarsRejected)
{
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, 32, "env/v%d", i);
        me->set(key, (long)i);
    }
    char arg[]           = "one more";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallSetParam(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你设的环境变数太多了，请先用 unset 删掉几个吧。");
}

TEST_F(CmdUserAccountTest, SetParam_WizOnlyKeyRejectedForMortal)
{
    char arg[]           = "invisibility 1";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallSetParam(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "只有巫师能用这个设定。");
    EXPECT_EQ(me->query("env/invisibility"), 0);
}

TEST_F(CmdUserAccountTest, SetParam_WimpyOutOfRangeRejected)
{
    char arg[]           = "wimpy 100";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallSetParam(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "wimpy [<逃跑时「气」的百分比>]");
    EXPECT_EQ(me->query("env/wimpy"), 0);
}

// ---------------- unset ----------------

TEST_F(CmdUserAccountTest, Unset_NoArgsReturnsZero)
{
    char arg[256] = "";
    EXPECT_EQ(CallUnset(me, 0, arg), 0);
}

TEST_F(CmdUserAccountTest, Unset_DeletesEnvVar)
{
    me->set("env/foo", (long)42);
    char arg[] = "foo";
    EXPECT_EQ(CallUnset(me, 0, arg), 1);
    EXPECT_EQ(me->query("env/foo"), 0);
}

// ---------------- mudage ----------------

TEST_F(CmdUserAccountTest, Mudage_LowHpRejected)
{
    me->set("hp", (long)5);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallMudage(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你的精神不能集中来算自己的年岁。");
}

TEST_F(CmdUserAccountTest, Mudage_ComputesBreakdownAndCostsHp)
{
    // 1月2天3小时4分 → 触发 mon/day/hrs/min 全部分支
    long age = (((1L * 30 + 2) * 24 + 3) * 60 + 4) * 60;
    me->set("mud_age", age);
    long hp_before = me->query("hp");
    char arg[256]  = "";
    EXPECT_EQ(CallMudage(me, 0, arg), 1);
    EXPECT_EQ(me->query("hp"), hp_before - 5);
}

// ---------------- password / validate / verifypass ----------------

TEST_F(CmdUserAccountTest, Passwd_WrongOldPasswordRejected)
{
    g_mock_db_succeed    = false;
    char arg[]           = "oldpass newpass";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPasswd(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "与原密码不符，不能设置新密码！");
    g_mock_db_succeed = true;
}

TEST_F(CmdUserAccountTest, Passwd_WrongVerifyPassRejected)
{
    // verify_user 通过（mock 默认 true），但 verify_password 无查询结果 → false
    char arg[]           = "oldpass newpass";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPasswd(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "身份验证未通过!");
    EXPECT_EQ(me->query_temp("pending/modify_pass"), 0);
}

TEST_F(CmdUserAccountTest, Passwd_FirstStageSetsPending)
{
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    char arg[] = "oldpass newpass";
    ASSERT_EQ(CallPasswd(me, 0, arg), 1);
    EXPECT_NE(me->query_temp("pending/modify_pass"), 0);
}

TEST_F(CmdUserAccountTest, Passwd_NewPasswordTooShortRejected)
{
    me->set_temp("pending/modify_pass", (long)1);
    char arg[]           = "abc";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPasswd(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "密码不能少于5个字元！");
}

TEST_F(CmdUserAccountTest, Passwd_NewPasswordTooLongRejected)
{
    me->set_temp("pending/modify_pass", (long)1);
    char arg[]           = "abcdefghij12345678901";  // 21 > 20
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPasswd(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "您的密码也太长了！");
}

TEST_F(CmdUserAccountTest, Passwd_ChangePasswordDbFails)
{
    me->set_temp("pending/modify_pass", (long)1);
    db->SetExecuteFailAt(0);
    char arg[]           = "newpass";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPasswd(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "修改密码失败！");
    EXPECT_NE(me->query_temp("pending/modify_pass"), 0) << "失败不清 pending";
}

TEST_F(CmdUserAccountTest, Passwd_ChangePasswordSucceeds)
{
    me->set_temp("pending/modify_pass", (long)1);
    char arg[] = "newpass";
    ASSERT_EQ(CallPasswd(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("pending/modify_pass"), 0);
    EXPECT_TRUE(db->ExecutedContaining("cc_passwd"));
}

TEST_F(CmdUserAccountTest, Validate_WrongPasswordRejected)
{
    g_mock_db_succeed    = false;
    char arg[]           = "secret code12";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallValidate(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "密码不符，不能进行进一步的命令操作！");
    g_mock_db_succeed = true;
}

TEST_F(CmdUserAccountTest, Validate_WrongVerifyPassRejected)
{
    char arg[]           = "secret code12";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallValidate(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "暗码不符，不能进行进一步的命令操作！");
    EXPECT_EQ(me->query_temp("validate"), 0);
}

TEST_F(CmdUserAccountTest, Validate_SuccessSetsFlag)
{
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    char arg[] = "secret code12";
    ASSERT_EQ(CallValidate(me, 0, arg), 1);
    EXPECT_NE(me->query_temp("validate"), 0);
}

TEST_F(CmdUserAccountTest, Validate_AlreadyValidatedSkipsChecks)
{
    me->set_temp("validate", (long)1);
    g_mock_db_succeed = false;  // 即便凭据校验必败也不再触发
    char arg[]        = "anything here";
    EXPECT_EQ(CallValidate(me, 0, arg), 1);
    g_mock_db_succeed = true;
}

TEST_F(CmdUserAccountTest, VerifyPass_WrongOldRejected)
{
    char arg[]           = "oldcode newcode";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVerifyPass(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "身份验证未通过!");
}

TEST_F(CmdUserAccountTest, VerifyPass_TooShortRejected)
{
    // 单 token：msg 拿副本、arg 清空；靠模式匹配让 verify_password 通过
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    char arg[]           = "abc";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVerifyPass(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "暗码不能少于5个字元！");
}

TEST_F(CmdUserAccountTest, VerifyPass_TooLongRejected)
{
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    char arg[]           = "abcdefghij12345678901";  // 21 > 20
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVerifyPass(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "您的暗码也太长了！");
}

TEST_F(CmdUserAccountTest, VerifyPass_DbChangeFails)
{
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    db->SetExecuteFailAt(0);
    char arg[]           = "oldcode newcode1";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallVerifyPass(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "修改暗码失败！");
}

TEST_F(CmdUserAccountTest, VerifyPass_Succeeds)
{
    db->SetQueryResultMatching("cc_verifypass",
                               MockDatabase::MakeSingleRow({"x"}));
    char arg[] = "oldcode newcode1";
    EXPECT_EQ(CallVerifyPass(me, 0, arg), 1);
    EXPECT_TRUE(db->ExecutedContaining("cc_verifypass"));
}

// ---------------- upgrade ----------------

TEST_F(CmdUserAccountTest, Upgrade_NotEnoughExpRejected)
{
    me->set("combat_exp", (long)0);
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallUpgrade(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你当前的经验不够升一级！");
    EXPECT_EQ(me->query("level"), 50);
}

TEST_F(CmdUserAccountTest, Upgrade_SucceedsAndLevelsUp)
{
    // combat_exp = L(51) → valid_exp = L(51)-L(50) = need_exp，恰好达标
    me->set("combat_exp", (long)CChar::level2_exp(51));
    char arg[256] = "";
    ASSERT_EQ(CallUpgrade(me, 0, arg), 1);
    EXPECT_EQ(me->query("level"), 51);
}

// ---------------- tune ----------------

TEST_F(CmdUserAccountTest, Tune_RemovesTunedChannel)
{
    CVector v;
    v.append("chat");
    me->set("channels", v);

    char arg[] = "chat";
    EXPECT_EQ(CallTune(me, 0, arg), 1);
    CVector* after = me->queryvec("channels");
    ASSERT_NE(after, (CVector*)nullptr);
    EXPECT_EQ(after->find("chat"), after->end());
}

TEST_F(CmdUserAccountTest, Tune_UnknownChannelPrintsHint)
{
    char arg[256] = "nosuchchannel";
    EXPECT_EQ(CallTune(me, 0, arg), 1);
}

// ---------------- refusetell / wantlisten ----------------

// 在线用户登记（find_player 走 g_user_list）；析构即摘除
class ScopedAccountUser {
public:
    ScopedAccountUser(const char* id, const char* name)
    {
        u = new StateRecordingUser();
        u->set_name(name, id);
        g_user_list[u->object_id()] = u;
    }
    ~ScopedAccountUser()
    {
        g_user_list.erase(u->object_id());
        u->move(nullptr);
        delete u;
    }
    ScopedAccountUser(const ScopedAccountUser&)            = delete;
    ScopedAccountUser& operator=(const ScopedAccountUser&) = delete;
    StateRecordingUser* get() const { return u; }

private:
    StateRecordingUser* u;
};

TEST_F(CmdUserAccountTest, RefuseTell_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRefuseTell(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "目前找不到这个人。");
}

TEST_F(CmdUserAccountTest, RefuseTell_TargetSetsNotellKey)
{
    ScopedAccountUser bell("bell1", "铃铛");
    char arg[256] = "";
    ASSERT_EQ(CallRefuseTell(me, bell.get()->object_id(), arg), 1);
    EXPECT_NE(me->query_temp("notell/bell1"), 0);
}

TEST_F(CmdUserAccountTest, RefuseTell_NoTargetBlocksAll)
{
    char arg[256] = "";
    ASSERT_EQ(CallRefuseTell(me, 0, arg), 1);
    EXPECT_NE(me->query_temp("notell/all"), 0);
}

TEST_F(CmdUserAccountTest, Listen_TargetClearsNotellKeys)
{
    ScopedAccountUser bell("bell2", "铃铛2");
    me->set_temp("notell/bell2", (long)1);
    me->set_temp("notell/all", (long)1);
    char arg[256] = "";
    ASSERT_EQ(CallListen(me, bell.get()->object_id(), arg), 1);
    EXPECT_EQ(me->query_temp("notell/bell2"), 0);
    EXPECT_EQ(me->query_temp("notell/all"), 0);
}

TEST_F(CmdUserAccountTest, Listen_UnknownTargetRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallListen(me, 999999, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "目前找不到这个人。");
}

TEST_F(CmdUserAccountTest, Listen_NoTargetClearsWholeNotell)
{
    me->set_temp("notell/someone", (long)1);
    char arg[256] = "";
    ASSERT_EQ(CallListen(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("notell/someone"), 0);
}

// ---------------- refuse ----------------

TEST_F(CmdUserAccountTest, Refuse_NoChannelsConfiguredRejected)
{
    char arg[256]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRefuse(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你目前没有设置任何信息通道。");
}

TEST_F(CmdUserAccountTest, Refuse_ListsClosedChannels)
{
    me->set_temp("refuse/practice", (long)1);
    char arg[256] = "";
    EXPECT_EQ(CallRefuse(me, 0, arg), 1);
}

TEST_F(CmdUserAccountTest, Refuse_NoneRestoresAll)
{
    me->set_temp("refuse/practice", (long)1);
    char arg[] = "none";
    EXPECT_EQ(CallRefuse(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("refuse/practice"), 0);
}

TEST_F(CmdUserAccountTest, Refuse_UnknownChannelRejected)
{
    char arg[]           = "hp";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRefuse(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "不允许关闭该信息通道。");
    EXPECT_EQ(me->query_temp("refuse/hp"), 0);
}

TEST_F(CmdUserAccountTest, Refuse_ClosesWhitelistedChannel)
{
    char arg[] = "practice";
    EXPECT_EQ(CallRefuse(me, 0, arg), 1);
    EXPECT_NE(me->query_temp("refuse/practice"), 0);
}
