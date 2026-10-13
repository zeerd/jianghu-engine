// test_lua_npc.cpp — CLuaNpcT：脚本驱动的 NPC 虚函数派发
//
// 契约：
//  - create/setup 把脚本字段写进对象；未定义脚本函数时回退基类实现
//  - do_talk 优先交给脚本，脚本未定义时走 set_inquiry 注册的询问逻辑
//    （命中即回答，未命中列菜单）
//  - 战斗/社交/生死回调（die、finish_kill、is_defeated、hit_ob、greeting、
//    师徒、relay_*）逐个派发到脚本，脚本返回什么就是什么
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>

#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "Item.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "lua/LuaNpc.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"
#include "mock/mock_itemmgr.h"

extern char g_notify_fail_msg[512];

namespace {

class LuaNpcScriptTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom room;
    CChar* me = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr  = g_item_mgr;
        g_item_mgr = &item_mgr;
        room.set("name", "lua_npc_room");
        room.set("infinity", 1);

        me = new CChar();
        me->set_name("访客", "lua_npc_visitor");
        me->set("hp", (long)100);
        me->move(&room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        delete me;
        g_item_mgr = saved_mgr;
    }

    // 由 NPC 管理器实例化脚本 NPC（create + setup 都会跑到）
    CNpc* Spawn(const char* script)
    {
        CNpc* npc = load_npc(script);
        if (npc) npc->move(&room);
        return npc;
    }

    static void Free(CNpc* npc)
    {
        g_npc_list.erase(npc->object_id());
        npc->set_heart_beat(0);
        npc->move(nullptr);
        delete npc;
    }

    static CLuaNpc* AsScriptNpc(CNpc* npc)
    {
        return dynamic_cast<CLuaNpc*>(npc);
    }
};

// ---------------- create / setup ----------------
TEST_F(LuaNpcScriptTest, CreateAndSetup_RunScriptHooks)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    EXPECT_STREQ(npc->name(), "夹具伙计");
    EXPECT_STREQ(npc->querystr("id"), "fixture npc");
    EXPECT_EQ(npc->query("hp"), 100);
    EXPECT_EQ(npc->query("setup_done"), 1) << "setup() 必须派发到脚本";

    Free(npc);
}

// ---------------- do_talk 与询问 ----------------
TEST_F(LuaNpcScriptTest, DoTalk_DispatchesToScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    char ask[32] = "天气如何";
    EXPECT_EQ(npc->do_talk(me, ask), 1);
    EXPECT_STREQ(npc->querystr("last_talk"), "天气如何");

    Free(npc);
}

TEST_F(LuaNpcScriptTest, Inquiry_PresetAnswerReachesVisitor)
{
    CNpc* npc = Spawn("t_quiet_npc");
    ASSERT_NE(npc, nullptr);
    CLuaNpc* script_npc = AsScriptNpc(npc);
    ASSERT_NE(script_npc, nullptr);
    script_npc->set_inquiry("茶钱", "一两银子。", "茶钱");

    StateRecordingUser visitor;
    visitor.set_name("客官", "lua_npc_guest");
    visitor.move(&room);
    visitor.delete_temp("client/loading");
    visitor.clear();

    char ask[32] = "茶钱";
    EXPECT_EQ(npc->do_talk(&visitor, ask), 1);
    EXPECT_TRUE(visitor.has("一两银子"))
        << "预置答案必须答给访客" << visitor.msgs.size();

    visitor.move(nullptr);
    Free(npc);
}

TEST_F(LuaNpcScriptTest, Inquiry_FuncCallbackAnswers)
{
    CNpc* npc = Spawn("t_quiet_npc");
    ASSERT_NE(npc, nullptr);
    CLuaNpc* script_npc = AsScriptNpc(npc);
    ASSERT_NE(script_npc, nullptr);
    script_npc->set_inquiry_func("身世", "answer_origin", "身世");

    StateRecordingUser visitor;
    visitor.set_name("客官", "lua_npc_guest2");
    visitor.move(&room);
    visitor.delete_temp("client/loading");
    visitor.clear();

    char ask[32] = "身世";
    EXPECT_EQ(npc->do_talk(&visitor, ask), 1);
    EXPECT_TRUE(visitor.has("后山一块石头"))
        << "回调返回值必须答给访客" << visitor.msgs.size();

    visitor.move(nullptr);
    Free(npc);
}

TEST_F(LuaNpcScriptTest, Inquiry_UnknownTopicStillHandled)
{
    CNpc* npc = Spawn("t_quiet_npc");
    ASSERT_NE(npc, nullptr);
    CLuaNpc* script_npc = AsScriptNpc(npc);
    ASSERT_NE(script_npc, nullptr);
    script_npc->set_inquiry("茶钱", "一两银子。", "茶钱");

    // 未登记的询问：列菜单返回 1；一个询问都没有时才拒绝
    char ask[32] = "别的问题";
    EXPECT_EQ(npc->do_talk(me, ask), 1);

    Free(npc);
}

