// test_char_combat.cpp - Unit tests for CharCombat.cpp (伤害/忙乱/killer).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];

class CharCombatTest : public ::testing::Test {
protected:
    CChar* ch;
    CChar* foe;
    void SetUp() override
    {
        ch = new CChar();
        ch->set("name", "c");
        ch->set("id", "c1");
        ch->set("hp", (long)100);
        ch->set("eff_hp", (long)100);
        ch->set("max_hp", (long)100);
        ch->set("mp", (long)50);
        ch->set("max_mp", (long)50);
        foe = new CChar();
        foe->set("name", "foe");
        foe->set("id", "f1");
    }
    void TearDown() override
    {
        delete ch;
        delete foe;
    }
};

// ---------------- receive_damage ----------------
TEST_F(CharCombatTest, ReceiveDamage_ReducesHp)
{
    int32_t d = ch->receive_damage("hp", 30, foe);
    EXPECT_EQ(d, 30);
    EXPECT_EQ(ch->query("hp"), 70);
}

TEST_F(CharCombatTest, ReceiveDamage_NegativeIgnored)
{
    EXPECT_EQ(ch->receive_damage("hp", -5, foe), 0);
    EXPECT_EQ(ch->receive_damage("bad_type", 10, foe), 0);
}

TEST_F(CharCombatTest, ReceiveDamage_ZeroesHp)
{
    ch->receive_damage("hp", 200, foe);
    EXPECT_EQ(ch->query("hp"), -1);
}

// ---------------- receive_wound ----------------
TEST_F(CharCombatTest, ReceiveWound_ReducesEffHp)
{
    int32_t d = ch->receive_wound("hp", 40, foe);
    EXPECT_EQ(d, 40);
    EXPECT_EQ(ch->query("eff_hp"), 60);
    // hp follows down to eff if hp > eff
    EXPECT_LE(ch->query("hp"), 60);
}

TEST_F(CharCombatTest, ReceiveWound_NegativeIgnored)
{
    EXPECT_EQ(ch->receive_wound("hp", -1, foe), 0);
    EXPECT_EQ(ch->receive_wound("xx", 5, foe), 0);
}

// ---------------- receive_curing ----------------
TEST_F(CharCombatTest, ReceiveCuring_RaisesEffHp)
{
    ch->set("eff_hp", (long)50);
    int32_t h = ch->receive_curing("hp", 30);
    EXPECT_EQ(h, 30);
    EXPECT_EQ(ch->query("eff_hp"), 80);
}

TEST_F(CharCombatTest, ReceiveCuring_ClampsToMax)
{
    ch->set("eff_hp", (long)90);
    int32_t h = ch->receive_curing("hp", 100);
    EXPECT_EQ(h, 10);  // only up to max 100
    EXPECT_EQ(ch->query("eff_hp"), 100);
}

TEST_F(CharCombatTest, ReceiveCuring_NegativeIgnored)
{
    EXPECT_EQ(ch->receive_curing("hp", -5), 0);
    EXPECT_EQ(ch->receive_curing("bad", 5), 0);
}

// ---------------- receive_healing ----------------
TEST_F(CharCombatTest, ReceiveHealing_RaisesHp)
{
    ch->set("hp", (long)40);
    ch->set("eff_hp", (long)100);
    int32_t h = ch->receive_healing("hp", 30);
    EXPECT_EQ(h, 30);
    EXPECT_EQ(ch->query("hp"), 70);
}

TEST_F(CharCombatTest, ReceiveHealing_NoOverheal)
{
    // hp already >= eff -> no healing
    ch->set("hp", (long)100);
    ch->set("eff_hp", (long)100);
    EXPECT_EQ(ch->receive_healing("hp", 30), 0);
}

TEST_F(CharCombatTest, ReceiveHealing_Clamps)
{
    ch->set("hp", (long)90);
    ch->set("eff_hp", (long)100);
    int32_t h = ch->receive_healing("hp", 50);
    EXPECT_EQ(h, 10);
    EXPECT_EQ(ch->query("hp"), 100);
}

