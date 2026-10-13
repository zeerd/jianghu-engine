// test_bind_char_combat.cpp — binds in CharCombat.cpp + CharState.cpp +

// CharFight.cpp + Char.cpp + CharFlush.cpp + CharSave.cpp

#include "test_bind_common.h"

#include "User.h"

#include "mock/mock_database.h"  // DB 保险

#include "Global.h"  // WIELD_HAND

#include <windows.h>  // LuaItem.inl 需 UUID（rpc 链经 windows.h）

#include "LuaItem.h"  // CLuaEquip

#include "Skill.h"  // load_skill

#include "test_char_common.h"

#include "test_state_recorder.h"

#include "test_state_recorder.h"

class BindCharCombatTest : public BindTest {
protected:
    CChar ch;

    CChar foe;

    void SetUp() override

    {
        BindTest::SetUp();

        ch.set_name("ch", "ch1");

        foe.set_name("foe", "foe1");
    }
};

// ---- CharCombat.cpp ----

TEST_F(BindCharCombatTest, ReceiveDamageWoundPoisonCuring)

{
    ch.set("hp", (long)100);

    ch.set("eff_hp", (long)100);

    ch.set("max_hp", (long)100);

    // damage 扣 hp，返回所扣数值

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hp");

    lua_pushinteger(L, 30);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(30, CallInt(l_receive_damage, 4));

    EXPECT_EQ(70, ch.query("hp"));

    // wound 扣 eff_hp（hp 不高于新 eff 时不追扣）

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hp");

    lua_pushinteger(L, 5);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(5, CallInt(l_receive_wound, 4));

    EXPECT_EQ(95, ch.query("eff_hp"));

    EXPECT_EQ(70, ch.query("hp"));

    // poison（无投毒者条件）→ receive_wound：eff_hp 再扣 2

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hp");

    lua_pushinteger(L, 2);

    lua_pushstring(L, "");

    EXPECT_EQ(2, CallInt(l_receive_poison, 4));

    EXPECT_EQ(93, ch.query("eff_hp"));

    // curing 补 eff_hp，封顶 max_hp → applied = 7

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hp");

    lua_pushinteger(L, 10);

    EXPECT_EQ(7, CallInt(l_receive_curing, 3));

    EXPECT_EQ(100, ch.query("eff_hp"));

    // healing 补 hp（hp < eff_hp 才生效）→ +10

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hp");

    lua_pushinteger(L, 10);

    EXPECT_EQ(10, CallInt(l_receive_healing, 3));

    EXPECT_EQ(80, ch.query("hp"));
}

TEST_F(BindCharCombatTest, StartBusyIsBusy)

{
    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 3);

    CallVoid(l_start_busy, 2);

    lua_bind_push_object(L, &ch);

    EXPECT_TRUE(CallBool(l_is_busy, 1));
}

TEST_F(BindCharCombatTest, Powerup)

{
    // start_powerup 只接受 CSkill 作 self（CharCombat.cpp dynamic_cast 守卫）；

    // 普通 CChar 被拦下、不 push 返回值

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 5);

    lua_pushinteger(L, 10);

    lua_pushinteger(L, 2);

    lua_pushstring(L, "power");

    EXPECT_EQ(0, CallRetCount(l_start_powerup, 5));

    // remove_powerup 是静态清理：void 无返回、pcall 不抛

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_remove_powerup, 1));
}

TEST_F(BindCharCombatTest, KillSetFightKilling)

{
    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(0, CallRetCount(l_set_killer, 2));

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(0, CallRetCount(l_fight_ob, 2));

    // is_killing 返回一个布尔（值依赖杀手表内部键，不断言具体值）

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(1, CallRetCount(l_is_killing, 2));

    // kill_ob(target) 只取第 2 个 userdata 参数

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &foe);

    EXPECT_EQ(0, CallRetCount(l_kill_ob, 2));
}

TEST_F(BindCharCombatTest, RemoveAllKiller)

{
    // 契约：remove_all_killer 同时清 last_damage_from

    ch.set_temp("last_damage_from", (long)42);

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_remove_all_killer, 1));

    EXPECT_EQ(0, ch.query_temp("last_damage_from"));
}

// ---- CharState.cpp ----

TEST_F(BindCharCombatTest, Unconcious)

{
    ch.set("eff_hp", (long)100);

    ch.set("max_hp", (long)100);

    lua_bind_push_object(L, &ch);

    CallVoid(l_unconcious, 1);

    EXPECT_STREQ(ch.querystr_temp("disabled"), "昏迷不醒");
}

TEST_F(BindCharCombatTest, ReviveNeedsRoom)

