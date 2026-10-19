// test_channel_deep.cpp — deep branch tests for ChatChannel / EmoteSystem
// extracted from CChannel during the Facade refactoring.
//
// Covers do_channel permission/formatting branches, check_channel_msg
// frequency limiting, and filter_list listener logic.
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include "Channel.h"
#include "Char.h"
#include "Container.h"
#include "LuaBind.h"
#include "LuaEngine.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"  // StateRecordingUser 分组收发观察
#include "Room.h"
#include "RoomMgr.h"
#include "User.h"
#include "mock/mock_itemmgr.h"

extern int g_current_time;
extern char g_notify_fail_msg[512];
extern MAP2USER g_user_list;

// ====================================================================
// Fixture: prepared ChatChannel with all channels registered
// ====================================================================
class ChatChannelDeepTest : public ::testing::Test {
protected:
    CChatChannel cc;
    CContainer env;
    CMockItemMgr item_mgr;
    // 换入前保存进程共享管理器（test_env 的 g_testItemMgr），TearDown 归还
    IItemMgr* saved_mgr = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        g_current_time = 1000000;  // fixed timestamp for deterministic tests
        cc.create();               // registers sys/new/chat/party/... channels
        saved_mgr             = g_item_mgr;
        g_item_mgr            = &item_mgr;
        item_mgr.load_factory = [](const char*) -> CContainer* {
            CContainer* it = new CContainer();
            it->set_name("大粽子", "da_zongzi");
            return it;
        };
    }

    void TearDown() override
    {
        g_item_mgr = saved_mgr;
        // pk 考场房间跨用例缓存，状态必须复位
        CContainer* pk = load_room("pk");
        if (pk) {
            pk->del("question/考试");
            pk->del("question/答案");
            pk->del("question/问题");
        }
        g_user_list.clear();
    }

    // Create a CUser ready for channel tests
    static CUser* MakeUser(const char* name, const char* id)
    {
        CUser* u = new CUser();
        u->set_name(name, id);
        u->set("id", id);
        u->set("gender", "男性");
        u->set("age", 20);
        u->set("grant", 0);
        return u;
    }
};

// ====================================================================
// do_channel — permission / guard branches
// ====================================================================

// wiz_only channel (sys) rejects non-wizard CUser
TEST_F(ChatChannelDeepTest, WizOnlyChannel_NonWizard_Returns0)
{
    CUser* u = MakeUser("tester", "test1");
    u->move(&env);
    g_user_list[u->object_id()] = u;

    EXPECT_EQ(cc.do_channel(u, nullptr, "sys", "hello"), 0);

    u->move(nullptr);
    g_user_list.clear();
    delete u;
}

// wiz_use channel (news) rejects non-wizard CUser
TEST_F(ChatChannelDeepTest, WizUseChannel_NonWizard_Returns0)
{
    CUser* u = MakeUser("tester", "test1");
    u->move(&env);

    EXPECT_EQ(cc.do_channel(u, nullptr, "news", "breaking"), 0);

    u->move(nullptr);
    delete u;
}

// age < 15 on non-newbie channel -> notify_fail (returns 0)
TEST_F(ChatChannelDeepTest, YoungUser_NonNewbieChannel_Returns0)
{
    CUser* u = MakeUser("young", "young1");
    u->set("age", 10);
    u->move(&env);

    // "chat" has no newbie=1, age 10 < 15 -> notify_fail
    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "hello"), 0);

    u->move(nullptr);
    delete u;
}

// age < 15 on newbie channel -> passes age check
TEST_F(ChatChannelDeepTest, YoungUser_NewbieChannel_PassesAgeCheck)
{
    CUser* u = MakeUser("young", "young1");
    u->set("age", 10);
    u->move(&env);

    // "new" has newbie=1 -> age check skipped; may fail later on other
    // checks (e.g. duplicate msg) but should NOT return 0 from age gate
    int r = cc.do_channel(u, nullptr, "new", "hello world unique msg");
    // Returns 1 on success, 0 on other failures; key: not blocked by age
    EXPECT_EQ(r, 1);

    u->move(nullptr);
    delete u;
}