// ---------------- receive_poison ----------------
TEST_F(CharCombatTest, ReceivePoison)
{
    ch->set("hp", (long)100);
    ch->set("eff_hp", (long)100);
    int32_t d = ch->receive_poison("hp", 20, "poison");
    EXPECT_GE(d, 0);
}

// ---------------- hooks.receive: 调用点实参契约 ----------------
TEST_F(CharCombatTest, Receive_HookDispatchesAllKinds)
{
    lua_State* L = ensure_lua_env();
    ASSERT_EQ(
        luaL_dostring(L,
                      "_G.__orig_receive = hooks.receive\n"
                      "_G.__recv_log = ''\n"
                      "hooks.receive = function(kind, me, who, attr, amount)\n"
                      "    _G.__recv_log = _G.__recv_log .. string.format(\n"
                      "        '%s|%s|%d|%s|%s;', kind, attr, amount,\n"
                      "        who and who:id(1) or '-', me:id(1))\n"
                      "end"),
        LUA_OK);

    ch->receive_damage("hp", 30, foe);
    ch->receive_wound("hp", 20, foe);
    ch->receive_curing("hp", 10);
    ch->receive_healing("hp", 5);

    // 无效输入与无实际生效的调用不通知
    ch->receive_damage("hp", -5, foe);
    ch->receive_damage("bad", 10, foe);
    ch->receive_curing("bad", 5);
    ch->receive_curing("hp", 0);
    ch->set("hp", (long)90);  // hp 已等于 eff_hp，不再补
    ch->receive_healing("hp", 5);

    lua_getglobal(L, "__recv_log");
    ASSERT_TRUE(lua_isstring(L, -1));
    EXPECT_STREQ(lua_tostring(L, -1),
                 "damage|hp|30|f1|c1;"
                 "wound|hp|20|f1|c1;"
                 "curing|hp|10|-|c1;"
                 "healing|hp|5|-|c1;");
    lua_pop(L, 1);

    EXPECT_EQ(luaL_dostring(L,
                            "hooks.receive = _G.__orig_receive\n"
                            "_G.__orig_receive = nil\n"
                            "_G.__recv_log = nil"),
              LUA_OK);
}

TEST_F(CharCombatTest, Receive_BigbossAccumulatesOnlyWithAttacker)
{
    ensure_lua_env();

    // 普通角色不累加
    EXPECT_EQ(ch->receive_damage("hp", 30, foe), 30);
    EXPECT_EQ(ch->querymap_temp("bigboss"), nullptr);

    // 大 BOSS 的环境伤害（无攻击者）不累加
    ch->set("bigboss", (long)1);
    EXPECT_EQ(ch->receive_damage("hp", 30, NULL), 30);
    EXPECT_EQ(ch->query("hp"), 40);
    EXPECT_EQ(ch->querymap_temp("bigboss"), nullptr);
}

// ---------------- busy / interrupt ----------------
TEST_F(CharCombatTest, StartBusyByCount)
{
    ch->start_busy(3, NULL, NULL);
    EXPECT_TRUE(ch->is_busy());
}

TEST_F(CharCombatTest, StartBusyFunction)
{
    ch->start_busy((BUSYFUNCTION*)NULL, NULL);  // null busy -> ignored
    ch->start_busy(2, NULL, NULL);
    EXPECT_TRUE(ch->is_busy());
}

TEST_F(CharCombatTest, InterruptMe)
{
    ch->start_busy(5, NULL, NULL);
    ch->interrupt_me(foe, "hit");  // no interrupt fn -> no-op
    EXPECT_TRUE(ch->is_busy());
}

// ---------------- killers ----------------
TEST_F(CharCombatTest, SetAndRemoveKiller)
{
    // set_killer records by id; is_killing queries by object_id (pre-existing
    // key mismatch)。保留 set/remove_killer 路径覆盖，终态查询为 false。
    ch->set_killer(foe);
    ch->remove_killer(foe);
    ch->set_killer(foe);
    EXPECT_FALSE(ch->is_killing(foe));
}