{
    CContainer room;

    ch.move(&room);

    ch.disable_player("昏迷不醒");

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 0);

    CallVoid(l_revive, 2);

    EXPECT_EQ('\0', ch.querystr_temp("disabled")[0]);

    ch.move(nullptr);
}

TEST_F(BindCharCombatTest, SendState_NoArg_Broadcasts)

{
    CContainer room;

    StateRecordingUser* actor = new StateRecordingUser();

    StateRecordingUser* viewer = new StateRecordingUser();

    actor->set_name("actor", "ba1");

    viewer->set_name("viewer", "bv1");

    actor->set("max_hp", (long)100);

    actor->move(&room);

    viewer->move(&room);

    actor->clear();

    viewer->clear();  // 隔离出 send_state 的效果，排除 setup move 触发的

    // on_add_object

    actor->set_temp("client/serial", 1);  // 已建立客户端连接，才允许私发

    lua_bind_push_object(L, actor);

    CallVoid(l_send_state, 1);  // 无第二参数 -> 广播

    EXPECT_TRUE(viewer->has("tea="));  // 房间内其他玩家收到广播

    actor->move(nullptr);

    viewer->move(nullptr);

    delete actor;

    delete viewer;
}

TEST_F(BindCharCombatTest, SendState_WithArg_NoBroadcast)

{
    CContainer room;

    StateRecordingUser* actor = new StateRecordingUser();

    StateRecordingUser* viewer = new StateRecordingUser();

    actor->set_name("actor", "ba1");

    viewer->set_name("viewer", "bv1");

    actor->set("max_hp", (long)100);

    actor->move(&room);

    viewer->move(&room);

    actor->clear();

    viewer->clear();  // 隔离出 send_state 的效果，排除 setup move 触发的

    // on_add_object

    actor->set_temp("client/serial", 1);  // 已建立客户端连接，才允许私发

    lua_bind_push_object(L, actor);

    lua_bind_push_object(L, actor);  // 传入第二参数 -> 非广播

    CallVoid(l_send_state, 2);

    EXPECT_EQ(viewer->sent, 0);  // 未向其他玩家广播

    EXPECT_TRUE(actor->has("att="));  // 自身仍收到完整状态包

    actor->move(nullptr);

    viewer->move(nullptr);

    delete actor;

    delete viewer;
}

// ---- CharFight.cpp ----

TEST_F(BindCharCombatTest, QueryWeaponAndFighting)

{
    // 未装备武器 → nil（lua_bind_push_object 对 NULL 压 nil）

    lua_bind_push_object(L, &ch);

    int k = call(l_query_weapon, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &foe);

    EXPECT_FALSE(CallBool(l_is_fighting_target, 2));

    lua_bind_push_object(L, &ch);

    k = call(l_query_left_weapon, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);
}

// ---- Char.cpp ----

TEST_F(BindCharCombatTest, StateQueries)

{
    // 裸角色初始态：未战斗、不忙、无状态、无队伍

    lua_bind_push_object(L, &ch);

    EXPECT_FALSE(CallBool(l_is_fighting, 1));

    lua_bind_push_object(L, &ch);

    EXPECT_FALSE(CallBool(l_is_busy, 1));

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "poison");

    EXPECT_EQ(0, CallInt(l_query_condition, 2));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallInt(l_query_team, 1));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_clear_condition, 1));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_reset_level, 1));
}

TEST_F(BindCharCombatTest, CommandNoHandler)

{
    // 契约：command 返回是否成功；无环境角色不可发命令 → false

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "nosuchcmd");

    EXPECT_FALSE(CallBool(l_command, 2));
}

TEST_F(BindCharCombatTest, EnableDisablePlayer)

{
    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "test disable");

    EXPECT_EQ(0, CallRetCount(l_disable_player, 2));

    EXPECT_STREQ("test disable", ch.querystr_temp("disabled"));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_enable_player, 1));

    EXPECT_EQ('\0', ch.querystr_temp("disabled")[0]);
}

// ---- CharFlush.cpp ----

TEST_F(BindCharCombatTest, FlushBindsForwardToFlush)

{
    // 绑定必须把调用转发到 CChar::flush_*（协议内容契约归 test_char_flush）：

    // 记录型玩家建立空连接后，经 Lua 包装调用应产出协议包

    StateRecordingUser* u = new StateRecordingUser();

    u->set("id", "bind_fl");

    u->set("name", "绑定");

    u->set_skill("unarmed", 50);

    u->set_temp("client/serial", 0);

    lua_bind_push_object(L, u);

    CallVoid(l_flush_my_info, 1);

    EXPECT_GT(u->sent, 0);

    u->clear();

    lua_bind_push_object(L, u);

    CallVoid(l_flush_my_skill, 1);  // this == me

    EXPECT_GT(u->sent, 0);

    delete u;
}