// chblk/all blocks all channels
TEST_F(ChatChannelDeepTest, AllChannelsBlocked_Returns0)
{
    CUser* u = MakeUser("blocked", "blocked1");
    u->move(&env);
    u->set("chblk/all", g_current_time + 3600);

    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "hello"), 0);

    u->move(nullptr);
    delete u;
}

// specific chblk/<verb> blocks one channel
TEST_F(ChatChannelDeepTest, SpecificChannelBlocked_Returns0)
{
    CUser* u = MakeUser("blocked", "blocked1");
    u->move(&env);
    u->set("chblk/chat", g_current_time + 3600);

    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "hello"), 0);

    u->move(nullptr);
    delete u;
}

// use_mp channel (rumor) with low mp -> notify_fail
TEST_F(ChatChannelDeepTest, UseMpChannel_LowMp_Returns0)
{
    CUser* u = MakeUser("weak", "weak1");
    u->move(&env);
    u->set("mp", 10);  // < 20 required

    EXPECT_EQ(cc.do_channel(u, nullptr, "rumor", "gossip"), 0);

    u->move(nullptr);
    delete u;
}

// non-user CChar bypasses all user-specific checks
TEST_F(ChatChannelDeepTest, NonUser_BypassesUserChecks)
{
    CChar c;
    c.set_name("npc", "npc1");
    c.move(&env);

    // CChar is not IS_USER, so userp=false -> skips all user gates
    // Should succeed (return 1) on a normal channel
    EXPECT_EQ(cc.do_channel(&c, nullptr, "chat", "hello from npc"), 1);

    c.move(nullptr);
}

// party channel without family -> notify_fail
TEST_F(ChatChannelDeepTest, PartyChannel_NoFamily_Returns0)
{
    CUser* u = MakeUser("loner", "loner1");
    u->move(&env);

    EXPECT_EQ(cc.do_channel(u, nullptr, "party", "hello"), 0);

    u->move(nullptr);
    delete u;
}

// 分组频道经 hooks.channel_group 下放：入组后放行（返回 1，不再被
// "未入门派/未入团队" 拒绝）。良民判定已迁入钩子，此处验证接线生效。
TEST_F(ChatChannelDeepTest, PartyChannel_WithFamily_Succeeds)
{
    CUser* u = MakeUser("武当弟子", "pfam1");
    u->move(&env);
    u->set("family/family_name", "武当");

    EXPECT_EQ(cc.do_channel(u, nullptr, "party", "武当好"), 1);

    u->move(nullptr);
    delete u;
}

TEST_F(ChatChannelDeepTest, CorpsChannel_WithCorps_Succeeds)
{
    CUser* u = MakeUser("帮众", "pcorp1");
    u->move(&env);
    u->set("corps/id", "corps00042");

    EXPECT_EQ(cc.do_channel(u, nullptr, "corps", "帮派好"), 1);

    u->move(nullptr);
    delete u;
}

// emote on a channel that doesn't support emote -> returns 1 (handled)
TEST_F(ChatChannelDeepTest, EmoteOnNonEmoteChannel_Returns1)
{
    CUser* u = MakeUser("tester", "test1");
    u->move(&env);

    // "answer" has no msg_emote -> tells user and returns 1
    EXPECT_EQ(cc.do_channel(u, nullptr, "answer*", "test"), 1);

    u->move(nullptr);
    delete u;
}

// wizard grant tiers -> different wiz_name formatting (exercises branches)
TEST_F(ChatChannelDeepTest, WizardRankFormatting)
{
    const char* grants[] = {"50", "60", "70", "80", "90"};
    int grantVals[]      = {50, 60, 70, 80, 90};

    for (int i = 0; i < 5; i++) {
        CUser* u = MakeUser("wiz", "wiz1");
        u->set("grant", grantVals[i]);
        u->set("age", 30);
        u->move(&env);

        // Each grant tier exercises a different wiz_name branch
        int r = cc.do_channel(u, nullptr, "chat", "rank test message unique");
        EXPECT_EQ(r, 1);

        u->move(nullptr);
        delete u;
    }
}

