// test_char_fight.cpp - Unit tests for CharFight.cpp (武器/战斗触发/比试).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Room.h"
#include "Item.h"
#include "AutoFight.h"  // CAutoFight::start_* 摘 call_out
#include "Combatd.h"
#include "Global.h"
#include "LuaEngine.h"
#include "test_lua_env.h"
#include "mock/mock_itemmgr.h"

extern char g_notify_fail_msg[512];

class CharFightTest : public ::testing::Test {
protected:
    CChar* ch;
    void SetUp() override
    {
        ensure_lua_env();
        ch = new CChar();
        ch->set("name", "f");
        ch->set("id", "f1");
        ch->set("hp", (long)100);
        ch->set("eff_hp", (long)100);
        ch->set("max_hp", (long)100);
    }
    void TearDown() override { delete ch; }
};

// ---------------- query_weapon / left / shield ----------------
TEST_F(CharFightTest, QueryWeapon_Empty)
{
    EXPECT_EQ(ch->query_weapon(), nullptr);
    EXPECT_EQ(ch->query_left_weapon(), nullptr);
    EXPECT_EQ(ch->query_shield(), nullptr);
}

// ---------------- query_unarmed ----------------
TEST_F(CharFightTest, QueryUnarmed_None) { EXPECT_EQ(ch->query_unarmed(), 0); }

TEST_F(CharFightTest, QueryUnarmed_MaxOfSkills)
{
    ch->set_skill("unarmed", 10);
    ch->set_skill("strike", 30);
    ch->set_skill("claw", 20);
    EXPECT_EQ(ch->query_unarmed(), 30);
}

// ---------------- query_action ----------------
TEST_F(CharFightTest, QueryAction_Human)
{
    ch->set("race", "人类");
    CMapping* m = ch->query_action();
    EXPECT_NE(m, nullptr);
    EXPECT_NE(m->querystr("action")[0], '\0');
}

TEST_F(CharFightTest, QueryAction_Beast)
{
    ch->set("race", "野兽");
    CVector verbs;
    verbs.append("bite");
    ch->set("verbs", verbs);
    CMapping* m = ch->query_action();
    EXPECT_NE(m, nullptr);
}

TEST_F(CharFightTest, QueryAction_Default)
{
    ch->set("race", "妖怪");  // neither human nor beast
    CMapping* m = ch->query_action();
    EXPECT_STREQ(m->querystr("action"), "$N攻击$n的$l。");
}

// ---------------- init (auto-fight triggers) ----------------
TEST_F(CharFightTest, Init_NotFightingNoTrigger)
{
    CChar* other = new CChar();
    other->set("id", "o1");
    ch->init(other);  // 三个 auto_fight 分支都不中 -> 不触发
    EXPECT_EQ(0, ch->query_temp("looking_for_trouble"));
    EXPECT_FALSE(ch->is_fighting());
    delete other;
}

TEST_F(CharFightTest, Init_AggressiveUser)
{
    CUser* other = new CUser();
    other->set("id", "o2");
    other->set("name", "玩家");
    ch->set("attitude", "aggressive");
    CContainer* room = new CContainer();
    ch->move(room);
    other->move(room);
    ch->init(other);  // aggressive 分支 -> CAutoFight::auto_fight 排程
    // 契约：auto_fight 已触发（AutoFight.cpp 置 looking_for_trouble）
    EXPECT_EQ(1, ch->query_temp("looking_for_trouble"));
    ch->move(nullptr);
    other->move(nullptr);
    delete other;
    delete room;
}

// ---------------- is_defeated ----------------
TEST_F(CharFightTest, IsDefeated_Winner)
{
    CChar* winner = new CChar();
    winner->set("id", "w1");
    winner->set("name", "胜者");
    CChar* failer = new CChar();
    failer->set("id", "l1");
    CContainer* room = new CContainer();
    winner->move(room);
    failer->move(room);
    winner->is_defeated(winner, failer);  // winner == this -> 消息（mock）
    // 契约：is_defeated 是纯消息函数，不产生战斗状态
    EXPECT_FALSE(winner->is_fighting(failer));
    winner->move(nullptr);
    failer->move(nullptr);
    delete winner;
    delete failer;
    delete room;
}

