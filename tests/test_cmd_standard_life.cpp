// test_cmd_standard_life.cpp - Unit tests for CmdStandardLife.cpp
//
// Tests CCmdStandard::valid_move, wakeup1 (free function).
//
#include "pch.h"
#include "Char.h"
#include "Item.h"
#include "mock/mock_itemmgr.h"
#include "User.h"
#include "Room.h"
#include "CmdStandard.h"
#include "GameInit.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

// protected call_out 存储的测试暴露（收尾清列表防悬垂）
struct LifeExposed : CChar {
    using CChar::m_callout_list;
};
struct LifeUser : CUser {
    using CChar::m_busy_count;
    using CChar::m_busy_func;
    using CUser::m_callout_list;
};

class CmdStandardLifeTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallSay(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"say", me, target, arg);
    }
    static int CallGoDoor(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"go", me, target, arg);
    }
    static int CallSleep(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"sleep", me, target, arg);
    }
    static int call_exercise(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"exercise", me, target, arg);
    }

    static int call_exercising(CChar* me)
    {
        return CCmdStandard::exercising(me);
    }

    static int call_halt_exercise(CChar* me, CChar* who, char* how)
    {
        return CCmdStandard::halt_exercise(me, who, how);
    }

    static void call_wakeup2(CContainer* ob, int32_t param1, int32_t param2)
    {
        CCmdStandard::wakeup2(ob, param1, param2);
    }

    static int call_tell(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"tell", me, target, arg);
    }

    static int call_look(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"look", me, target, arg);
    }

    CChar* me;
    CContainer* room;
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr  = g_item_mgr;
        g_item_mgr = &item_mgr;
        room       = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->set("mp", (long)50);
        me->set("max_mp", (long)50);
        me->set("eff_hp", (long)100);
        me->set("con", (long)20);
        me->set("combat_exp", (long)100000);
        me->move(room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
        g_item_mgr = saved_mgr;
    }
};

TEST_F(CmdStandardLifeTest, Say_Empty)
{
    int result = CallSay(me, 0, (char*)"");
    // say with empty text may still succeed (broadcasts empty)
    EXPECT_NE(result, -1);
}

TEST_F(CmdStandardLifeTest, Say_WithText)
{
    int result = CallSay(me, 0, (char*)"hello world");
    // say should succeed
    EXPECT_EQ(result, 1);
}

TEST_F(CmdStandardLifeTest, GoDoor_NoDoor)
{
    int result = CallGoDoor(me, 0, (char*)"north");
    // no door → fail
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardLifeTest, Wakeup1_Basic)
{
    me->disable_player("昏迷不醒");
    me->set("hp", (long)0);
    me->set("mp", (long)0);
    wakeup1(me, 0, 0);
    EXPECT_EQ(me->query("hp"), me->query("eff_hp"));
}

TEST_F(CmdStandardLifeTest, Wakeup1_Null) { wakeup1(nullptr, 0, 0); }

// ---------------- Sleep 准入 ----------------
TEST_F(CmdStandardLifeTest, Sleep_OrdinaryCharRejectedOutsideSleepRoom)
{
    ensure_lua_env();

    g_notify_fail_msg[0] = '\0';
    int result           = CallSleep(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里不是睡觉的地方。");
}

// 钩子缺席时回退到数据集无关的准入：门派豁免不再生效
TEST_F(CmdStandardLifeTest, Sleep_FallbackRejectsFamilyExempt)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(
        luaL_dostring(
            L, "__orig_char_rule = hooks.char_rule; hooks.char_rule = nil"),
        LUA_OK);

    me->set("family/family_name", "丐帮");

    g_notify_fail_msg[0] = '\0';
    int result           = CallSleep(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里不是睡觉的地方。");

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);
}

extern int g_current_time;

// 记录被搭话的内容，证明 say 确实把话转达给目标
class SayRelaySpy : public CChar {
public:
    void relay_say(CChar* me, char* msg) override
    {
        relay_count++;
        last_said = msg ? msg : "";
    }

    int relay_count = 0;
    std::string last_said;
};

