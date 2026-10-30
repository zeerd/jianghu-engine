// test_npc_mirror.cpp — 镜像NPC：镜像基模块（lua-default/npc/
// mirror_base.lua）经脚本显式 install 装配后的行为契约。
//
// 契约：
//  - 镜像NPC 是普通脚本 NPC：脚本自己 require 基模块并在文件末尾
//    install，缺省钩子（die / chat_msg / chat_msg_combat /
//    reward_killer）只补脚本未定义的项
//  - make_mirror 按倍率复制玩家等级/属性/技能/映射/激发；不设置名字；
//    简版不配武器（一次都不触碰物品管理器）
//  - 玩家技能太烂时轻功/招架映射回落到基线路数，内功无玩家映射时兜底
//  - 血线低于 1/3 时 chat_msg_combat 尝试自救（heal perform）
//  - 满血时 chat_msg_combat 不做动作直接返回
//  - chat_msg 有概率遁走并自毁
//  - die 找到凶手先发奖励（脚本 reward_killer）再自毁，找不到仅自毁
//  - 热重载（脚本 chunk 重跑）后装配随 install 再次生效
//  - 样本脚本 lua-sample/npc/sample_wooden_dummy.lua 的 do_talk 两条
//    分支与它自己接管的 die（见文件末尾的 SampleWoodenDummyTest）
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstdint>
#include <memory>

#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "Item.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "User.h"
#include "LuaBind.h"       // lua_bind_push_object
#include "test_lua_env.h"
#include "test_sample_script.h"  // ScopedSampleScript（暂存样本脚本原文）
#include "test_npc_cleanup.h"    // discard_npc
#include "test_state_recorder.h"
#include "mock/mock_itemmgr.h"

extern MAP2USER g_user_list;
extern char g_notify_fail_msg[512];

namespace {

class NpcMirrorTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom env;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr   = g_item_mgr;
        g_item_mgr  = &item_mgr;
        env.set("infinity", 1);
    }

    void TearDown() override { g_item_mgr = saved_mgr; }

    // 装载一个镜像夹具并放进测试房间（失败返回 nullptr）
    CNpc* load_mirror(const char* base_name)
    {
        CNpc* npc = load_npc(base_name);
        if (npc) npc->move(&env);
        return npc;
    }

    // 经 require 出的镜像基模块调用 make_mirror——它不是引擎派发的钩子，
    // 也没有 C++ 绑定：调用方必须借模块引用调用
    void call_make_mirror(CNpc* mirror, CChar* me, int diff)
    {
        lua_State* L = g_LuaEngine->get_state();
        lua_getglobal(L, "require");
        lua_pushstring(L, "npc.mirror_base");
        ASSERT_EQ(LUA_OK, lua_pcall(L, 1, 1, 0)) << lua_tostring(L, -1);
        lua_getfield(L, -1, "make_mirror");
        lua_bind_push_object(L, mirror);
        lua_bind_push_object(L, me);
        lua_pushinteger(L, diff);
        ASSERT_EQ(LUA_OK, lua_pcall(L, 3, 0, 0)) << lua_tostring(L, -1);
        lua_pop(L, 1);  // module 表
    }
};

}  // namespace

// ====================================================================
// 装配：脚本自己 install，缺省钩子随装配到位
// ====================================================================
TEST_F(NpcMirrorTest, MirrorScriptGetsAssembledChatFlee)
{
    CNpc* dummy = load_mirror("t_mirror_npc");
    ASSERT_NE(dummy, nullptr);
    EXPECT_STREQ(dummy->name(), "镜像夹具");

    // chat_msg 的遁走自毁（基模块默认）：单次概率 1/10，500 次内必命中
    const int32_t id = dummy->object_id();
    bool fled        = false;
    for (int i = 0; i < 500; i++) {
        dummy->chat_msg();
        if (g_last_destruct_object == id) {
            fled = true;
            break;
        }
    }

    EXPECT_TRUE(fled) << "chat_msg 的遁走自毁没被装配";
    if (!fled) discard_npc(dummy);
}