TEST_F(CharFightTest, IsDefeated_NotWinner)
{
    CChar* winner = new CChar();
    winner->set("id", "w2");
    CChar* failer = new CChar();
    failer->set("id", "l2");
    ch->is_defeated(winner, failer);  // this != winner -> no-op
    // 对照组：非胜者分支同样无状态变化
    EXPECT_FALSE(ch->is_fighting(failer));
    delete winner;
    delete failer;
}

// ---------------- cmd_hit_ob ----------------
TEST_F(CharFightTest, CmdHitOb_NoEnv)
{
    CChar* foe = new CChar();
    foe->set("id", "h1");
    char arg[] = "";
    EXPECT_EQ(ch->cmd_hit_ob(foe, arg), 0);  // no environment -> notify_fail
    delete foe;
}

TEST_F(CharFightTest, CmdHitOb_NoFightRoom)
{
    CContainer* room = new CContainer();
    room->set("no_fight", (long)1);
    ch->move(room);
    CChar* foe = new CChar();
    foe->set("id", "h2");
    foe->move(room);
    char arg[] = "";
    EXPECT_EQ(ch->cmd_hit_ob(foe, arg), 0);  // no_fight -> notify_fail
    ch->move(nullptr);
    foe->move(nullptr);
    delete foe;
    delete room;
}

TEST_F(CharFightTest, CmdHitOb_DifferentEnv)
{
    CContainer* room = new CContainer();
    ch->move(room);
    CChar* foe = new CChar();
    foe->set("id", "h3");  // foe has no env -> different
    char arg[] = "";
    EXPECT_EQ(ch->cmd_hit_ob(foe, arg), 0);
    ch->move(nullptr);
    delete foe;
    delete room;
}

// ---------------- cmd_hit_ob 等级门：玩家互斗需双方 >= 75 级 ----------------
static CUser* MakeGateUser(const char* name, const char* id, long level)
{
    CUser* u = new CUser();
    u->set_name(name, id);
    u->set("level", level);
    u->set("hp", (long)100);
    u->set("eff_hp", (long)100);
    return u;
}

TEST_F(CharFightTest, CmdHitOb_LevelGate_LowLevelUsersRejected)
{
    CContainer* room = new CContainer();
    CUser* attacker  = MakeGateUser("刺客", "lv_gate_a", 50);
    CUser* victim    = MakeGateUser("路人", "lv_gate_v", 50);
    attacker->move(room);
    victim->move(room);

    g_notify_fail_msg[0] = '\0';
    char arg[]           = "";
    EXPECT_EQ(attacker->cmd_hit_ob(victim, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "才入江湖，何必动刀动枪。");

    victim->move(nullptr);
    attacker->move(nullptr);
    delete victim;
    delete attacker;
    delete room;
}

TEST_F(CharFightTest, CmdHitOb_LevelGate_PassedThenBusyRejected)
{
    CContainer* room = new CContainer();
    CUser* attacker  = MakeGateUser("刺客", "lv_gate_a2", 75);
    CUser* victim    = MakeGateUser("路人", "lv_gate_v2", 75);
    attacker->move(room);
    victim->move(room);
    attacker->start_busy(1);

    g_notify_fail_msg[0] = '\0';
    char arg[]           = "";
    EXPECT_EQ(attacker->cmd_hit_ob(victim, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你现在正忙着。")
        << "双方已达等级，准入门放行后应落到 busy 判定";

    victim->move(nullptr);
    attacker->move(nullptr);
    delete victim;
    delete attacker;
    delete room;
}

// ---------------- CCombatd::fight: 连击链后的追加攻击钩子 ----------------
class CharFightHookTest : public CharFightTest {
protected:
    CContainer fight_room;
    CChar* victim = nullptr;

    void SetUp() override
    {
        CharFightTest::SetUp();
        fight_room.set_name("room", "fight_hook_room");
        victim = new CChar();
        victim->set("name", "v");
        victim->set("id", "fv1");
        ch->set("hp", (long)100000);
        ch->set("eff_hp", (long)100000);
        ch->set("max_hp", (long)100000);
        victim->set("hp", (long)100000);
        victim->set("eff_hp", (long)100000);
        victim->set("max_hp", (long)100000);
        ch->move(&fight_room);
        victim->move(&fight_room);
    }

    void TearDown() override
    {
        ch->move(nullptr);
        victim->move(nullptr);
        CharFightTest::TearDown();
        delete victim;
        victim = nullptr;
    }
};

// 空手备战连击（prepare_count>1 且无武器）：追加一次快击且 flag 复位
TEST_F(CharFightHookTest, Fight_QuickPrepareCombo_ExecutesAndResets)
{
    // prepare_skill 要求 mapped_to 技能已学且基础技能可加载
    ch->set_skill("test_a", 30);
    ch->set_skill("test_b", 30);
    ch->prepare_skill("unarmed", "test_a");
    ch->prepare_skill("finger", "test_b");
    EXPECT_EQ(ch->query_skill_prepare()->get_count(), 2);

    g_combatd.fight(ch, victim);

    EXPECT_EQ(ch->query_temp("action_flag"), 0)
        << "连击后 action_flag 必须复位";
}

// combatd_query 绑定：无该键时返回数值
TEST_F(CharFightHookTest, CombatdQuery_ReturnsNumber)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L, "_G.__cdq = combatd_query('no_such_key')"),
              LUA_OK);
    lua_getglobal(L, "__cdq");
    EXPECT_TRUE(lua_isnumber(L, -1));
    lua_pop(L, 1);
    luaL_dostring(L, "_G.__cdq = nil");
}

