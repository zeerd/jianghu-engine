// test_npc_mirror.cpp — CNpcMirror：镜像生成、自疗、遁走与死亡奖励
//
// 契约：
//  - make_mirror 按倍率复制玩家等级/属性/技能，并为其配一把武器
//  - make_weapon 无可用技击技能时放弃配装
//  - 血线低于 1/3 时 chat_msg_combat 尝试自救（吃药）
//  - 满血时 chat_msg_combat 不做动作直接返回
//  - chat_msg 有概率遁走并自毁
//  - die 找到凶手先发奖励再自毁，找不到凶手仅自毁
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstdint>

#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "Item.h"
#include "Npc.h"
#include "NpcMirror.h"
#include "Room.h"
#include "User.h"
#include "LuaBindDecls.h"  // l_make_mirror
#include "LuaBind.h"       // lua_bind_push_object
#include "test_lua_env.h"
#include "test_state_recorder.h"
#include "mock/mock_itemmgr.h"

extern MAP2USER g_user_list;

namespace {

CContainer* g_last_factory_item = nullptr;

// 物品工厂：镜像的武器与自救药品都走真实 CItem
CContainer* mirror_item_factory(const char* base_name)
{
    if (!base_name || !base_name[0]) return nullptr;
    CItem* item = new CItem();
    item->set("base_name", base_name);
    item->create();
    item->set_name(base_name);
    g_last_factory_item = item;
    return item;
}

// 记录奖励钩子，同时执行基类实现
class RewardSpyMirror : public CNpcMirror {
public:
    void reward_killer(CChar* me) override
    {
        CNpcMirror::reward_killer(me);
        reward_count++;
        last_killer = me;
    }

    int reward_count   = 0;
    CChar* last_killer = nullptr;
};

class NpcMirrorTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom env;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr             = g_item_mgr;
        item_mgr.load_factory = &mirror_item_factory;
        g_item_mgr            = &item_mgr;
        g_last_factory_item   = nullptr;
        env.set("infinity", 1);
    }

    void TearDown() override { g_item_mgr = saved_mgr; }
};

}  // namespace

// 镜像按倍率复制玩家状态，并领到一把木剑
TEST_F(NpcMirrorTest, MakeMirror_ScalesPlayerStateAndArmsItself)
{
    CChar player;
    player.set_name("本尊", "mirror_src");
    player.set("level", (long)30);
    player.set("max_hp", (long)1000);
    player.set("max_mp", (long)500);
    player.set("str", (long)20);
    player.set_skill("unarmed", 50);
    player.set_skill("sword", 30);
    player.set_temp("fool/name", "镜像怪");

    CNpcMirror mirror;
    mirror.set_name("镜像", "mirror_npc");
    mirror.set("infinity", 1);
    mirror.make_mirror(&player, 5);

    EXPECT_STREQ(mirror.name(), "镜像怪");
    EXPECT_EQ(mirror.query("level"), 15);
    EXPECT_EQ(mirror.query("max_hp"), 500);
    EXPECT_EQ(mirror.query("hp"), mirror.query("max_hp"));
    EXPECT_EQ(mirror.query("max_mp"), 250);
    EXPECT_EQ(mirror.query_skill("taishan_sword", 1), 100);
    EXPECT_EQ(mirror.query_skill("unarmed", 1), 25);
    ASSERT_NE(g_last_factory_item, nullptr) << "镜像必须领到武器";
    EXPECT_STREQ(g_last_factory_item->name(), "木刺");
    EXPECT_EQ(g_last_factory_item->environment(), &mirror);
}

// 只有身法/内功/招架时没有可配的技击技能，放弃配装
TEST_F(NpcMirrorTest, MakeWeapon_WithoutOffensiveSkill_ReturnsNull)
{
    CChar player;
    player.set("level", (long)30);
    player.set_skill("dodge", 50);
    player.set_skill("force", 50);
    player.set_skill("parry", 50);

    EXPECT_EQ(CNpcMirror::make_weapon(&player), nullptr);
    EXPECT_EQ(g_last_factory_item, nullptr) << "不该向物品管理要装备";
}

// 血线低于 1/3 时尝试吃药自救，药必须入包
TEST_F(NpcMirrorTest, ChatMsgCombat_LowHpCarriesPotion)
{
    CNpcMirror mirror;
    mirror.set_name("镜像", "mirror_cure");
    mirror.set("infinity", 1);
    mirror.move(&env);
    mirror.set("max_hp", (long)300);
    mirror.set("hp", (long)50);

    // 吃药分支单次命中率 1/3，64 次内必然命中
    for (int i = 0; i < 64 && g_last_factory_item == nullptr; i++)
        mirror.chat_msg_combat();

    ASSERT_NE(g_last_factory_item, nullptr) << "低血必须尝试自救";
    const std::string potion = g_last_factory_item->name();
    EXPECT_TRUE(potion == "yaopin_1" || potion == "yaopin_2") << potion;
    EXPECT_EQ(g_last_factory_item->environment(), &mirror);

    mirror.move(nullptr);
}