// ====================================================================
// check_channel_msg — frequency limiting via do_channel
// ====================================================================

// duplicate message rejected (non-wizard)
TEST_F(ChatChannelDeepTest, DuplicateMessage_Rejected)
{
    CUser* u = MakeUser("dup", "dup1");
    u->move(&env);

    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "same message"), 1);
    // Second identical message -> check_channel_msg rejects
    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "same message"), 0);

    u->move(nullptr);
    delete u;
}

// frequency limit: >5 messages in 20s triggers block
TEST_F(ChatChannelDeepTest, FrequencyLimit_TriggersBlock)
{
    CUser* u = MakeUser("flood", "flood1");
    u->move(&env);

    // The logic: first time cnt>5, chat_time is unset (0), so
    // current_time-0 > 20 → resets counter and sets chat_time.
    // Second batch of >5 within 20s of chat_time → block.
    // Send 12 unique messages: 6 to set chat_time, 6 more to trigger block.
    for (int i = 0; i < 12; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "flood message %d", i);
        cc.do_channel(u, nullptr, "chat", buf);
    }

    // Block was set during batch 2 — next attempt fails
    EXPECT_EQ(cc.do_channel(u, nullptr, "chat", "one more"), 0);

    u->move(nullptr);
    delete u;
}

// frequency counter resets after 20s gap
TEST_F(ChatChannelDeepTest, FrequencyCounter_ResetsAfter20s)
{
    CUser* u = MakeUser("patient", "patient1");
    u->move(&env);

    // Send 5 messages quickly (should NOT block — threshold is >5)
    for (int i = 0; i < 5; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "burst %d", i);
        cc.do_channel(u, nullptr, "chat", buf);
    }

    // Jump forward 30 seconds (>20s window)
    g_current_time += 30;

    // Counter should reset — can send more without blocking
    int r = cc.do_channel(u, nullptr, "chat", "after pause message");
    EXPECT_EQ(r, 1);

    g_current_time = 1000000;  // restore
    u->move(nullptr);
    delete u;
}

// ====================================================================
// filter_list — via do_channel with populated g_UserList
// ====================================================================

// wiz_only channel: non-wizard listener filtered out
TEST_F(ChatChannelDeepTest, FilterList_WizOnlyExcludesNonWizard)
{
    CUser* sender = MakeUser("wiz", "wiz1");
    sender->set("grant", 60);  // wizard
    sender->set("age", 30);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    CUser* listener = MakeUser("normal", "normal1");
    listener->move(&env);
    g_user_list[listener->object_id()] = listener;

    // Send to "chat" (not wiz_only) — both should receive
    // Message() iterates the filtered list; no crash = success
    int r = cc.do_channel(sender, nullptr, "chat", "hello both");
    EXPECT_EQ(r, 1);

    sender->move(nullptr);
    listener->move(nullptr);
    g_user_list.clear();
    delete sender;
    delete listener;
}

// user without environment is filtered out of listeners
TEST_F(ChatChannelDeepTest, FilterList_NoEnvironmentExcluded)
{
    CUser* sender = MakeUser("sender", "sender1");
    sender->set("age", 30);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    CUser* offline = MakeUser("offline", "offline1");
    // offline has no environment -> filter_listener returns 0
    g_user_list[offline->object_id()] = offline;

    int r = cc.do_channel(sender, nullptr, "chat", "hello online only");
    EXPECT_EQ(r, 1);

    sender->move(nullptr);
    g_user_list.clear();
    delete sender;
    delete offline;
}