// ---------------- 深路：左手武器/盾、野兽默认动作、init 自动战两支
// ----------------

TEST_F(CharFightTest, QueryLeftWeaponAndShieldWithEquippedHand)
{
    auto* blade = new CItem();
    blade->set("id", "leftblade");
    blade->set("skill_type", "sword");
    blade->set("armor_type", "shield");
    blade->move(ch);
    (*ch->get_equip_list())[WIELD_LEFT_HAND] = blade->object_id();

    EXPECT_EQ(blade, ch->query_left_weapon()) << "左手武器命中并返回(46,49)";
    EXPECT_EQ(blade, ch->query_shield()) << "盾类判定命中(188,191)";
}

TEST_F(CharFightTest, QueryActionBeastWithoutVerbsUsesDefault)
{
    ch->set("race", "野兽");
    CMapping* m = ch->query_action();
    ASSERT_NE(m, nullptr);
    EXPECT_NE(m->querystr("action")[0], '\0') << "无 verbs 走默认文案(140)";
    EXPECT_EQ(0, m->query("damage")) << "默认伤害 0(141)";
}

TEST_F(CharFightTest, InitHatredMarksLookingForTrouble)
{
    auto* a = new CChar();
    a->set_name("甲", "init_a");
    auto* b = new CUser();
    b->set_name("乙", "init_b");
    b->set("id", "init_b");

    char oid_str[20];
    snprintf(oid_str, 20, "%ld", (long)b->object_id());
    b->set("id", oid_str);  // is_killing 按 object_id 比对 → id 对齐口径
    a->set_killer(b);       // is_killing(b) 为真
    a->init(b);

    EXPECT_EQ(1, a->query_temp("looking_for_trouble"))
        << "hatred 自动战已排程(67)";

    a->remove_call_out(&CAutoFight::start_hatred);  // 悬垂 call_out 防护
    delete a;
    b->move(nullptr);
    delete b;
}

TEST_F(CharFightTest, InitVendettaMarksLookingForTrouble)
{
    auto* a = new CChar();
    a->set_name("丙", "init_c");
    a->set("vendetta_mark", "血仇");
    auto* b = new CUser();
    b->set_name("丁", "init_d");
    b->set("id", "init_d");
    b->set("vendetta/血仇", 1);

    a->init(b);

    EXPECT_EQ(1, a->query_temp("looking_for_trouble"))
        << "vendetta 自动战已排程(71)";

    a->remove_call_out(&CAutoFight::start_vendetta);  // 悬垂 call_out 防护
    delete a;
    b->move(nullptr);
    delete b;
}