// ---- CharSave.cpp (uses mock db) ----

TEST_F(BindCharCombatTest, SaveLoadOffline)

{
    CharMockDb db;

    g_db = &db;

    CUser u;

    u.set("id", "save1");

    u.set("name", "saver");

    u.set("gender", "男性");

    u.set("combat_exp", (long)1000);

    u.set_temp("client/loading", 1);

    // save 必须真的把 SQL 交给 db 层（CharMockDb 记录最后一条）

    lua_bind_push_object(L, &u);

    CallInt(l_save_to_server, 1);

    EXPECT_FALSE(db.LastSql().empty());

    lua_bind_push_object(L, &u);

    EXPECT_EQ(1, CallRetCount(l_load_from_server, 1));

    // offline 清除载入标记（User.cpp: delete client/loading）

    lua_bind_push_object(L, &u);

    EXPECT_EQ(0, CallRetCount(l_offline, 1));

    EXPECT_EQ(0, u.query_temp("client/loading"));

    g_db = nullptr;
}

// ---- l_do_wield / l_unwield_weapon / l_start_powerup 深路 ----

TEST_F(BindCharCombatTest, DoWieldBind_FindsEquipByName)

{
    ASSERT_NE(ensure_lua_env(), nullptr);  // CLuaEquip 走全局引擎

    auto* eq = new CLuaEquip("ssword");

    eq->del("equipped");  // 构造/脚本可能带装备态：镜像 SetupObjects 的清态

    eq->set_name("长剑", "bw_sword");

    eq->set("id", "bw_sword");

    CMapping apply;

    apply.set("damage", 1L);

    apply.set("desc", "攻击");

    eq->set("apply", apply);

    eq->set("wield_position", (long)WIELD_HAND);  // do_wield 拒 WIELD_NONE

    eq->move(&ch);

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "bw_sword");

    EXPECT_EQ(1, CallInt(l_wield_weapon, 2)) << "命中装备并装备(307-312)";

    EXPECT_NE(0, (long)eq->query("equipped")) << "槽位登记";

    EXPECT_EQ(1, ch.query_temp("apply/damage")) << "apply 增益入体";

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "no_such_item");

    EXPECT_EQ(0, CallInt(l_wield_weapon, 2)) << "未命中 → 0(315)";

    eq->move(nullptr);

    delete eq;
}

TEST_F(BindCharCombatTest, UnwieldWeaponBind_ClearsEquip)

{
    ASSERT_NE(ensure_lua_env(), nullptr);

    auto* eq = new CLuaEquip("ssword");

    eq->del("equipped");  // 构造/脚本可能带装备态：镜像 SetupObjects 的清态

    eq->set_name("长剑", "bw_sword2");

    eq->set("id", "bw_sword2");

    CMapping apply;

    apply.set("damage", 1L);

    apply.set("desc", "攻击");

    eq->set("apply", apply);

    eq->set("wield_position", (long)WIELD_HAND);

    eq->move(&ch);

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "bw_sword2");

    ASSERT_EQ(1, CallInt(l_wield_weapon, 2));  // 前置：真装备

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "bw_sword2");

    EXPECT_EQ(1, CallInt(l_unwield_weapon, 2)) << "按名卸装(327-332)";

    EXPECT_EQ(0, (long)eq->query("equipped")) << "槽位登记已清";

    EXPECT_EQ(0, ch.query_temp("apply/damage")) << "apply 增益回收";

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "no_such_item");

    EXPECT_EQ(0, CallInt(l_unwield_weapon, 2)) << "未命中 → 0(335)";

    eq->move(nullptr);

    delete eq;
}

TEST_F(BindCharCombatTest, StartPowerupBind_AppliesBuffs)

{
    ASSERT_NE(ensure_lua_env(), nullptr);

    CSkill* sk = load_skill("force");

    ASSERT_NE(sk, nullptr);

    lua_bind_push_object(L, sk);

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 60);

    lua_pushinteger(L, 5);

    lua_pushinteger(L, 7);

    lua_pushstring(L, "威风凛凛");

    CallVoid(l_start_powerup, 6);

    EXPECT_EQ(5, ch.query_temp("powerup/attack")) << "记录可回收的增益量(451)";

    EXPECT_EQ(7, ch.query_temp("powerup/dodge"));

    EXPECT_STREQ("威风凛凛", ch.querystr_temp("powerup/msg"));

    EXPECT_EQ(5, ch.query_temp("apply/attack")) << "增益即时生效";

    EXPECT_EQ(7, ch.query_temp("apply/dodge"));
}