// ---------------- exercise：打坐练内功 ----------------
TEST_F(CmdStandardLifeTest, Exercise_WithoutMappedForce_Rejected)
{
    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_exercise(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你必须先用 enable 选择你要用的内功心法。");
}

TEST_F(CmdStandardLifeTest, Exercise_CostSettingGuards)
{
    me->query_skill_map()->set("force", "test_gong");
    char arg[8] = "";

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_exercise(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你要花多少气练功？用set exercise 10设置。");

    me->set("env/exercise", (long)5);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_exercise(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你最少要花 10 点「气」才能练功。用set exercise 10设置。");

    me->set("env/exercise", (long)500);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_exercise(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你现在的气太少了，无法产生内息运行全身经脉。");
}

TEST_F(CmdStandardLifeTest, Exercise_StartsBusyAndRecordsCost)
{
    me->query_skill_map()->set("force", "test_gong");
    me->set("env/exercise", (long)50);

    char arg[8] = "";
    EXPECT_EQ(call_exercise(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("pending/exercising"), 1);
    EXPECT_EQ(me->query_temp("exercise_cost"), 50);
    EXPECT_TRUE(me->is_busy());
}

// 气不足时中断打坐，且内力按 max_mp*2 收口
TEST_F(CmdStandardLifeTest, Exercise_AbortsWhenHpTooLow)
{
    me->query_skill_map()->set("force", "test_gong");
    me->set("env/exercise", (long)50);
    char arg[8] = "";
    ASSERT_EQ(call_exercise(me, 0, arg), 1);

    me->set("mp", (long)200);
    me->set("max_mp", (long)50);
    me->set("hp", (long)0);

    EXPECT_EQ(call_exercising(me), 0);
    EXPECT_EQ(me->query("mp"), 100) << "内力过高必须收口到 max_mp*2";
}

// 没有剩余打坐额度时直接收口（防止内力过高）
TEST_F(CmdStandardLifeTest, Exercise_WithoutBudgetCapsMp)
{
    me->set("mp", (long)200);
    me->set("max_mp", (long)50);

    EXPECT_EQ(call_exercising(me), 0);
    EXPECT_EQ(me->query("mp"), 100);
    EXPECT_EQ(me->query_temp("pending/exercising"), 0);
}

TEST_F(CmdStandardLifeTest, Exercise_RoundCompletesAndRaisesMp)
{
    me->query_skill_map()->set("force", "test_gong");
    me->set("env/exercise", (long)10);
    char arg[8] = "";
    ASSERT_EQ(call_exercise(me, 0, arg), 1);
    ASSERT_EQ(me->query_temp("exercise_cost"), 10);

    // 只剩 1 点额度：一轮结算后必然收工
    me->set_temp("exercise_cost", 1);
    long mp_before = me->query("mp");

    EXPECT_EQ(call_exercising(me), 0);
    EXPECT_GT(me->query("mp"), mp_before) << "打坐必须增长内力";
    EXPECT_EQ(me->query_temp("pending/exercising"), 0) << "额度用尽即收工";
}

// 内力满且内力修为远超技能上限 → 到瓶颈，封顶不涨
TEST_F(CmdStandardLifeTest, Exercise_Finish_AtBottleneckKeepsMaxMp)
{
    me->query_skill_map()->set("force", "test_gong");
    me->set("env/exercise", (long)10);
    char arg[8] = "";
    ASSERT_EQ(call_exercise(me, 0, arg), 1);

    me->set_temp("exercise_cost", 1);
    me->set("mp", (long)200);
    me->set("max_mp", (long)50);

    EXPECT_EQ(call_exercising(me), 0);
    EXPECT_EQ(me->query("mp"), 50) << "瓶颈期按 max_mp 封顶";
    EXPECT_EQ(me->query("max_mp"), 50) << "瓶颈期不涨上限";
}

// 内力充足且修为未到瓶颈 → 内力上限 +1
TEST_F(CmdStandardLifeTest, Exercise_Finish_RaisesMaxMp)
{
    me->query_skill_map()->set("force", "test_gong");
    me->set_skill("force", 100);
    me->set("env/exercise", (long)10);
    char arg[8] = "";
    ASSERT_EQ(call_exercise(me, 0, arg), 1);

    me->set_temp("exercise_cost", 1);
    me->set("mp", (long)200);
    me->set("max_mp", (long)50);

    EXPECT_EQ(call_exercising(me), 0);
    EXPECT_EQ(me->query("max_mp"), 51) << "未到瓶颈则内力上限 +1";
    EXPECT_EQ(me->query("mp"), 51) << "内力重置为新上限";
}

TEST_F(CmdStandardLifeTest, HaltExercise_AllThreeReasons)
{
    me->set("mp", (long)200);
    me->set("max_mp", (long)50);

    const char* reasons[] = {"halt", "kill", "other"};
    for (const char* how : reasons) {
        me->set_temp("pending/exercising", 1);
        EXPECT_EQ(call_halt_exercise(me, nullptr, (char*)how), 1) << how;
        EXPECT_EQ(me->query_temp("pending/exercising"), 0) << how;
        EXPECT_EQ(me->query("mp"), 100) << how;
        me->set("mp", (long)200);
    }
}

// ---------------- wakeup2：同床醒来 ----------------
TEST_F(CmdStandardLifeTest, Wakeup2_FemaleWakesUpRestored)
{
    me->set("gender", "女性");
    me->set("hp", (long)100);
    me->set("max_hp", (long)100);
    me->set("mp", (long)0);
    me->set("max_mp", (long)50);
    me->set("str", (long)20);
    me->set("con", (long)20);
    me->set_temp("block_msg/all", 1);
    me->disable_player("睡梦中");

    call_wakeup2(me, 0, 0);

    EXPECT_EQ(me->query("mp"), 50) << "醒来补满内力";
    EXPECT_EQ(me->query("max_atman"), 1);
    EXPECT_EQ(me->query("hp"), 50) << "扣掉 50 点体力";
    EXPECT_EQ(me->query_temp("block_msg/all"), 0);
    EXPECT_EQ(me->query_attr(CChar::IS_LIVING), 1) << "醒后解除昏睡";
}

TEST_F(CmdStandardLifeTest, Wakeup2_MaleWakesUpRestored)
{
    me->set("gender", "男性");
    me->set("hp", (long)100);
    me->set("max_hp", (long)100);
    me->set("mp", (long)0);
    me->set("max_mp", (long)50);
    me->set("str", (long)10);
    me->set("con", (long)30);
    me->set_temp("block_msg/all", 1);
    me->disable_player("睡梦中");

    call_wakeup2(me, 0, 0);

    EXPECT_EQ(me->query("mp"), 50);
    EXPECT_EQ(me->query("max_atman"), 1);
    EXPECT_EQ(me->query("hp"), 150) << "体力占优时 kee_cost 为负，反而恢复体力";
    EXPECT_EQ(me->query_temp("block_msg/all"), 0);
}

// ---------------- Sleep：睡觉 ----------------
TEST_F(CmdStandardLifeTest, Sleep_RejectsNonSleepRoom)
{
    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallSleep(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这里不是睡觉的地方。");
}

TEST_F(CmdStandardLifeTest, Sleep_AloneInSleepRoom)
{
    room->set("sleep_room", 1);
    me->set("con", (long)20);
    me->set("last_sleep", (long)-1000000);

    char arg[8] = "";
    EXPECT_EQ(CallSleep(me, 0, arg), 1);
    EXPECT_EQ(me->query_temp("block_msg/all"), 1) << "睡觉屏蔽频道消息";
    EXPECT_EQ(me->query("last_sleep"), g_current_time) << "记录入睡时刻";
    EXPECT_EQ(me->query_attr(CChar::IS_LIVING), 0) << "睡着即失去行动力";
}

// ---------------- say：说话 ----------------
TEST_F(CmdStandardLifeTest, Say_NoSayFlag_Rejected)
{
    me->set("no_say", 1);
    char arg[16]         = "复述一句";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallSay(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "由于发送了大量信息，你的说话权利被禁止了。");
}

// 有目标时走"对某人说"文案并把话转达给目标
TEST_F(CmdStandardLifeTest, Say_ToTargetRelaysToTarget)
{
    SayRelaySpy listener;
    listener.set_name("听众", "life_listener");
    listener.move(room);

    char arg[16] = "你好吗";
    EXPECT_EQ(CallSay(me, listener.object_id(), arg), 1);
    EXPECT_EQ(listener.relay_count, 1);
    EXPECT_EQ(listener.last_said, "你好吗");

    listener.move(nullptr);
}

// 血量低于 20% 时气若游丝，词间打点省略
TEST_F(CmdStandardLifeTest, Say_LowHpTruncatesText)
{
    StateRecordingUser observer;
    observer.set_name("旁观", "life_observer");
    observer.move(room);
    observer.delete_temp("client/loading");
    observer.clear();

    me->set("hp", (long)10);
    char arg[16] = "你好 世界";
    EXPECT_EQ(CallSay(me, 0, arg), 1);
    EXPECT_TRUE(observer.has(" ... ")) << observer.msgs.size();

    observer.move(nullptr);
}

extern MAP2USER g_user_list;

// ---------------- tell：私聊 ----------------
TEST_F(CmdStandardLifeTest, Tell_UnknownTarget_Rejected)
{
    char arg[8]          = "hi";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_tell(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "没有这个人。。。");
}

TEST_F(CmdStandardLifeTest, Tell_UnreachableTarget_Rejected)
{
    StateRecordingUser silent;
    silent.set_name("昏睡者", "life_sleeper");
    silent.move(room);
    silent.delete_temp("client/loading");
    silent.disable_player("睡梦中");
    g_user_list[silent.object_id()] = &silent;

    char arg[8]          = "hi";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_tell(me, silent.object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方听不到你讲话。");

    g_user_list.erase(silent.object_id());
    silent.move(nullptr);
}

TEST_F(CmdStandardLifeTest, Tell_NotellTarget_Rejected)
{
    StateRecordingUser picky;
    picky.set_name("勿扰者", "life_picky");
    picky.move(room);
    picky.delete_temp("client/loading");
    picky.set_temp("notell/tester1", 1);
    g_user_list[picky.object_id()] = &picky;

    char arg[8]          = "hi";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_tell(me, picky.object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "勿扰者不接受你的悄悄话。");

    g_user_list.erase(picky.object_id());
    picky.move(nullptr);
}

TEST_F(CmdStandardLifeTest, Tell_DeliversMessageToTarget)
{
    StateRecordingUser friend_user;
    friend_user.set_name("好友", "life_friend");
    friend_user.move(room);
    friend_user.delete_temp("client/loading");
    g_user_list[friend_user.object_id()] = &friend_user;
    friend_user.clear();

    char arg[16] = "吃饭了吗";
    EXPECT_EQ(call_tell(me, friend_user.object_id(), arg), 1);
    EXPECT_TRUE(friend_user.has("M=吃饭了吗")) << friend_user.msgs.size();

    g_user_list.erase(friend_user.object_id());
    friend_user.move(nullptr);
}

// ---------------- look：察看 ----------------
TEST_F(CmdStandardLifeTest, Look_UnknownTarget_Rejected)
{
    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_look(me, 123456, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要观察什么？");
}

TEST_F(CmdStandardLifeTest, Look_ItemInRoom)
{
    CItem* scroll = new CItem();
    scroll->set_name("一卷竹简", "life_scroll");
    scroll->move(room);

    char arg[8] = "";
    EXPECT_EQ(call_look(me, scroll->object_id(), arg), 1);

    scroll->move(nullptr);
    delete scroll;
}

TEST_F(CmdStandardLifeTest, Look_RoomWhenNoTarget)
{
    CRoom hall;
    hall.set("name", "life_hall");
    hall.set("infinity", 1);
    me->move(&hall);

    char arg[8] = "";
    EXPECT_EQ(call_look(me, 0, arg), 1);

    me->move(nullptr);
}

// ---------------- Sleep：双人同眠 ----------------
TEST_F(CmdStandardLifeTest, Sleep_PartnerHandshakeThenTogether)
{
    CRoom inn;
    inn.set("name", "life_inn");
    inn.set("infinity", 1);
    inn.set("sleep_room", 1);

    StateRecordingUser him;
    him.set_name("书生", "life_him");
    him.set("gender", "男性");
    him.set("hp", (long)100);
    him.set("max_hp", (long)100);
    him.set("last_sleep", (long)-1000000);

    StateRecordingUser her;
    her.set_name("姑娘", "life_her");
    her.set("gender", "女性");
    her.set("hp", (long)100);
    her.set("max_hp", (long)100);
    her.set("last_sleep", (long)-1000000);

    him.move(&inn);
    her.move(&inn);
    him.delete_temp("client/loading");
    her.delete_temp("client/loading");

    char ask[8] = "";
    // 第一步：书生提出，姑娘考虑中
    EXPECT_EQ(CallSleep(&him, her.object_id(), ask), 1);
    EXPECT_EQ(him.query_temp("pending/sleep"), her.object_id());
    EXPECT_EQ(her.query_attr(CChar::IS_LIVING), 1) << "尚未拍板不应昏睡";

    // 第二步：姑娘应允，双双入睡
    EXPECT_EQ(CallSleep(&her, him.object_id(), ask), 1);
    EXPECT_EQ(him.query_temp("block_msg/all"), 1);
    EXPECT_EQ(her.query_temp("block_msg/all"), 1);
    EXPECT_EQ(him.query("last_sleep"), g_current_time);
    EXPECT_EQ(her.query("last_sleep"), g_current_time);
    EXPECT_EQ(him.query_attr(CChar::IS_LIVING), 0);
    EXPECT_EQ(her.query_attr(CChar::IS_LIVING), 0);

    him.move(nullptr);
    her.move(nullptr);
}

// ====================================================================
// Sleep 深路：独睡 sleep_room 支 + 冷却 + 定向守卫链 + 同睡成功尾
// （测试进程 g_current_time=0 会与 last_sleep=0 撞 90 秒冷却 → 每用例推进时间）
// ====================================================================

TEST_F(CmdStandardLifeTest, SleepSoloInSleepRoomThenCooldown)
{
    int saved_time = g_current_time;
    g_current_time = 1000000;
    room->set("sleep_room", 1);
    room->set("sleep_room", 1);

    g_notify_fail_msg[0] = '\0';
    int r                = CallSleep(me, 0, (char*)"");
    EXPECT_EQ(1, r) << "sleep_room 独睡成功(139-144)";
    EXPECT_GT(me->query("last_sleep"), 0) << "last_sleep 已记录(141)";
    EXPECT_EQ(1, me->query_temp("block_msg/all")) << "屏蔽消息开启(142)";
    EXPECT_EQ('\0', g_notify_fail_msg[0]) << "无拒绝消息";

    me->remove_call_out(wakeup1);  // 摘 wakeup1 防悬垂
    me->enable_player();

    // 冷却：last_sleep 刚写 → 90 秒内再睡被拒
    g_notify_fail_msg[0] = '\0';
    int r2               = CallSleep(me, 0, (char*)"");
    EXPECT_EQ(0, r2) << "冷却期拒绝(124)";
    EXPECT_NE(g_notify_fail_msg[0], '\0') << "冷却提示已给出";

    g_current_time = saved_time;
}

TEST_F(CmdStandardLifeTest, SleepForcedAloneTakesElseBranch)
{
    // 非睡房 + force_sleep 越过首守卫 → 独睡走 else 支(139-144)
    int saved_time = g_current_time;
    g_current_time = 1000000;
    me->set_temp("force_sleep", 1);

    g_notify_fail_msg[0] = '\0';
    int r                = CallSleep(me, 0, (char*)"");
    EXPECT_EQ(1, r) << "force_sleep 独睡成功(139-144)";
    EXPECT_GT(me->query("last_sleep"), 0) << "last_sleep 已记录(141)";
    EXPECT_EQ(1, me->query_temp("block_msg/all")) << "屏蔽消息开启(142)";
    EXPECT_EQ('\0', g_notify_fail_msg[0]) << "无拒绝消息";

    me->remove_call_out(wakeup1);  // 摘 wakeup1 防悬垂
    me->enable_player();
    me->delete_temp("force_sleep");
    g_current_time = saved_time;
}
TEST_F(CmdStandardLifeTest, SleepTargetedGuardsWalkthrough)
{
    // 定向同睡守卫链：158 找人 → 160 非玩家 → 168 性别 → 171 存活 →
    // 178 血量 → 181 自身 condition → 186 对方 condition → 191 对方装备 →
    // 195 自身装备。每步消息互异，防止被更早守卫静默拦截而断言失真。
    int saved_time = g_current_time;
    g_current_time = 1000000;
    room->set("sleep_room", 1);

    char prev_msg[512];
    char find_msg[512];

    // 158: 目标不存在
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, 999999, (char*)"")) << "找不到人(158)";
    ASSERT_NE(g_notify_fail_msg[0], '\0') << "158 应给出拒绝消息";
    snprintf(find_msg, sizeof(find_msg), "%s", g_notify_fail_msg);
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);

    // 160: 目标非玩家（NPC）
    CChar npc_obj;
    npc_obj.set_name("过客", "sleep_npc");
    npc_obj.move(room);
    ASSERT_TRUE(room->present(npc_obj.object_id())) << "NPC 入房可见";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, npc_obj.object_id(), (char*)""))
        << "NPC 不陪睡(160)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中与上一守卫不同的 160";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);

    // 168: 性别不配对（双方皆未设性别 → 天然不成立）
    CUser target;
    target.set_name("路人", "sleep_target");
    target.set("id", "sleep_target");
    target.move(room);
    target.delete_temp(
        "client/loading");  // 入场加载态使 present() 按不可见跳过
    ASSERT_TRUE(room->present(target.object_id())) << "CUser 入房可见";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "性别不配(168)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 168 而非更早守卫";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);

    // 171: 性别配对但目标昏迷（IS_LIVING 由 disabled 临时态决定）
    me->set("gender", "男性");
    target.set("gender", "女性");
    target.disable_player("昏迷");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "昏迷不可睡(171)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 171";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);
    target.enable_player();

    // 178: 血量不足 50%（此前各守卫全过）
    long saved_hp = me->query("hp");
    me->set("hp", (long)10);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "血量不足(178)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 178";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);
    me->set("hp", saved_hp);

    // 181: 自身带 condition 且无 wc_poison
    me->set("condition/cold", (long)1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "自身带状态(181)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 181";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);
    me->del("condition/cold");
    me->del("condition");

    // 186: 对方带 condition（自身已清）
    target.set("condition/cold", (long)1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "对方带状态(186)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 186 而非残留的 181";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);
    target.del("condition/cold");
    target.del("condition");

    // 191: 对方穿着装备
    target.set_temp("apply/index", (long)1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "对方穿着装备(191)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 191";
    snprintf(prev_msg, sizeof(prev_msg), "%s", g_notify_fail_msg);
    target.delete_temp("apply/index");

    // 195: 自身穿着装备
    me->set_temp("apply/index", (long)1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallSleep(me, target.object_id(), (char*)""))
        << "自身穿着装备(195)";
    ASSERT_NE(g_notify_fail_msg[0], '\0');
    EXPECT_STRNE(g_notify_fail_msg, prev_msg) << "命中 195";
    me->delete_temp("apply/index");

    // 收尾：栈对象先出房再析构（CContainer 析构不摘环境条目，残留悬垂
    // 指针会让 TearDown 的 move 遍历 SEH）
    target.move(nullptr);
    npc_obj.move(nullptr);
    EXPECT_EQ(1u, room->get_item_list()->size()) << "房内仅剩 me，无栈对象残留";

    g_current_time = saved_time;
}
TEST_F(CmdStandardLifeTest, Wakeup2CostCapMpRefillAndNestedJump)
{
    // 嵌套：me 放进一个角色容器 → while 真 → 277 向上跳一层
    CChar carrier;
    carrier.set_name("背夫", "wk_carrier");
    carrier.move(room);
    me->move(&carrier);

    // kee_cost = 50 + 5*(str-con) → str 高/con 低使 kee_cost 超过 max_hp/2(266)
    me->set("str", (long)100);
    me->set("con", (long)0);
    long hp_before = me->query("hp");
    me->set("mp", (long)10);  // mp < max_mp → 268 回填

    call_wakeup2(me, 0, 0);

    EXPECT_EQ(me->query("max_mp"), me->query("mp")) << "mp 回满(268)";
    EXPECT_LT(me->query("hp"), hp_before) << "kee_cost 按封顶扣血(266)";
    EXPECT_EQ(static_cast<CContainer*>(room), me->environment())
        << "嵌套环境逐层跳出(277)";
    EXPECT_EQ('\0', me->querystr_temp("disabled")[0]) << "唤醒解除睡眠态";
    carrier.move(nullptr);  // 栈容器先出房再析构，防 room 列表悬垂
}
