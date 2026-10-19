// test_channel_subsystems.cpp — direct unit tests for the 6 subsystem
// classes extracted from CChannel during the Facade refactoring.
//
// Covers: BanlistManager, VotingSystem, SnoopSystem, EmoteSystem,
//         ChatChannel, and CChannel facade reference aliases.
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "test_state_recorder.h"  // 旁听回执观察面

#include "Channel.h"
#include "Char.h"
#include "Container.h"

// ====================================================================
// BanlistManager — pure data container
// ====================================================================
TEST(CBanlistManager, DefaultEmpty)
{
    CBanlistManager bm;
    EXPECT_TRUE(bm.banlist.undefinedp("1.2.3.4"));
    EXPECT_TRUE(bm.loginlist.undefinedp("1.2.3.4"));
    EXPECT_TRUE(bm.loginlimit.undefinedp("1.2.3.4"));
}

TEST(CBanlistManager, SetQueryRoundTrip)
{
    CBanlistManager bm;
    bm.banlist.set("1.2.3.4", 1L);
    EXPECT_EQ(bm.banlist.query("1.2.3.4"), 1L);
    EXPECT_FALSE(bm.banlist.undefinedp("1.2.3.4"));

    bm.loginlist.set("5.6.7.8", 3L);
    EXPECT_EQ(bm.loginlist.query("5.6.7.8"), 3L);

    bm.loginlimit.set("5.6.7.8", 10L);
    EXPECT_EQ(bm.loginlimit.query("5.6.7.8"), 10L);
}

TEST(CBanlistManager, ClearEmptiesAll)
{
    CBanlistManager bm;
    bm.banlist.set("1.2.3.4", 1L);
    bm.loginlist.set("5.6.7.8", 3L);
    bm.loginlimit.set("5.6.7.8", 10L);

    bm.clear();
    EXPECT_TRUE(bm.banlist.undefinedp("1.2.3.4"));
    EXPECT_TRUE(bm.loginlist.undefinedp("5.6.7.8"));
    EXPECT_TRUE(bm.loginlimit.undefinedp("5.6.7.8"));
}

TEST(CBanlistManager, DelRemovesEntry)
{
    CBanlistManager bm;
    bm.banlist.set("1.2.3.4", 1L);
    bm.banlist.del("1.2.3.4");
    EXPECT_TRUE(bm.banlist.undefinedp("1.2.3.4"));
}

// ====================================================================
// VotingSystem — pure data container
// ====================================================================
TEST(CVotingSystem, DefaultEmpty)
{
    CVotingSystem vs;
    EXPECT_TRUE(vs.voted_players.undefinedp("option1"));
    EXPECT_TRUE(vs.votes_casted.undefinedp("player1"));
}

TEST(CVotingSystem, SetQueryRoundTrip)
{
    CVotingSystem vs;
    vs.voted_players.set("option1", 5L);
    EXPECT_EQ(vs.voted_players.query("option1"), 5L);

    vs.votes_casted.set("player1", 1L);
    EXPECT_EQ(vs.votes_casted.query("player1"), 1L);
}

TEST(CVotingSystem, ClearEmptiesBoth)
{
    CVotingSystem vs;
    vs.voted_players.set("option1", 5L);
    vs.votes_casted.set("player1", 1L);

    vs.clear();
    EXPECT_TRUE(vs.voted_players.undefinedp("option1"));
    EXPECT_TRUE(vs.votes_casted.undefinedp("player1"));
}

// ====================================================================
// SnoopSystem — direct tests (complement ChannelSnoopList facade tests)
// ====================================================================
TEST(SnoopSystemDirect, SnoopThenQuery)
{
    CSnoopSystem sys;
    CChar a, b;
    sys.snoop(&a, &b);
    EXPECT_EQ(sys.query_snooping(&a), &b);
}

TEST(SnoopSystemDirect, ReSnoopReplaces)
{
    CSnoopSystem sys;
    CChar a, b, c;
    sys.snoop(&a, &b);
    sys.snoop(&a, &c);
    EXPECT_EQ(sys.query_snooping(&a), &c);
}