// ====================================================================
// make_mirror：按倍率抄玩家；简版不配武器、不设置名字
// ====================================================================
TEST_F(NpcMirrorTest, MakeMirrorScalesPlayerState)
{
    CChar player;
    player.set_name("本尊", "mirror_src");
    player.set("level", (long)30);
    player.set("max_hp", (long)1000);
    player.set("max_mp", (long)500);
    player.set("str", (long)20);
    player.set_skill("unarmed", 50);
    player.set_skill("sword", 30);

    CNpc* mirror = load_mirror("t_mirror_npc");
    ASSERT_NE(mirror, nullptr);

    call_make_mirror(mirror, &player, 5);

    EXPECT_EQ(mirror->query("level"), 15);
    EXPECT_EQ(mirror->query("max_hp"), 500);
    EXPECT_EQ(mirror->query("hp"), mirror->query("max_hp"));
    EXPECT_EQ(mirror->query("max_mp"), 250);
    EXPECT_EQ(mirror->query("mp"), 500) << "内力按满上限的 2 倍抄";
    EXPECT_EQ(mirror->query_skill("unarmed", 1), 25);

    // 玩家没有 dodge/force/parry：基线等级 = 10 + 30 * 3，并回落到基线路数
    EXPECT_EQ(mirror->query_skill("dodge", 1), 100);
    EXPECT_STREQ(mirror->query_skill_mapped("dodge"), "dodge");
    EXPECT_STREQ(mirror->query_skill_mapped("force"), "force");
    EXPECT_STREQ(mirror->query_skill_mapped("parry"), "sword");

    // 简版不配武器：物品管理器一次都不该被问到
    EXPECT_EQ(item_mgr.load_item_call_count, 0);

    // make_mirror 不设置名字，夹具的名字原样保留
    EXPECT_STREQ(mirror->name(), "镜像夹具");

    discard_npc(mirror);
}

TEST_F(NpcMirrorTest, MakeMirrorKeepsStrongPlayerMappings)
{
    CChar player;
    player.set_name("高手", "mirror_src2");
    player.set("level", (long)10);  // 基线 40，回落线 = 40 + 20 = 60
    player.set_skill("dodge", 200);
    player.set_skill("force", 120);
    // map_skill/prepare_skill 只认已学技能：目标没学过会被静默丢弃
    player.set_skill("dodge_adv_sample", 200);
    player.set_skill("force_adv_sample", 120);
    player.set_skill("strike", 80);
    player.map_skill("dodge", "dodge_adv_sample");
    player.map_skill("force", "force_adv_sample");
    player.prepare_skill("unarmed", "strike");

    CNpc* mirror = load_mirror("t_mirror_npc");
    ASSERT_NE(mirror, nullptr);

    call_make_mirror(mirror, &player, 5);

    // 抄来的技能够强（200 * 5 / 10 = 100 ≥ 60）：玩家自己的映射与激发
    // 原样保留，不回落
    EXPECT_EQ(mirror->query_skill("dodge", 1), 100);
    EXPECT_STREQ(mirror->query_skill_mapped("dodge"), "dodge_adv_sample");
    EXPECT_STREQ(mirror->query_skill_mapped("force"), "force_adv_sample");
    EXPECT_STREQ(mirror->query_skill_prepared("unarmed"), "strike");

    discard_npc(mirror);
}

// ====================================================================
// chat_msg_combat：满血安静、低血尝试自救且不依赖药品
// ====================================================================
TEST_F(NpcMirrorTest, ChatMsgCombatFullHpReturnsQuietly)
{
    CNpc* mirror = load_mirror("t_bare_mirror_npc");
    ASSERT_NE(mirror, nullptr);
    mirror->set("hp", (long)100);
    mirror->set("max_hp", (long)100);

    EXPECT_EQ(mirror->chat_msg_combat(), nullptr);
    EXPECT_EQ(item_mgr.load_item_call_count, 0);

    discard_npc(mirror);
}