// ---- l_die / l_die_base：经免死快路（fb_nodie）执行完整 die ----

TEST_F(BindCharCombatTest, DieBindsRunFbNoDieFastPath)

{
    ASSERT_NE(ensure_lua_env(), nullptr);

    MockDatabase mdb;

    extern IDatabase* g_db;

    g_db = &mdb;

    CRoom home;

    home.set_name("主城", "die_home");

    CRoom grave;

    grave.set_name("副本", "die_grave");

    grave.set("PK", (long)1);

    g_room_list["die_home"] = &home;

    g_room_list["die_grave"] = &grave;

    auto* u = new CUser();

    u->set_name("敢死者", "daredevil");

    u->set("hp", (long)9);

    u->set("eff_hp", (long)9);

    u->set("max_hp", (long)100);

    u->set("mp", (long)50);

    u->set("max_mp", (long)50);

    u->set("start_city", "die_home");

    u->move(&grave);

    u->set_temp("fb_nodie", 1);  // move 清 fb_nodie → 入房后打标

    lua_bind_push_object(L, u);

    CallVoid(l_die, 1);

    EXPECT_EQ(1, u->query("hp")) << "l_die 执行 die(307-314)";

    EXPECT_EQ(static_cast<CContainer*>(&home), u->environment())

        << "l_die 走免死回城";

    u->move(&grave);

    u->set("hp", (long)9);

    u->set_temp("fb_nodie", 1);  // 二次入房同样重打标

    lua_bind_push_object(L, u);

    CallVoid(l_die_base, 1);

    EXPECT_EQ(1, u->query("hp")) << "l_die_base 执行 CChar::die(318-325)";

    u->move(nullptr);

    delete u;

    g_room_list.erase("die_home");

    g_room_list.erase("die_grave");

    g_db = nullptr;
}

// ---- l_do_bishi null 守卫 / l_query_left_weapon 回推 ----

TEST_F(BindCharCombatTest, DoBishiBindNullArgsPushNil)

{
    ASSERT_NE(ensure_lua_env(), nullptr);

    CRoom not_a_char;

    not_a_char.set_name("非角色", "nochar");

    lua_pushcfunction(L, l_do_bishi);  // fn below args

    lua_bind_push_object(L, &not_a_char);  // me: not a CChar

    lua_bind_push_object(L, &not_a_char);  // victim: not a CChar

    int rc = lua_pcall(L, 2, 1, 0);

    ASSERT_EQ(LUA_OK, rc) << "do_bishi: " << lua_tostring(L, -1);

    EXPECT_TRUE(lua_isnil(L, -1)) << "null 守卫推 nil(418-419)";

    lua_pop(L, 1);
}

TEST_F(BindCharCombatTest, QueryLeftWeaponBindPushesWeapon)

{
    ASSERT_NE(ensure_lua_env(), nullptr);

    auto* blade = new CItem();

    blade->set("id", "bind_leftblade");

    blade->set("skill_type", "sword");

    blade->move(&ch);

    (*ch.get_equip_list())[WIELD_LEFT_HAND] = blade->object_id();

    lua_bind_push_object(L, &ch);

    int k = call(l_query_left_weapon, 1);

    ASSERT_EQ(1, k);

    EXPECT_FALSE(lua_isnil(L, -1)) << "回推左手武器(388)";

    lua_pop(L, 1);

    ch.get_equip_list()->erase(WIELD_LEFT_HAND);

    blade->move(nullptr);

    delete blade;
}

// ---- Skill.cpp: remove_powerup 有状态解除深路(232-240) ----
TEST_F(BindCharCombatTest, RemovePowerupClearsBuffs)
{
    ch.set_temp("apply/attack", 10);
    ch.set_temp("apply/dodge", 7);
    ch.set_temp("powerup/attack", 5);
    ch.set_temp("powerup/dodge", 7);
    ch.set_temp("powerup/msg", "增益消退了");

    CSkill::remove_powerup(&ch, 0, 0);

    EXPECT_EQ(5, ch.query_temp("apply/attack")) << "attack 增益回收(232-233)";
    EXPECT_EQ(0, ch.query_temp("apply/dodge")) << "dodge 增益回收(234)";
    EXPECT_FALSE(ch.querymap_temp("powerup")) << "powerup 态整体清除(239)";
}

// ---- Skill.cpp: valid_effect 基类恒真守卫(173-175) ----
TEST_F(BindCharCombatTest, ValidEffectAlwaysTrue)
{
    CSkill sk;
    char act[] = "strike";
    EXPECT_EQ(1, sk.valid_effect(&ch, nullptr, act, 0))
        << "基类 valid_effect 恒 1(173-175)";
}