TEST(SnoopSystemDirect, RemoveByListener)
{
    CSnoopSystem sys;
    CChar a, b;
    sys.snoop(&a, &b);
    sys.remove_snoop(&a);
    EXPECT_EQ(sys.query_snooping(&a), nullptr);
}

TEST(SnoopSystemDirect, RemoveByVictim)
{
    CSnoopSystem sys;
    CChar a, b;
    sys.snoop(&a, &b);
    sys.remove_snoop(&b);
    EXPECT_EQ(sys.query_snooping(&a), nullptr);
}

TEST(SnoopSystemDirect, ReleaseMeClearsAll)
{
    CSnoopSystem sys;
    CChar a, b, c;
    sys.snoop(&a, &b);
    sys.snoop(&c, &a);
    sys.release_me();
    EXPECT_EQ(sys.query_snooping(&a), nullptr);
    EXPECT_EQ(sys.query_snooping(&c), nullptr);
}

TEST(SnoopSystemDirect, UntrackedReturnsNull)
{
    CSnoopSystem sys;
    CChar x;
    EXPECT_EQ(sys.query_snooping(&x), nullptr);
}

TEST(SnoopSystemDirect, NullVictimRemovesExisting)
{
    CSnoopSystem sys;
    CChar a, b;
    sys.snoop(&a, &b);
    EXPECT_EQ(sys.query_snooping(&a), &b);
    sys.snoop(&a, nullptr);  // victim=NULL -> remove only
    EXPECT_EQ(sys.query_snooping(&a), nullptr);
}

// ====================================================================
// EmoteSystem — early-return guards
// ====================================================================
TEST(EmoteSystemDirect, NullMeReturnsNull)
{
    CEmoteSystem es;
    CChar a;
    // me->environment() == nullptr -> early return
    EXPECT_EQ(es.do_emote(&a, nullptr, "smile", 0), nullptr);
}

TEST(EmoteSystemDirect, EmptyVerbReturnsNull)
{
    CEmoteSystem es;
    CChar a;
    CContainer env;
    a.move(&env);
    EXPECT_EQ(es.do_emote(&a, nullptr, "", 0), nullptr);
    a.move(nullptr);
}

TEST(EmoteSystemDirect, UnknownVerbReturnsNull)
{
    CEmoteSystem es;
    CChar a;
    CContainer env;
    a.move(&env);
    EXPECT_EQ(es.do_emote(&a, nullptr, "notanemote_zzz", 0), nullptr);
    a.move(nullptr);
}

TEST(EmoteSystemDirect, ClearEmptiesMapping)
{
    CEmoteSystem es;
    es.clear();  // should not crash on empty
    CChar a;
    CContainer env;
    a.move(&env);
    // 契约：clear 后映射为空 → 查不到动词返回 nullptr
    EXPECT_EQ(es.do_emote(&a, nullptr, "smile", 0), nullptr);
    es.clear();  // 二次 clear 幂等
    a.move(nullptr);
}

// ====================================================================
// ChatChannel — do_channel guard paths
// ====================================================================
TEST(ChatChannelDirect, UnknownChannelReturnsZero)
{
    CChatChannel cc;
    CChar me;
    CContainer env;
    me.set_name("tester", "test1");
    me.move(&env);
    // Unknown verb -> channel not found -> return 0
    EXPECT_EQ(cc.do_channel(&me, nullptr, "nonexistent_ch_zzz", "hello"), 0);
    me.move(nullptr);
}

TEST(ChatChannelDirect, ShortVerbReturnsZero)
{
    CChatChannel cc;
    CChar me;
    CContainer env;
    me.set_name("tester", "test1");
    me.move(&env);
    // len < 0 after strlen-1 -> return 0
    EXPECT_EQ(cc.do_channel(&me, nullptr, "", "hello"), 0);
    me.move(nullptr);
}

TEST(ChatChannelDirect, ClearEmptiesChannels)
{
    CChatChannel cc;
    cc.clear();
    // After clear, unknown channel still returns 0
    CChar me;
    CContainer env;
    me.set_name("tester", "test1");
    me.move(&env);
    EXPECT_EQ(cc.do_channel(&me, nullptr, "chat", "hello"), 0);
    me.move(nullptr);
}