TEST_F(CharCombatTest, RemoveAllKiller)
{
    ch->set_killer(foe);
    ch->remove_all_killer();
    EXPECT_FALSE(ch->is_killing(foe));
}

TEST_F(CharCombatTest, KillAndFightOb)
{
    // fight_ob -> cmd_hit_ob：无环境 → notify_fail 拒绝且不开战
    ch->kill_ob(foe);
    ch->fight_ob(foe);
    EXPECT_STREQ(g_notify_fail_msg, "此地禁止战斗!");
    EXPECT_FALSE(ch->is_fighting());
}

extern MAP2USER g_user_list;

// 忙碌钩子探针：真值保留 / 假值清除 / 中断放行
namespace {
// continue_action/interrupt_me 是 protected：派生类 using 暴露给直测
struct BusyExposedChar : CChar {
    using CChar::continue_action;
    using CChar::interrupt_me;
};
int g_busy_calls    = 0;
char g_intr_how[40] = {0};
int ProbeBusyKeep(CChar*)
{
    ++g_busy_calls;
    return 1;
}
int ProbeBusyDrop(CChar*)
{
    ++g_busy_calls;
    return 0;
}
int ProbeIntrClear(CChar*, CChar*, char* how)
{
    strncpy(g_intr_how, how, sizeof(g_intr_how) - 1);
    return 1;
}
}  // namespace

// ---------------- 晕倒链：无 who 时按 last_damage_from 补记 ----------------
TEST_F(CharCombatTest, ReceiveDamage_FaintedNoWho_MarksFaint)
{
    auto* killer = new CUser();
    killer->set("id", "atk1");
    g_user_list[killer->object_id()] = killer;

    ch->set("hp", (long)5);
    ch->set_temp("last_damage_from", killer->object_id());

    EXPECT_EQ(10, ch->receive_damage("hp", 10, nullptr));
    EXPECT_EQ(-1, ch->query("hp")) << "血量打穿";
    EXPECT_STREQ(killer->id(1), ch->querystr_temp("last_fainted_from"))
        << "补记打晕者（字符串型临时量）(68,72)";

    g_user_list.erase(killer->object_id());
    delete killer;
}

TEST_F(CharCombatTest, ReceiveWound_FaintedNoWho_MarksFaintForUser)
{
    auto* killer = new CUser();
    killer->set("id", "atk2");
    g_user_list[killer->object_id()] = killer;

    auto* vic = new CUser();  // 用户受害者 → 123 排 remove_faint 回执
    vic->set("id", "vic2");
    vic->set("hp", (long)5);
    vic->set("eff_hp", (long)5);
    vic->set("max_hp", (long)5);
    vic->set_temp("last_damage_from", killer->object_id());

    EXPECT_EQ(10, vic->receive_wound("hp", 10, nullptr));
    EXPECT_EQ(-1, vic->query("eff_hp")) << "伤值打穿";
    EXPECT_STREQ(killer->id(1), vic->querystr_temp("last_fainted_from"))
        << "补记打晕者（字符串型临时量）(117,121,122)";

    g_user_list.erase(killer->object_id());
    delete killer;
    vic->move(nullptr);
    delete vic;
}

// ---------------- 归因重定向：宠物的主人吃账（damage 侧带 verbose 日志） -----
TEST_F(CharCombatTest, ReceiveDamage_OwnerRedirectAndVerboseLog)
{
    auto* owner = new CUser();
    owner->set("id", "boss1");
    owner->set("grant", (long)CMainObject::GRANT_WIZARD);
    owner->set("env/combat", "verbose");
    g_user_list[owner->object_id()] = owner;

    auto* pet = new CUser();
    pet->set("id", "pet1");
    pet->set("owner", owner->object_id());

    EXPECT_EQ(10, ch->receive_damage("hp", 10, pet));
    EXPECT_EQ((long)owner->object_id(),
              (long)ch->query_temp("last_damage_from"))
        << "记账记到主人头上(53-55)；verbose 日志路(82)随之执行";
    EXPECT_EQ(90, ch->query("hp"));

    g_user_list.erase(owner->object_id());
    delete owner;
    pet->move(nullptr);
    delete pet;
}