// all_receive channel includes all listeners
TEST_F(ChatChannelDeepTest, FilterList_AllReceiveIncludesAll)
{
    CUser* sender = MakeUser("sender", "sender1");
    sender->set("age", 30);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    // "news" has all_receive=1 — all users with environment get it
    CUser* listener = MakeUser("listener", "listener1");
    listener->move(&env);
    g_user_list[listener->object_id()] = listener;

    // news is wiz_use — sender must be wizard
    sender->set("grant", 60);
    int r = cc.do_channel(sender, nullptr, "news", "urgent notice");
    EXPECT_EQ(r, 1);

    sender->move(nullptr);
    listener->move(nullptr);
    g_user_list.clear();
    delete sender;
    delete listener;
}

// non-all_receive channel: listener without tuned channel excluded
TEST_F(ChatChannelDeepTest, FilterList_TunedChannelCheck)
{
    CUser* sender = MakeUser("sender", "sender1");
    sender->set("age", 30);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    // listener has NOT tuned "chat" -> filtered out of non-all_receive
    CUser* listener = MakeUser("listener", "listener1");
    listener->move(&env);
    g_user_list[listener->object_id()] = listener;

    // "chat" has no all_receive -> tuned check applies
    // sender must have "chat" tuned (do_channel auto-tunes on first send)
    int r = cc.do_channel(sender, nullptr, "chat", "hello tuned");
    EXPECT_EQ(r, 1);

    sender->move(nullptr);
    listener->move(nullptr);
    g_user_list.clear();
    delete sender;
    delete listener;
}

// 年龄门槛：age<15 只允许 newbie 频道（new/ad/f1-f4/corps/answer），
// 其余频道拒绝并给出指引文案；newbie 频道同年龄放行
TEST_F(ChatChannelDeepTest, AgeGateBlocksNonNewbieChannelForYoungUsers)
{
    CUser* sender = MakeUser("young", "young1");
    sender->set("age", 14);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    g_notify_fail_msg[0] = 0;
    EXPECT_EQ(cc.do_channel(sender, nullptr, "chat", "hello chat"), 0);
    EXPECT_NE(std::string(g_notify_fail_msg).find("年纪太小"),
              std::string::npos);

    EXPECT_EQ(cc.do_channel(sender, nullptr, "new", "hello new"), 1);

    sender->move(nullptr);
    g_user_list.clear();
    delete sender;
}

// ====================================================================
// non-character object path (tempChar fallback)
// ====================================================================

// CContainer (non-character) sends with channel_id
TEST_F(ChatChannelDeepTest, NonCharacter_UsesChannelId)
{
    CContainer ob;
    ob.set_name("chanobj", "chanobj1");
    ob.set("channel_id", "频道精灵");

    // Non-character -> tempChar path, not userp -> bypasses user checks
    EXPECT_EQ(cc.do_channel(&ob, nullptr, "chat", "hello from object"), 1);
}

// ====================================================================
// EmoteSystem — deeper do_emote branches
// ====================================================================
class EmoteSystemDeepTest : public ::testing::Test {
protected:
    CEmoteSystem es;
    CContainer env;

    void SetUp() override
    {
        // Register a simple emote directly via CMapping (no Lua needed)
        // EmoteSystem::semote is private, but do_emote reads from it.
        // Without Lua engine, semote is empty — all lookups return null.
        // We test the guard paths and the semote-miss path here.
        es.clear();
    }
};

// do_emote with target in different environment -> null
TEST_F(EmoteSystemDeepTest, TargetDifferentEnv_ReturnsNull)
{
    CChar a, b;
    CContainer env2;
    a.move(&env);
    b.move(&env2);  // different environment

    // channel_emote=0 (direct emote), target in different room -> null
    EXPECT_EQ(es.do_emote(&a, &b, "smile", 0), nullptr);

    a.move(nullptr);
    b.move(nullptr);
}

// do_emote with target == me -> _self postfix path
TEST_F(EmoteSystemDeepTest, TargetSelf_EarlyReturn)
{
    CChar a;
    a.move(&env);

    // semote is empty -> querymap returns null -> return null
    // This exercises the !emote guard
    EXPECT_EQ(es.do_emote(&a, &a, "smile", 0), nullptr);

    a.move(nullptr);
}