TEST_F(LuaNpcScriptTest, Inquiry_EmptyRegistryRefuses)
{
    CNpc* npc = Spawn("t_quiet_npc");
    ASSERT_NE(npc, nullptr);

    char ask[32]         = "随便问点什么";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(npc->do_talk(me, ask), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。");

    Free(npc);
}

// ---------------- 行为/物品/生命周期回调 ----------------
TEST_F(LuaNpcScriptTest, HandleAction_RunsScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    char comm[32] = "bow";
    char arg[32]  = "";
    char tar[32]  = "";
    EXPECT_EQ(npc->handle_action(comm, me, arg, tar), 0);
    EXPECT_STREQ(npc->querystr("last_action"), "bow");

    Free(npc);
}

TEST_F(LuaNpcScriptTest, AcceptObject_RunsScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    CItem* gift = new CItem();
    gift->set_name("供品", "lua_npc_gift");
    gift->move(&room);

    EXPECT_EQ(npc->accept_object(me, gift), 1);
    EXPECT_EQ(npc->query("last_accepted"), 1);

    gift->move(nullptr);
    delete gift;
    Free(npc);
}

TEST_F(LuaNpcScriptTest, Init_RunsScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    npc->init(me);
    EXPECT_EQ(npc->query("init_done"), 1);

    Free(npc);
}

TEST_F(LuaNpcScriptTest, Die_RunsScriptWithoutDestroyingNpc)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);
    const int32_t id = npc->object_id();

    npc->die();

    EXPECT_EQ(npc->query("last_died"), 1) << "死亡逻辑交给脚本";
    EXPECT_NE(g_last_destruct_object, id) << "脚本没调 die_base 就不该自毁";

    Free(npc);
}

TEST_F(LuaNpcScriptTest, FinishKillAndIsDefeated_RunScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    CChar* player = new CChar();
    player->set_name("少侠", "lua_npc_fighter");
    player->move(&room);

    npc->finish_kill(player);
    EXPECT_EQ(npc->query("last_finish_kill"), 1);

    npc->is_defeated(player, me);
    EXPECT_EQ(npc->query("last_defeated"), 1);

    player->move(nullptr);
    delete player;
    Free(npc);
}

// ---------------- 观察/战斗播报 ----------------
TEST_F(LuaNpcScriptTest, DoLookAndHitOb_RunScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    EXPECT_EQ(npc->do_look(me), 1);
    EXPECT_EQ(npc->query("last_look"), 1);

    const char* wound = npc->hit_ob(me, me, 7);
    EXPECT_STREQ(wound, "你被拍了一下。") << "脚本返回的伤讯必须原样带回";
    EXPECT_EQ(npc->query("last_hit_bonus"), 7);

    Free(npc);
}

TEST_F(LuaNpcScriptTest, ChatMsgAndGreeting_ReturnScriptText)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    EXPECT_STREQ(npc->chat_msg(), "夹具伙计嘀咕了一句。");
    EXPECT_STREQ(npc->chat_msg_combat(), "夹具伙计抡起了扫帚。");
    EXPECT_STREQ(AsScriptNpc(npc)->greeting(me), "客官里边儿请。");
    EXPECT_EQ(npc->query("last_chat"), 1);
    EXPECT_EQ(npc->query("last_greeting"), 1);

    Free(npc);
}

// ---------------- 师徒/战斗意愿/转发 ----------------
TEST_F(LuaNpcScriptTest, ApprenticeHooks_RunScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    npc->attempt_apprentice(me);
    EXPECT_EQ(npc->query("last_attempt_apprentice"), 1);

    char skill[32] = "unarmed";
    EXPECT_EQ(npc->prevent_learn(me, skill), 0);
    EXPECT_STREQ(npc->querystr("last_prevent_learn"), "unarmed");

    EXPECT_EQ(npc->recognize_apprentice(me), 1);
    EXPECT_EQ(npc->query("last_recognize"), 1);

    Free(npc);
}

TEST_F(LuaNpcScriptTest, FightHooks_RunScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    EXPECT_EQ(npc->accept_fight(me), 1);
    EXPECT_EQ(npc->query("last_accept_fight"), 1);

    npc->attack(me);
    EXPECT_EQ(npc->query("last_attack"), 1);

    Free(npc);
}

TEST_F(LuaNpcScriptTest, RelaySayAndEmote_RunScript)
{
    CNpc* npc = Spawn("t_fixture_npc");
    ASSERT_NE(npc, nullptr);

    char msg[32] = "掌柜的，来一壶酒";
    npc->relay_say(me, msg);
    EXPECT_STREQ(npc->querystr("last_relay_say"), "掌柜的，来一壶酒");

    npc->relay_emote(me, "nod");
    EXPECT_STREQ(npc->querystr("last_relay_emote"), "nod");

    Free(npc);
}

}  // namespace

// ---------------- 脚本缺字段 → Path B 回退基类(590-591,629-630)
// ----------------
TEST_F(LuaNpcScriptTest, QuietNpcScriptMissingMethodsFallBack)
{
    // t_quiet_npc 已载脚本但无 greeting/chat_msg_combat 字段
    CNpc* npc = Spawn("t_quiet_npc");
    ASSERT_NE(npc, nullptr);
    CLuaNpc* script_npc = AsScriptNpc(npc);
    ASSERT_NE(script_npc, nullptr);

    EXPECT_STREQ("", script_npc->greeting(me))
        << "缺 greeting → 基类空串(590-591)";
    EXPECT_EQ(nullptr, npc->chat_msg_combat())
        << "缺 chat_msg_combat → 基类 NULL(629-630)";

    Free(npc);
}