// ====================================================================
// CChannel facade — reference aliases & delegation
// ====================================================================
TEST(ChannelFacade, BanlistAliasPointsToSubsystem)
{
    CChannel ch;
    // Writing through the facade alias must be visible via the accessor
    ch.banlist.set("9.9.9.9", 1L);
    EXPECT_EQ(ch.banlist_mgr().banlist.query("9.9.9.9"), 1L);

    // And vice-versa
    ch.banlist_mgr().banlist.set("8.8.8.8", 1L);
    EXPECT_EQ(ch.banlist.query("8.8.8.8"), 1L);
}

TEST(ChannelFacade, VotingAliasPointsToSubsystem)
{
    CChannel ch;
    ch.voted_players.set("opt1", 42L);
    EXPECT_EQ(ch.voting().voted_players.query("opt1"), 42L);

    ch.voting().voted_players.set("opt2", 7L);
    EXPECT_EQ(ch.votes_casted.query("opt2"), 0L);  // different mapping
    EXPECT_EQ(ch.voting().voted_players.query("opt2"), 7L);
}

TEST(ChannelFacade, LoginAliasPointsToSubsystem)
{
    CChannel ch;
    ch.loginlist.set("1.1.1.1", 5L);
    EXPECT_EQ(ch.banlist_mgr().loginlist.query("1.1.1.1"), 5L);

    ch.loginlimit.set("1.1.1.1", 10L);
    EXPECT_EQ(ch.banlist_mgr().loginlimit.query("1.1.1.1"), 10L);
}

TEST(ChannelFacade, SnoopDelegation)
{
    CChannel ch;
    CChar a, b;
    ch.snoop(&a, &b);
    EXPECT_EQ(ch.query_snooping(&a), &b);
    ch.remove_snoop(&a);
    EXPECT_EQ(ch.query_snooping(&a), nullptr);
}

TEST(ChannelFacade, SubsystemAccessorsNonNull)
{
    CChannel ch;
    EXPECT_NE(&ch.chat(), nullptr);
    EXPECT_NE(&ch.emote(), nullptr);
    EXPECT_NE(&ch.snoop_sys(), nullptr);
    EXPECT_NE(&ch.banlist_mgr(), nullptr);
    EXPECT_NE(&ch.voting(), nullptr);
    EXPECT_NE(&ch.relationship(), nullptr);
}

// ====================================================================
// SnoopSystem：旁听投递 / 重入锁 / 非匹配抹除（do_snooping + remove 支）
// ====================================================================
TEST(SnoopSystemDirect, DoSnoopingRelaysToListener)
{
    CSnoopSystem sys;
    StateRecordingUser listener;
    listener.set_name("旁听人", "sn_listener");
    CChar victim;
    victim.set_name("被听人", "sn_victim");
    sys.snoop(&listener, &victim);

    sys.do_snooping(&victim, (char*)"密语内容");

    EXPECT_TRUE(listener.has("密语内容")) << "旁听回执含原文(54-56)";
    EXPECT_EQ(0, listener.query_temp("snoop_lock")) << "重入锁用后即清(55,57)";
}

TEST(SnoopSystemDirect, DoSnoopingLockedVictimSkipsRelay)
{
    CSnoopSystem sys;
    StateRecordingUser listener;
    listener.set_name("旁听人", "sn_listener2");
    CChar victim;
    victim.set_name("被听人", "sn_victim2");
    victim.set_temp("snoop_lock", 1);  // 重入中
    sys.snoop(&listener, &victim);

    sys.do_snooping(&victim, (char*)"不该送达");

    EXPECT_FALSE(listener.has("不该送达")) << "重入锁早退(49)";
    sys.remove_snoop(&listener);
}

TEST(SnoopSystemDirect, RemoveSnoopNonMatchKeepsRegistry)
{
    CSnoopSystem sys;
    CChar a, b, stranger;
    sys.snoop(&a, &b);
    sys.remove_snoop(&stranger);  // 非监听亦非被听 → 走 ++it 支(73)
    EXPECT_EQ(sys.query_snooping(&a), &b) << "登记表原封(73)";
}
