// test_char_core.cpp - Unit tests for Char.cpp (core lifecycle/heartbeat).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "test_lua_env.h"

class CharCoreTest : public ::testing::Test {
protected:
    CChar* ch;
    void SetUp() override
    {
        // needed by setup_char / load_room
        ensure_lua_env();
        ch = new CChar();
        ch->set("name", "core");
        ch->set("id", "core1");
        ch->set("hp", (long)100);
        ch->set("eff_hp", (long)100);
        ch->set("max_hp", (long)100);
        ch->set("mp", (long)50);
        ch->set("max_mp", (long)50);
        ch->set("con", (long)20);
        ch->set("combat_exp", (long)1000);
        ch->set("level", (long)10);
    }
    void TearDown() override { delete ch; }
};

// ---------------- Query ----------------
TEST_F(CharCoreTest, Query_Attrs)
{
    EXPECT_EQ(ch->query_attr(CChar::IS_CHAR), 1);
    EXPECT_EQ(ch->query_attr(CChar::IS_LIVING), 1);
    EXPECT_EQ(ch->query_attr(CChar::IS_BUSY), 0);
    // Query(IS_BUSY) reflects the function-busy pointer, not the counter.
    ch->start_busy(2, NULL, NULL);
    EXPECT_EQ(ch->query_attr(CChar::IS_BUSY), 0);
    EXPECT_EQ(ch->is_busy(), 1);  // is_busy covers the counter too
}

TEST_F(CharCoreTest, Query_DisabledNotLiving)
{
    ch->disable_player("昏迷");
    EXPECT_EQ(ch->query_attr(CChar::IS_LIVING), 0);
    ch->enable_player();
    EXPECT_EQ(ch->query_attr(CChar::IS_LIVING), 1);
}

// ---------------- equip ----------------
TEST_F(CharCoreTest, EquipListAndQuery)
{
    EXPECT_NE(ch->get_equip_list(), nullptr);
    EXPECT_EQ(ch->query_equip(1), 0);
    (*ch->get_equip_list())[1] = 12345;
    EXPECT_EQ(ch->query_equip(1), 12345);
}

// ---------------- Wield_Position ----------------
TEST_F(CharCoreTest, WieldPosition)
{
    EXPECT_STREQ(CChar::wield_position(0), "无");
    EXPECT_STREQ(CChar::wield_position(1), "头");
}

// ---------------- command ----------------
TEST_F(CharCoreTest, Command_Unknown)
{
    char cmd[] = "this_command_does_not_exist";
    EXPECT_EQ(ch->command(cmd), 0);
}

// ---------------- enable/disable player ----------------
TEST_F(CharCoreTest, EnableDisablePlayer)
{
    EXPECT_EQ(ch->disable_player("busy"), 1);
    EXPECT_STREQ(ch->querystr_temp("disabled"), "busy");
    EXPECT_EQ(ch->enable_player(), 1);
    EXPECT_EQ(ch->querystr_temp("disabled")[0], '\0');
}

// ---------------- is_busy / is_fighting ----------------
TEST_F(CharCoreTest, IsBusy)
{
    EXPECT_EQ(ch->is_busy(), 0);
    ch->start_busy(3, NULL, NULL);
    EXPECT_EQ(ch->is_busy(), 1);
}

TEST_F(CharCoreTest, IsFighting)
{
    EXPECT_EQ(ch->is_fighting(), 0);
    ch->set_temp("fight/is_fighting", (long)1);
    EXPECT_EQ(ch->is_fighting(), 1);
}

// ---------------- condition ----------------
TEST_F(CharCoreTest, ConditionQuery)
{
    EXPECT_EQ(ch->query_condition("poison"), 0);
    ch->apply_condition("poison", 5, 0);
    EXPECT_GT(ch->query_condition("poison"), 0);
    ch->clear_condition();
    EXPECT_EQ(ch->query_condition("poison"), 0);
}

// ---------------- is_killing ----------------
TEST_F(CharCoreTest, IsKilling)
{
    CChar* foe = new CChar();
    foe->set("id", "foe1");
    EXPECT_EQ(ch->is_killing(foe), 0);
    ch->set_killer(foe);  // records by id; is_killing checks object_id
    ch->remove_all_killer();
    EXPECT_EQ(ch->is_killing(foe), 0);
    delete foe;
}