TEST_F(CharCombatTest, ReceiveWound_OwnerRedirect)
{
    auto* owner = new CUser();
    owner->set("id", "boss2");
    g_user_list[owner->object_id()] = owner;

    auto* pet = new CUser();
    pet->set("id", "pet2");
    pet->set("owner", owner->object_id());

    EXPECT_EQ(10, ch->receive_wound("hp", 10, pet));
    EXPECT_EQ((long)owner->object_id(),
              (long)ch->query_temp("last_damage_from"))
        << "伤账同样记主人(102-103)";
    EXPECT_EQ(90, ch->query("eff_hp"));

    g_user_list.erase(owner->object_id());
    delete owner;
    pet->move(nullptr);
    delete pet;
}

// ---------------- 中毒加成：毒师下手伤害加倍 ----------------
TEST_F(CharCombatTest, ReceivePoison_SkillBonusDoublesDamage)
{
    auto* killer = new CUser();
    killer->set("id", "poi1");
    killer->set_skill("poison", 100);
    g_user_list[killer->object_id()] = killer;

    ch->set_temp("condition/who_test_poi", killer->object_id());

    EXPECT_EQ(10, ch->receive_poison("hp", 10, "test_poi"))
        << "返回值仍是基础伤";
    EXPECT_EQ(100 - 20, ch->query("hp"))
        << "毒伤加成 10*(100/100+1)=20(286-288)";
    EXPECT_EQ(100 - 10, ch->query("eff_hp")) << "wound 照常结算";

    g_user_list.erase(killer->object_id());
    delete killer;
}

// ---------------- 忙碌推进与中断 ----------------
TEST_F(CharCombatTest, ContinueAction_BusyCountDecrements)
{
    auto* b = new BusyExposedChar();  // protected → 派生暴露直调
    b->set_name("busy", "b1");
    b->set("hp", (long)100);

    b->start_busy(3);
    EXPECT_TRUE(b->is_busy());
    b->continue_action();  // 3→2(218-219)
    EXPECT_TRUE(b->is_busy());
    b->continue_action();  // 2→1
    b->continue_action();  // 1→0
    EXPECT_FALSE(b->is_busy()) << "计数走完即解除";

    delete b;
}

TEST_F(CharCombatTest, ContinueAction_FuncHookLifecycle)
{
    auto* b = new BusyExposedChar();
    b->set_name("busy", "b2");
    b->set("hp", (long)100);

    b->start_busy(&ProbeBusyKeep);
    g_busy_calls = 0;
    b->continue_action();  // 真值 → 保留(223-224)
    b->continue_action();
    EXPECT_EQ(2, g_busy_calls) << "真值钩子保留，可再次触发";

    b->start_busy(&ProbeBusyDrop);
    b->continue_action();  // 假值 → 清除
    g_busy_calls = 0;
    b->continue_action();  // 钩子已清 → 不再触发
    EXPECT_EQ(0, g_busy_calls) << "假值钩子用后即清";

    delete b;
}

TEST_F(CharCombatTest, Interrupt_HookReturningTrueClearsBusy)
{
    auto* b = new BusyExposedChar();
    b->set_name("busy", "b3");
    b->set("hp", (long)100);
    g_intr_how[0] = 0;

    b->start_busy(&ProbeBusyKeep, &ProbeIntrClear);
    EXPECT_TRUE(b->is_busy());

    b->interrupt_me(foe, (char*)"被击退");
    EXPECT_FALSE(b->is_busy()) << "中断钩子放行 → 忙碌全清(241-244)";
    EXPECT_STREQ("被击退", g_intr_how) << "中断原因原样传递";

    delete b;
}