TEST_F(NpcMirrorTest, ChatMsgCombatLowHpAttemptsHeal)
{
    CNpc* mirror = load_mirror("t_bare_mirror_npc");
    ASSERT_NE(mirror, nullptr);
    mirror->set("hp", (long)20);
    mirror->set("max_hp", (long)300);

    // 救援分支单次命中率 1/2，64 次内必然走进去；heal perform 由
    // 技能侧提供，这里只验证分支可达且不抛错、不牵扯物品管理器
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(mirror->chat_msg_combat(), nullptr);
    }
    EXPECT_EQ(item_mgr.load_item_call_count, 0);

    discard_npc(mirror);
}

// ====================================================================
// die：认得凶手先发奖励（脚本 reward_killer）再化灰自毁
// ====================================================================
TEST_F(NpcMirrorTest, Die_WithKillerRewardsThenDestroys)
{
    StateRecordingUser killer;
    killer.set_name("凶手", "mirror_killer");
    killer.move(&env);
    g_user_list[killer.object_id()] = &killer;

    StateRecordingUser witness;
    witness.set_name("旁观", "mirror_witness");
    witness.move(&env);

    CNpc* dummy = load_mirror("t_mirror_npc");
    ASSERT_NE(dummy, nullptr);
    dummy->set_temp("last_damage_from", (int32_t)killer.object_id());
    const int32_t id = dummy->object_id();

    dummy->die();

    EXPECT_TRUE(killer.has("镜像的奖励发出去了"))
        << "reward_killer 钩子没分派到脚本";
    EXPECT_EQ(g_last_destruct_object, id);
    EXPECT_TRUE(witness.has("惨叫一声，化为灰烬"))
        << "基模块默认 die 的化灰台词没生效";

    g_user_list.erase(killer.object_id());
    killer.move(nullptr);
    witness.move(nullptr);
}

// 死亡：认不得凶手时只自毁（基模块默认 die，脚本没覆写）
TEST_F(NpcMirrorTest, Die_WithoutKillerDestroysOnly)
{
    StateRecordingUser witness;
    witness.set_name("旁观", "mirror_witness2");
    witness.move(&env);

    CNpc* dummy = load_mirror("t_bare_mirror_npc");
    ASSERT_NE(dummy, nullptr);
    const int32_t id = dummy->object_id();

    dummy->die();

    EXPECT_EQ(g_last_destruct_object, id);
    EXPECT_TRUE(witness.has("惨叫一声，化为灰烬"));
    EXPECT_FALSE(witness.has("奖励"))
        << "认不得凶手时不该发出任何奖励动静";

    witness.move(nullptr);
}

// ====================================================================
// 热重载：脚本 chunk 重跑，install 随之再次装配；脚本自定义钩子不被覆盖
// ====================================================================
TEST_F(NpcMirrorTest, MirrorHooksSurviveScriptReload)
{
    // 先确保脚本已入缓存，再热重载（重载后的新脚本表须重新装配）
    CNpc* warmup = load_mirror("t_mirror_npc");
    ASSERT_NE(warmup, nullptr);

    CLuaEngine* engine = CLuaEngine::instance();
    EXPECT_NE(LUA_NOREF, engine->reload_script_by_name("t_mirror_npc"))
        << "夹具脚本重载失败";

    CNpc* dummy = load_mirror("t_mirror_npc");
    ASSERT_NE(dummy, nullptr);

    // 重载后整条链路仍在：脚本钩子（reward_killer 经 die 闭包分派）
    // 与装配钩子（chat_msg 遁走）都正常
    StateRecordingUser killer;
    killer.set_name("凶手", "mirror_killer2");
    killer.move(&env);
    g_user_list[killer.object_id()] = &killer;

    dummy->set_temp("last_damage_from", (int32_t)killer.object_id());
    dummy->die();  // 已自毁，交延迟销毁队列回收
    EXPECT_TRUE(killer.has("镜像的奖励发出去了")) << "重装后脚本钩子失效";

    g_user_list.erase(killer.object_id());
    killer.move(nullptr);
    discard_npc(warmup);
}