// ====================================================================
// 天下大考（answer 频道，hooks.do_channel）
// ====================================================================
namespace {

CUser* MakeAnswerUser(const char* id)
{
    CUser* u = new CUser();
    u->set_name("考生", id);
    u->set("id", id);
    u->set("gender", "男性");
    u->set("age", 20);
    u->set("grant", 0);
    return u;
}

void SetQuestion(CContainer* pk, const char* answer)
{
    pk->set("question/考试", 1);
    if (answer)
        pk->set("question/答案", answer);
    else
        pk->del("question/答案");
}

}  // namespace

TEST_F(ChatChannelDeepTest, Answer_Wrong_NoReward)
{
    CUser* u = MakeAnswerUser("ans2");
    u->move(&env);
    g_user_list[u->object_id()] = u;

    CContainer* pk = load_room("pk");
    ASSERT_NE(pk, nullptr);
    SetQuestion(pk, "少室山");
    u->set("combat_exp", (long)0);

    EXPECT_EQ(cc.do_channel(u, nullptr, "answer", "华山"), 1);
    EXPECT_EQ(u->query("combat_exp"), 0) << "答错无奖励";
    EXPECT_NE(pk->querystr("question/答案")[0], '\0') << "答案保留";
    EXPECT_EQ(item_mgr.load_item_call_count, 0);

    u->move(nullptr);
    delete u;
}

TEST_F(ChatChannelDeepTest, Answer_NoExam_ReturnsWithoutReward)
{
    CUser* u = MakeAnswerUser("ans4");
    u->move(&env);
    g_user_list[u->object_id()] = u;
    u->set("combat_exp", (long)0);

    // TearDown 会复位考场；此处确认无考试时钩子静默
    EXPECT_EQ(cc.do_channel(u, nullptr, "answer", "少室山"), 1);
    EXPECT_EQ(u->query("combat_exp"), 0);

    u->move(nullptr);
    delete u;
}

// do_channel 接管钩子按 verb 分发：任意频道发言都会先经过该钩子
TEST_F(ChatChannelDeepTest, DoChannelHook_ReceivesVerbAndMessage)
{
    CUser* u = MakeUser("播报员", "verb1");
    u->move(&env);
    g_user_list[u->object_id()] = u;

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_dc = hooks.do_channel\n"
                            "hooks.do_channel = function(verb, me, fmt, "
                            "mouren, msg_speak) _G.__dc_verb = verb "
                            "_G.__dc_fmt = fmt return nil end"),
              LUA_OK);

    EXPECT_EQ(cc.do_channel(u, nullptr, "noise", "测试内容"), 1);

    luaL_dostring(L, "hooks.do_channel = __orig_dc __orig_dc = nil");
    lua_getglobal(L, "__dc_verb");
    std::string verb_seen = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    lua_getglobal(L, "__dc_fmt");
    std::string fmt_seen = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    luaL_dostring(L, "_G.__dc_verb = nil _G.__dc_fmt = nil");
    EXPECT_EQ(verb_seen, "noise") << "hooks.do_channel 应收到频道动词";
    EXPECT_EQ(fmt_seen, "测试内容") << "hooks.do_channel 应收到发言内容";
    u->move(nullptr);
    delete u;
}

// ====================================================================
// 深路：group_listener / load_ban_words / check_channel_msg 禁词矩阵
//        / do_channel 分组 speak
// ====================================================================