// ---------------- query_team ----------------
TEST_F(CharCoreTest, QueryTeam)
{
    EXPECT_EQ(ch->query_team(), 0);
    ch->set_temp("fight/fight_team", (long)1);
    EXPECT_EQ(ch->query_team(), 1);
}

// ---------------- Heal_Up ----------------
class TestCoreChar : public CChar {
public:
    int DoHealUp() { return heal_up(); }
    // heart_beat 深路径的观测暴露（protected 节律/计数成员）
    using CChar::m_busy_count;
    using CChar::m_revive;
    using CChar::m_tick;
    using CChar::m_tick_two;
    using CChar::m_wait_loading;
    void PrimeTick()
    {
        m_tick     = 0;
        m_tick_two = 8;
    }
};

// 加载分支需要 userp(this)：user 侧只暴露加载计数
class TestBeatUser : public CUser {
public:
    using CChar::m_wait_loading;
};

TEST_F(CharCoreTest, heal_up)
{
    TestCoreChar* t = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "coreh");
    t->set("hp", (long)50);
    t->set("eff_hp", (long)100);
    t->set("max_hp", (long)100);
    t->set("con", (long)20);
    int flag = t->DoHealUp();
    EXPECT_GE(t->query("hp"), 50);
    EXPECT_GE(flag, 0);
    delete t;
}

// ---------------- remove_faint ----------------
TEST_F(CharCoreTest, RemoveFaint)
{
    ch->set_temp("last_fainted_from", "someone");
    CChar::remove_faint(ch, 0, 0);
    EXPECT_EQ(ch->querystr_temp("last_fainted_from")[0], '\0');
}

// ---------------- reset_level ----------------
TEST_F(CharCoreTest, ResetLevel_NonUser)
{
    ch->reset_level();  // not IS_USER -> early return
    EXPECT_EQ(ch->query("level"), 10);
}

TEST_F(CharCoreTest, ResetLevel_User)
{
    CUser* u = new CUser();
    u->set("id", "core2");
    u->set("name", "玩家");
    u->set("level", (long)5);
    u->set("combat_exp", (long)0);
    u->set("sk_exp", (long)0);
    u->reset_level();
    EXPECT_GE(u->query("level"), 1);
    delete u;
}

// ---------------- setup / enable ----------------
TEST_F(CharCoreTest, SetupRuns)
{
    CUser* u = new CUser();
    u->set("id", "core3");
    u->set("name", "玩家");
    u->setup();  // enable_player + setup_char
    EXPECT_EQ(u->querystr_temp("disabled")[0], '\0');
    delete u;
}

// ---------------- heart_beat ----------------
TEST_F(CharCoreTest, HeartBeat_NoEnv)
{
    ch->heart_beat();  // no environment -> immediate return
    EXPECT_EQ(ch->query("hp"), 100);
}

TEST_F(CharCoreTest, HeartBeat_Loading)
{
    CContainer* room = new CContainer();
    ch->move(room);
    ch->set_temp("client/loading", (long)1);
    ch->heart_beat();  // loading branch
    EXPECT_EQ(ch->query("hp"), 100);
    ch->move(nullptr);
    delete room;
}

TEST_F(CharCoreTest, HeartBeat_NormalTick)
{
    CContainer* room = new CContainer();
    ch->move(room);
    ch->heart_beat();  // normal path, no death
    EXPECT_GT(ch->query("hp"), 0);
    ch->move(nullptr);
    delete room;
}

TEST_F(CharCoreTest, HeartBeat_HpNegativeUnconcious)
{
    CContainer* room = new CContainer();
    ch->move(room);
    ch->set("hp", (long)-1);  // hp<0 but living -> unconcious (no destruct)
    ch->heart_beat();
    EXPECT_STREQ(ch->querystr_temp("disabled"), "昏迷不醒");
    ch->move(nullptr);
    delete room;
}

extern char g_notify_fail_msg[512];

// ---------------- heal_up 封顶路径(278-280) ----------------
TEST_F(CharCoreTest, HealUp_CapsHpAndRaisesEff)
{
    TestCoreChar* t = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "heal_cap");
    t->set("hp", (long)95);
    t->set("eff_hp", (long)100);
    t->set("max_hp", (long)150);
    t->set("con", (long)30);  // +10 → 105 ≥ eff → 封顶支
    int flag = t->DoHealUp();
    EXPECT_EQ(1, flag) << "封顶路径产生更新标记";
    EXPECT_EQ(100, t->query("hp")) << "hp 封顶到 eff_hp(278)";
    EXPECT_EQ(101, t->query("eff_hp")) << "eff_hp 未满 → +1(279-280)";
    delete t;
}