// 满血时不进自救分支，直接返回
TEST_F(NpcMirrorTest, ChatMsgCombat_FullHpReturnsQuietly)
{
    CNpcMirror mirror;
    mirror.set_name("镜像", "mirror_full");
    mirror.set("infinity", 1);
    mirror.move(&env);
    mirror.set("max_hp", (long)300);
    mirror.set("hp", (long)300);

    EXPECT_EQ(mirror.chat_msg_combat(), nullptr);
    EXPECT_EQ(g_last_factory_item, nullptr);

    mirror.move(nullptr);
}

// 逃走判定有概率触发，触发后自毁
TEST_F(NpcMirrorTest, ChatMsg_FleesAndDestroysItself)
{
    CNpcMirror* mirror = new CNpcMirror();
    mirror->set_name("镜像", "mirror_flee");
    mirror->move(&env);
    const int32_t id = mirror->object_id();

    bool fled = false;
    // 单次逃走概率 1/10，500 次内命中概率为 1-0.9^500
    for (int i = 0; i < 500; i++) {
        mirror->chat_msg();
        if (g_last_destruct_object == id) {
            fled = true;
            break;
        }
    }

    EXPECT_TRUE(fled) << "500 次内镜像必须遁走自毁";
    if (!fled) {
        mirror->set_heart_beat(0);
        mirror->move(nullptr);
        delete mirror;
    }
}

// 死亡：认得凶手时先发奖励，再自毁
TEST_F(NpcMirrorTest, Die_WithKillerRewardsThenDestroys)
{
    StateRecordingUser killer;
    killer.set_name("凶手", "mirror_killer");
    killer.move(&env);
    g_user_list[killer.object_id()] = &killer;

    RewardSpyMirror* mirror = new RewardSpyMirror();
    mirror->set_name("镜像", "mirror_die");
    mirror->move(&env);
    mirror->set_temp("last_damage_from", (int32_t)killer.object_id());
    const int32_t id = mirror->object_id();

    mirror->die();

    EXPECT_EQ(mirror->reward_count, 1);
    EXPECT_EQ(mirror->last_killer, &killer);
    EXPECT_EQ(g_last_destruct_object, id);

    g_user_list.erase(killer.object_id());
    killer.move(nullptr);
}

// 死亡：认不得凶手时只自毁
TEST_F(NpcMirrorTest, Die_WithoutKillerDestroysOnly)
{
    CNpcMirror* mirror = new CNpcMirror();
    mirror->set_name("镜像", "mirror_die_alone");
    mirror->move(&env);
    const int32_t id = mirror->object_id();

    mirror->die();

    EXPECT_EQ(g_last_destruct_object, id);
}

// 吃药：药从物品管理取走后入包并触发使用指令
TEST_F(NpcMirrorTest, EatYao_CarriesPotionAndUsesIt)
{
    CNpcMirror mirror;
    mirror.set_name("镜像", "mirror_eat");
    mirror.set("infinity", 1);
    mirror.move(&env);

    mirror.eat_yao((char*)"yaopin_1");

    ASSERT_NE(g_last_factory_item, nullptr);
    EXPECT_EQ(g_last_factory_item->environment(), &mirror);

    mirror.move(nullptr);
}

// 可选钩子的基类实现不得改动状态
TEST_F(NpcMirrorTest, CatchOb_LeavesStateUntouched)
{
    CNpcMirror mirror;
    mirror.set_name("镜像", "mirror_catch");
    mirror.set("hp", (long)123);

    CChar catcher;
    catcher.set_name("路人", "mirror_catcher");
    mirror.catch_ob(&catcher);

    EXPECT_EQ(mirror.query("hp"), 123);
}

// ====================================================================
// l_make_mirror 绑定深路：全参数（obj, me, diff）→ 46-54 全程
// 经自建 pcall（等价 BindTest::call 的安全上下文）。
// ====================================================================
TEST_F(NpcMirrorTest, MakeMirrorBindFullArgsScalesPlayer)
{
    lua_State* L = g_LuaEngine->get_state();

    CChar player;
    player.set_name("原型", "bind_src");
    player.set("level", (long)30);
    player.set("max_hp", (long)1000);
    player.set("max_mp", (long)500);
    player.set("str", (long)20);
    player.set_skill("unarmed", 50);
    player.set_skill("sword", 30);
    player.set_temp("fool/name", "愚者");

    CNpcMirror mirror;
    mirror.set_name("镜妖", "bind_mirror");
    mirror.set("infinity", 1);

    lua_pushcfunction(L, l_make_mirror);
    lua_bind_push_object(L, &mirror);
    lua_bind_push_object(L, &player);
    lua_pushinteger(L, 5);
    int rc = lua_pcall(L, 3, 0, 0);
    ASSERT_EQ(LUA_OK, rc) << lua_tostring(L, -1);

    EXPECT_EQ(15, static_cast<long>(mirror.query("level")))
        << "按 diff 缩放等级(52-54)";
    EXPECT_EQ(500, static_cast<long>(mirror.query("max_hp")))
        << "属性同步(46-54 全程)";
    ASSERT_NE(g_last_factory_item, nullptr) << "镜像武器已造";
    EXPECT_EQ(g_last_factory_item->environment(), &mirror);
}