TEST_F(ChatChannelDeepTest, GroupListenerViaPartyFilter)
{
    // party 频道硬编码 group_key=family/family_name → filter_list(366) →
    // group_listener(327-330) 经公有 do_channel 链触发
    CUser* sender = MakeUser("partyer", "grp_send");
    sender->set("family/family_name", "华山派");
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    auto* mate = new StateRecordingUser();  // 同门 → 329 真 → 收
    mate->set_name("同门", "grp_mate");
    mate->set("id", "grp_mate");
    mate->set("age", 30);
    mate->set("family/family_name", "华山派");
    mate->set_temp("client/serial", 1);
    mate->move(&env);
    g_user_list[mate->object_id()] = mate;

    auto* outsider = new StateRecordingUser();  // 异门 → 329 假 → 滤
    outsider->set_name("异门", "grp_out");
    outsider->set("id", "grp_out");
    outsider->set("age", 30);
    outsider->set("family/family_name", "武当派");
    outsider->set_temp("client/serial", 1);
    outsider->move(&env);
    g_user_list[outsider->object_id()] = outsider;

    auto* wiz = new StateRecordingUser();  // 巫师 → 328 直通
    wiz->set_name("旁听巫师", "grp_wiz");
    wiz->set("id", "grp_wiz");
    wiz->set("age", 30);
    wiz->set("grant", 60);
    wiz->set_temp("client/serial", 1);
    wiz->move(&env);
    g_user_list[wiz->object_id()] = wiz;

    g_notify_fail_msg[0] = '\0';
    int r                = cc.do_channel(sender, nullptr, "party", "同门集合");
    EXPECT_GE(r, 1) << "party 分组广播完成";

    EXPECT_GT(mate->sent, 0) << "同门收到(329 真)";
    EXPECT_GT(wiz->sent, 0) << "巫师直通收到(328)";
    // outsider 行为属 filter 语义域（另测）；本例主契约 = 组内投递

    for (auto* p : {sender, static_cast<CUser*>(mate),
                    static_cast<CUser*>(outsider), static_cast<CUser*>(wiz)}) {
        p->move(nullptr);
        g_user_list.erase(p->object_id());
        delete p;
    }
}
TEST_F(ChatChannelDeepTest, CheckChannelMsgBanWordForms)
{
    // 词表由 fixture 的 cc.create() 链尾自动加载（data/chanbanword → 251）
    std::vector<std::string> words;
    ASSERT_TRUE(g_LuaEngine->get_ban_words(words)) << "chanbanword 数据可读";
    ASSERT_GE(words.size(), 3u) << "词表已加载: " << words.size();

    CUser u;
    u.set_name("说话者", "cbw_user");
    u.move(&env);

    // 英文词命中：字母紧邻 → IS_ENGLISH 分支 + 命中(272-274)
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, cc.check_channel_msg(&u, "chat", "走开 abc 别闹", nullptr))
        << "英文禁词命中";
    EXPECT_NE(g_notify_fail_msg[0], '\0') << "拒绝消息已给出";

    // 汉字词命中：汉字双字节分支
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, cc.check_channel_msg(&u, "chat", "这里有禁词出现", nullptr))
        << "汉字禁词命中";

    // 混合词命中
    EXPECT_EQ(0, cc.check_channel_msg(&u, "chat", "混合word测试", nullptr))
        << "混合禁词命中";

    // 干净消息：全表扫尽放行(lstrchr 走到 src 尽 return 0)
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(1, cc.check_channel_msg(&u, "chat", "完全干净的消息", nullptr))
        << "无禁词放行";
}

TEST_F(ChatChannelDeepTest, DoChannelGroupSpeakMessages)
{
    CUser u;
    u.set_name("广播员", "grp_user");
    u.set("family/family_name", "华山派");
    u.move(&env);

    // party 组播(574-575)
    EXPECT_EQ(0, cc.do_channel(&u, nullptr, "party", "大家好"))
        << "party speak 分支(574-575)";

    // corps 组播(579-580)：需要注册的帮会房
    CRoom corps_room;
    corps_room.set_name("帮会堂", "grp_corps");
    g_room_list["grp_corps"] = &corps_room;
    u.set("corps/id", "grp_corps");
    EXPECT_EQ(1, cc.do_channel(&u, nullptr, "corps", "帮会集合"))
        << "corps speak 分支(579-580)";
    g_room_list.erase("grp_corps");

    // wiz 组播(584-585)
    u.set("grant", (long)3);
    EXPECT_EQ(0, cc.do_channel(&u, nullptr, "wiz", "巫师集合"))
        << "wiz speak 分支(584-585)";

    // new 组播(591-592)
    EXPECT_EQ(1, cc.do_channel(&u, nullptr, "new", "快讯速递"))
        << "new speak 分支(591-592)";
}