// ---------------- heart_beat 深拍：heal_up 生效支(180-182) ----------------
TEST_F(CharCoreTest, HeartBeat_DeepTickHealSendsState)
{
    CContainer* room = new CContainer();
    TestCoreChar* t  = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "deep_hb");
    t->set("hp", (long)95);
    t->set("eff_hp", (long)100);
    t->set("max_hp", (long)150);
    t->set("con", (long)30);
    t->move(room);

    t->PrimeTick();  // m_tick=0 → 本拍推进 m_tick_two ≥ 8 → 进入 heal 段
    t->heart_beat();
    EXPECT_EQ(101, t->query("eff_hp")) << "深拍 heal_up 生效(180)";
    EXPECT_EQ(100, t->query("hp")) << "hp 封顶(278同拍)";
    t->move(nullptr);
    delete t;
    delete room;
}

// ---------------- heart_beat 深拍：满血关拍支(184,186,188) ----------------
TEST_F(CharCoreTest, HeartBeat_DeepFullHpShutsDown)
{
    CContainer* room = new CContainer();
    TestCoreChar* t  = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "deep_full");
    t->set("hp", (long)100);
    t->set("eff_hp", (long)100);
    t->set("max_hp", (long)100);
    t->set("con", (long)30);  // hp 封顶但 eff==max → heal_up 返回 0
    t->move(room);

    t->PrimeTick();
    t->heart_beat();
    EXPECT_EQ(100, t->query("eff_hp"))
        << "满血 heal_up 不产生更新 → 走 else-if 关拍支(184,186)";
    t->move(nullptr);
    delete t;
    delete room;
}

// ---------------- 心跳：经验值下限回收(133) ----------------
TEST_F(CharCoreTest, HeartBeat_CombatExpFloor)
{
    CContainer* room = new CContainer();
    TestCoreChar* t  = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "expfloor");
    t->set("combat_exp", (long)-2100000000);
    t->move(room);
    t->heart_beat();
    EXPECT_EQ(2147483647L, t->query("combat_exp"))
        << "经验值低于下限 → 回收为上限(133)";
    t->move(nullptr);
    delete t;
    delete room;
}

// ---------------- 心跳：加载超5拍自动兜底指令(102) ----------------
TEST_F(CharCoreTest, HeartBeat_LoadingAutoLocAfterWait)
{
    CContainer* room = new CContainer();
    TestBeatUser* u  = new TestBeatUser();
    u->set("name", "loadu");
    u->set("id", "load_user1");
    u->move(room);
    u->set_temp("client/loading", (long)1);  // is_loading()

    g_notify_fail_msg[0] = '\0';
    for (int i = 0; i < 6; i++) u->heart_beat();
    EXPECT_EQ(6, u->m_wait_loading) << "加载等待逐拍累计(第6拍为指令拍)";
    EXPECT_EQ(0, (long)u->query_temp("client/loading"))
        << "第6拍触发 loc 兜底指令(102)并解除装载态";
    u->move(nullptr);
    delete u;
    delete room;
}

// ---------------- 心跳：复活倒计时(116) ----------------
TEST_F(CharCoreTest, HeartBeat_ReviveCountdown)
{
    CContainer* room = new CContainer();
    TestCoreChar* t  = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "rev1");
    t->move(room);
    t->m_revive = 1;
    t->heart_beat();
    EXPECT_EQ(0, t->m_revive) << "复活倒计时逐拍递减至0 → revive(116)";
    t->move(nullptr);
    delete t;
    delete room;
}

// ---------------- 心跳：忙碌推进 continue_action(147) ----------------
TEST_F(CharCoreTest, HeartBeat_BusyContinuesAction)
{
    CContainer* room = new CContainer();
    TestCoreChar* t  = new TestCoreChar();
    t->set("name", "core");
    t->set("id", "busy1");
    t->move(room);
    t->start_busy(3, NULL, NULL);
    ASSERT_EQ(1, t->is_busy());

    t->heart_beat();
    EXPECT_EQ(2, t->m_busy_count) << "忙碌拍推进计数 continue_action(147)";

    // 清理防心跳/忙碌残留
    t->m_busy_count = 0;
    t->set_heart_beat(0);
    t->move(nullptr);
    delete t;
    delete room;
}