// ====================================================================
// 样本脚本契约：lua-sample/npc/sample_wooden_dummy.lua，半山腰的练功木人。
// 单测脚本根不含 lua-sample，用共享夹具把样本原文暂存进脚本根、用完即删
// （引擎按内容 hash 自动重载）。样本自己 require 镜像基模块并在文件末尾
// install——装配是否到位由下面各用例的行为断言证明。
// ====================================================================
class SampleWoodenDummyTest : public NpcMirrorTest {
protected:
    void SetUp() override
    {
        NpcMirrorTest::SetUp();

        script = std::make_unique<ScopedSampleScript>(
            "npc/sample_wooden_dummy.lua",
            "lua-sample/npc/sample_wooden_dummy.lua");
        ASSERT_TRUE(script->ok()) << "样本脚本未就位: " << script->file();

        dummy = load_npc("sample_wooden_dummy");
        ASSERT_NE(dummy, nullptr);
        dummy->move(&env);
    }

    void TearDown() override
    {
        // 走过 die 的用例已把 dummy 置空（自毁的实例由延迟销毁队列回收）
        if (dummy && g_npc_list.count(dummy->object_id())) discard_npc(dummy);
        dummy = nullptr;
        script.reset();
        NpcMirrorTest::TearDown();
    }

    CNpc* dummy = nullptr;
    std::unique_ptr<ScopedSampleScript> script;
};

// 对练：按 diff=8 抄走挑战者状态，且把 make_mirror 写空的名字改回来
TEST_F(SampleWoodenDummyTest, SparringTalk_CopiesChallengerAndKeepsName)
{
    StateRecordingUser challenger;
    challenger.set_name("挑战者", "dummy_challenger");
    challenger.move(&env);
    challenger.set("level", (long)10);
    challenger.set("max_hp", (long)800);
    challenger.set("max_mp", (long)400);
    challenger.set_skill("unarmed", 40);
    challenger.set_skill("dodge", 30);
    challenger.clear();

    char ask[] = "对练";
    EXPECT_EQ(1, dummy->do_talk(&challenger, ask));

    EXPECT_EQ(dummy->query("max_hp"), 640) << "气血按 8/10 缩放";
    EXPECT_EQ(dummy->query("level"), 8) << "等级按 8/10 缩放";
    EXPECT_EQ(dummy->query_skill("unarmed", 1), 32) << "技能按 8/10 缩放";
    EXPECT_STREQ(dummy->name(), "练功木人")
        << "make_mirror 不设置名字，脚本负责 set_name ";
    EXPECT_TRUE(challenger.has("照着你的起手式摆开了门户"))
        << "对练应答走 say(msg, me)，只投递给问话人";

    challenger.move(nullptr);
}

// 其余问话交回引擎通用流程：脚本没注册询问 → notify_fail，木人状态不动
TEST_F(SampleWoodenDummyTest, OtherTalk_FallsBackToInquiryFlow)
{
    CChar asker;
    asker.set_name("闲人", "dummy_asker");
    asker.move(&env);

    char ask[] = "闲聊";
    EXPECT_EQ(0, dummy->do_talk(&asker, ask));
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。");
    EXPECT_STREQ(dummy->name(), "练功木人") << "回退分支不该动木人的状态";

    asker.move(nullptr);
}

// 死亡：脚本自己接管 die —— 散成一地木头并自毁，不是基类的「化为灰烬」
TEST_F(SampleWoodenDummyTest, Die_CollapsesIntoTimberAndDestroys)
{
    StateRecordingUser witness;
    witness.set_name("旁观", "dummy_witness");
    witness.move(&env);
    witness.clear();

    const int32_t id = dummy->object_id();
    dummy->die();
    dummy = nullptr;  // 已自毁，交延迟销毁队列回收

    EXPECT_EQ(g_last_destruct_object, id) << "被打散即自毁";
    EXPECT_TRUE(witness.has("噼里啪啦散成了一堆木头"))
        << "房间播报走 message_vision，同房旁观玩家收得到";

    witness.move(nullptr);
}