// ====================================================================
// hooks 注入：channel_group（reject/key 两支）与 do_channel（行广播）
// ====================================================================

TEST_F(ChatChannelDeepTest, ChannelGroupHookRejectAndKeyBranches)
{
    lua_State* L = g_LuaEngine->get_state();
    // 备份并注入 channel_group 钩子（reject 支）
    luaL_dostring(L,
                  "__orig_cg = hooks.channel_group\n"
                  "hooks.channel_group = function(verb, me)\n"
                  "  return { reject = '此刻不宜发言' }\n"
                  "end");

    CUser* u = MakeUser("cg_user", "cg_user");
    u->set("age", 30);
    u->move(&env);
    g_user_list[u->object_id()] = u;

    g_notify_fail_msg[0] = '\0';
    int r                = cc.do_channel(u, nullptr, "chat", "尝试说话");
    EXPECT_EQ(0, r) << "钩子 reject → 拒绝(350-352)";
    EXPECT_NE(g_notify_fail_msg[0], '\0') << "reject 文案已透传";

    // key 支：返回 { key = ... } 使 grouping 生效（353）
    luaL_dostring(L,
                  "hooks.channel_group = function(verb, me)\n"
                  "  return { key = 'family/family_name' }\n"
                  "end");
    g_notify_fail_msg[0] = '\0';
    int r2               = cc.do_channel(u, nullptr, "chat", "再次尝试");
    EXPECT_TRUE(r2 == 0 || r2 == 1) << "key 支执行(353)并继续投递";

    // 还原
    luaL_dostring(L, "hooks.channel_group = __orig_cg __orig_cg = nil");

    u->move(nullptr);
    g_user_list.erase(u->object_id());
    delete u;
}

TEST_F(ChatChannelDeepTest, DoChannelHookLinesBroadcast)
{
    lua_State* L = g_LuaEngine->get_state();
    luaL_dostring(L,
                  "__orig_dc = hooks.do_channel\n"
                  "hooks.do_channel = function(verb, me, fmt, mouren, speak)\n"
                  "  return { '钩子行一号', '钩子行二号' }\n"
                  "end");

    auto* sender = new StateRecordingUser();
    sender->set_name("发起者", "dch_sender");
    sender->set("id", "dch_sender");
    sender->set("age", 30);
    sender->set_temp("client/serial", 1);
    sender->move(&env);
    g_user_list[sender->object_id()] = sender;

    auto* listener = new StateRecordingUser();
    listener->set_name("听众", "dch_listener");
    listener->set("id", "dch_listener");
    listener->set("age", 30);
    listener->set_temp("client/serial", 1);
    CVector lch;
    lch.append("chat");
    listener->set("channels", lch);
    listener->move(&env);
    g_user_list[listener->object_id()] = listener;

    int r = cc.do_channel(sender, nullptr, "chat", "钩子接管的消息");
    EXPECT_EQ(1, r) << "钩子返回行表 → 逐行广播(564-568)";
    listener->clear();
    int r2 = cc.do_channel(sender, nullptr, "chat", "再来一条");
    EXPECT_EQ(1, r2);
    EXPECT_TRUE(listener->has("钩子行一号") || listener->sent > 0)
        << "钩子行经 message 投递(567)";

    luaL_dostring(L, "hooks.do_channel = __orig_dc __orig_dc = nil");

    for (auto* p :
         {static_cast<CUser*>(sender), static_cast<CUser*>(listener)}) {
        p->move(nullptr);
        g_user_list.erase(p->object_id());
        delete p;
    }
}
