// test_char_state.cpp - Unit tests for CharState.cpp (死亡/苏醒/状态广播).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "test_char_common.h"
#include "test_state_recorder.h"
#include "test_lua_env.h"

class CharStateTest : public ::testing::Test {
protected:
    CChar* ch;
    void SetUp() override
    {
        ensure_lua_env();
        ch = new CChar();
        ch->set("name", "d");
        ch->set("id", "d1");
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

// ---------------- unconcious ----------------
TEST_F(CharStateTest, Unconcious)
{
    ch->unconcious();
    EXPECT_STREQ(ch->querystr_temp("disabled"), "昏迷不醒");
    EXPECT_EQ(ch->query("hp"), 0);
    EXPECT_EQ(ch->query("mp"), 0);
}

TEST_F(CharStateTest, Unconcious_NotLiving)
{
    ch->disable_player("x");  // IS_LIVING -> 0
    ch->unconcious();         // early return
    EXPECT_STREQ(ch->querystr_temp("disabled"), "x");
}

// ---------------- revive ----------------
TEST_F(CharStateTest, Revive)
{
    CContainer* room = new CContainer();
    ch->move(room);
    ch->disable_player("昏迷不醒");
    ch->revive(0);
    EXPECT_EQ(ch->querystr_temp("disabled")[0], '\0');
    ch->move(nullptr);
    delete room;
}

TEST_F(CharStateTest, ReviveQuiet)
{
    CContainer* room = new CContainer();
    ch->move(room);
    ch->set_temp("block_msg/all", (long)1);
    ch->revive(1);  // quiet
    EXPECT_EQ(ch->query_temp("block_msg/all"), 0);
    ch->move(nullptr);
    delete room;
}

// ---------------- GetStateMsg ----------------
TEST_F(CharStateTest, GetStateMsg_Basic)
{
    std::string msg = "&C=1&T=1&";
    ch->get_state_msg(msg);
    EXPECT_NE(msg.find("tea=-1&"), std::string::npos);  // not fighting
    EXPECT_NE(msg.find("sta="), std::string::npos);
}

TEST_F(CharStateTest, GetStateMsg_FightingAndFlags)
{
    ch->set_temp("fight/is_fighting", (long)1);
    ch->set_temp("fight/fight_team", (long)2);
    ch->set_temp("fight/ap", (long)5);
    ch->set_temp("pending/exercising", (long)1);
    ch->set_temp("net_dead", (long)1);
    ch->apply_condition("poison_x", 5, 0);
    std::string msg = "&C=1&T=1&";
    ch->get_state_msg(msg);
    EXPECT_NE(msg.find("tea=1&"), std::string::npos);
    EXPECT_NE(msg.find("exe=1&"), std::string::npos);
    EXPECT_NE(msg.find("net=1&"), std::string::npos);
}

TEST_F(CharStateTest, GetStateMsg_Disabled)
{
    ch->disable_player("busy");
    std::string msg = "&C=1&T=1&";
    ch->get_state_msg(msg);
    EXPECT_NE(msg.find("dis="), std::string::npos);
}

// ---------------- SendState ----------------
// broadcast=true：房间内其他玩家收到状态标志精简包（含 tea=，不含 hp=/att=）。
TEST_F(CharStateTest, SendState_Broadcast_ReachesOtherUsers)
{
    CContainer room;
    StateRecordingUser actor;
    StateRecordingUser viewer;
    actor.set_name("actor", "a1");
    viewer.set_name("viewer", "v1");
    actor.set("hp", (long)50);
    actor.set("max_hp", (long)100);
    actor.set_temp("client/serial", 1);  // 已建立客户端连接，才允许私发
    actor.move(&room);
    viewer.move(&room);
    actor.clear();
    viewer.clear();  // 隔离出 send_state 的效果，排除 setup move 触发的
                     // on_add_object

    actor.send_state(true);

    EXPECT_TRUE(viewer.has("tea="));   // 广播送达其他玩家
    EXPECT_FALSE(viewer.has("att="));  // 精简包不含攻防
}

// broadcast=false：不广播，其他玩家收不到；自己（玩家）收到含攻防的完整包。
TEST_F(CharStateTest, SendState_NoBroadcast_SuppressesOthers)
{
    CContainer room;
    StateRecordingUser actor;
    StateRecordingUser viewer;
    actor.set_name("actor", "a1");
    viewer.set_name("viewer", "v1");
    actor.set("hp", (long)50);
    actor.set("max_hp", (long)100);
    actor.set_skill("unarmed", 50);
    actor.set_temp("client/serial", 1);  // 已建立客户端连接，才允许私发
    actor.move(&room);
    viewer.move(&room);
    actor.clear();
    viewer.clear();  // 隔离出 send_state 的效果，排除 setup move 触发的
                     // on_add_object

    actor.send_state(false);

    EXPECT_EQ(viewer.sent, 0);       // 未广播
    EXPECT_TRUE(actor.has("att="));  // 自身收到完整包
    EXPECT_TRUE(actor.has("hp="));
}

// NPC 广播：仍向房间内玩家送达状态标志（守卫2 位于广播之后，不影响广播）。
TEST_F(CharStateTest, SendState_Npc_Broadcast_ReachesOtherUsers)
{
    CContainer room;
    StateRecordingUser viewer;
    viewer.set_name("viewer", "v1");
    ch->set("max_hp", (long)100);
    ch->move(&room);
    viewer.move(&room);
    viewer.clear();  // 隔离出 send_state 的效果，排除 setup move 触发的
                     // on_add_object

    ch->send_state(true);

    EXPECT_TRUE(viewer.has("tea="));
    ch->move(nullptr);
}

// NPC 非广播：既无广播也无自身投递（守卫1 直接返回，全程无产出）。
TEST_F(CharStateTest, SendState_Npc_NoBroadcast_SuppressesOthers)
{
    CContainer room;
    StateRecordingUser viewer;
    viewer.set_name("viewer", "v1");
    ch->move(&room);
    viewer.move(&room);
    viewer.clear();  // 隔离出 send_state 的效果，排除 setup move 触发的
                     // on_add_object

    ch->send_state(false);

    EXPECT_EQ(viewer.sent, 0);
    ch->move(nullptr);
}

// 投递能力而非身份：未建立 client 连接、或已掉线的玩家，同样不该拼装私发包。
// 这两种状态用 userp()（只看 IS_USER）判不出，旧写法会给出相反结论。
// 注意：不进房间——玩家 move() 会经 change_room 设置 client/loading 键，
// 那之后就已经"有客户端"了，得不到这里的初始状态。
TEST_F(CharStateTest, SendState_UserNoEndpoint_NoPrivate)
{
    StateRecordingUser actor;
    actor.set_name("actor", "a1");
    actor.set("max_hp", (long)100);
    actor.set_skill("unarmed", 50);

    // 未建立客户端连接：不拼装、不投递
    actor.send_state(false);
    EXPECT_EQ(actor.sent, 0);

    // 连接已建立：私发完整状态包
    actor.set_temp("client/serial", 1);
    actor.clear();
    actor.send_state(false);
    EXPECT_GT(actor.sent, 0);
    EXPECT_TRUE(actor.has("att="));

    // 已掉线：连接形同虚设，同样不投递
    actor.set_temp("net_dead", (long)1);
    actor.clear();
    actor.send_state(false);
    EXPECT_EQ(actor.sent, 0);
}

// ---------------- die: 救命金丹 branch ----------------
TEST_F(CharStateTest, Die_Jindan)
{
    CContainer* room = new CContainer();
    room->set("PK", (long)1);
    ch->move(room);
    ch->set("kill/flee", (long)3);
    ch->set("kill/death", (long)0);
    ch->set("start_city", "rk");
    ch->set("combat_exp", (long)1000);
    ch->die();  // 救命金丹 -> DropAll + move(start_city), return
    EXPECT_EQ(ch->query("kill/death"), 1);
    ch->move(nullptr);
    delete room;
}

// ---------------- die: user full death ----------------
TEST_F(CharStateTest, Die_User)
{
    CharMockDb* db = new CharMockDb();
    g_db           = db;
    CUser* u       = new CUser();
    u->set("id", "d3");
    u->set("name", "玩家");
    u->set("hp", (long)1);
    u->set("eff_hp", (long)1);
    u->set("max_hp", (long)100);
    u->set("mp", (long)10);
    u->set("max_mp", (long)10);
    u->set("con", (long)20);
    u->set("combat_exp", (long)100000);
    u->set("level", (long)10);
    u->set("start_city", "rk");
    u->set_skill("unarmed", 50);
    CContainer* room = new CContainer();
    room->set("PK", (long)1);  // skip MakeCorpse
    u->move(room);
    u->die();  // full user death path
    EXPECT_EQ(u->query("hp"), 1);
    u->move(nullptr);
    delete u;
    delete room;
    g_db = nullptr;
    delete db;
}

#include "Global.h"              // g_room_list / g_start_room
#include "mock/mock_database.h"  // flush 链的 DB 保险

extern IDatabase* g_db;

// ---------------- die：副本免死回城支（fb_nodie） ----------------

TEST_F(CharStateTest, DieFbNoDieReturnsHomeWithHalfHp)
{
    MockDatabase db;
    g_db = &db;

    CRoom home;
    home.set_name("主城", "fb_home");
    home.set("PK", (long)1);  // die 用例惯例：PK 房跳过和平广播段
    CRoom grave;
    grave.set_name("副本", "fb_grave");
    grave.set("PK", (long)1);
    g_room_list["fb_home"]  = &home;
    g_room_list["fb_grave"] = &grave;

    CUser* u = new CUser();
    u->set_name("副本客", "fbhero");
    u->set("hp", (long)30);
    u->set("eff_hp", (long)30);
    u->set("max_hp", (long)100);
    u->set("mp", (long)50);
    u->set("max_mp", (long)50);
    u->set("start_city", "fb_home");
    u->move(&grave);
    u->set_temp("fb_nodie", 1);  // move 会清 fb_nodie → 入房后打标
    u->set_temp("team/dungeon", 1);

    u->die();

    EXPECT_EQ(1, u->query("hp")) << "免死保命(45)";
    EXPECT_EQ(50, u->query("eff_hp")) << "半血回复(46)";
    EXPECT_EQ(0, u->query("mp")) << "清空法力(47)";
    EXPECT_EQ(0, u->query_temp("team/dungeon")) << "清副本队伍(49)";
    EXPECT_EQ(static_cast<CContainer*>(&home), u->environment())
        << "回 start_city(51,52)";

    u->move(nullptr);
    delete u;
    g_room_list.erase("fb_home");
    g_room_list.erase("fb_grave");
    g_db = nullptr;
}

extern MAP2USER g_user_list;

// ---------------- die：击杀奖励链（88-105） ----------------

TEST_F(CharStateTest, DieFightOwnerTakesKillRewardCredit)
{
    MockDatabase db;
    g_db = &db;

    CUser* killer = new CUser();
    killer->set_name("打手", "thug1");
    killer->set("id", "thug1");
    g_user_list[killer->object_id()] = killer;

    CUser* owner = new CUser();
    owner->set_name("元凶", "master1");
    owner->set("id", "master1");
    owner->set("level", (long)10);
    owner->set("combat_exp", (long)1000);
    owner->set("con", (long)20);
    g_user_list[owner->object_id()] = owner;

    CUser* victim = new CUser();
    victim->set_name("受害者", "victim1");
    victim->set("id", "victim1");
    victim->set("hp", (long)1);
    victim->set("eff_hp", (long)1);
    victim->set("max_hp", (long)100);
    victim->set("mp", (long)10);
    victim->set("max_mp", (long)10);
    victim->set("level", (long)10);
    victim->set("combat_exp", (long)1000);
    victim->set("con", (long)20);
    victim->set("start_city", "no_such_city_kr");
    CContainer* room = new CContainer();
    room->set("PK", (long)1);  // die 用例惯例：跳过尸体
    victim->move(room);

    victim->set_temp("last_damage_from", killer->object_id());
    victim->set_temp("fight/owner", owner->object_id());
    owner->set_temp("fight/will_nk", victim->object_id());

    victim->die();

    EXPECT_STREQ("master1", victim->querystr("kill/last_killer"))
        << "战报 owner 转嫁记账(88-89,99)";
    EXPECT_EQ(1, victim->query("pds")) << "PK 败场 +1(handle_pk_reward)";
    EXPECT_EQ(1, victim->query("hp")) << "完整死亡保命(121)";
    EXPECT_FALSE(db.GetLastSql().empty()) << "击杀者/死者落库(102,119)";

    victim->move(nullptr);
    delete victim;
    delete room;
    g_user_list.erase(killer->object_id());
    delete killer;
    g_user_list.erase(owner->object_id());
    delete owner;
    g_db = nullptr;
}

TEST_F(CharStateTest, DieKillerWithOwnerRedirectsToOnlineOwner)
{
    MockDatabase db;
    g_db = &db;

    CUser* owner = new CUser();
    owner->set_name("主人家", "boss1");
    owner->set("id", "boss1");
    owner->set("level", (long)10);
    owner->set("combat_exp", (long)1000);
    owner->set("con", (long)20);
    g_user_list[owner->object_id()] = owner;

    CUser* killer = new CUser();  // 宠物式凶手：挂 owner
    killer->set_name("恶犬", "hound1");
    killer->set("id", "hound1");
    killer->set("owner", "boss1");
    g_user_list[killer->object_id()] = killer;

    CUser* victim = new CUser();
    victim->set_name("倒霉蛋", "victim2");
    victim->set("id", "victim2");
    victim->set("hp", (long)1);
    victim->set("eff_hp", (long)1);
    victim->set("max_hp", (long)100);
    victim->set("mp", (long)10);
    victim->set("max_mp", (long)10);
    victim->set("level", (long)10);
    victim->set("combat_exp", (long)1000);
    victim->set("con", (long)20);
    victim->set("start_city", "no_such_city_kr2");
    CContainer* room = new CContainer();
    room->set("PK", (long)1);
    victim->move(room);

    victim->set_temp("last_damage_from", killer->object_id());

    victim->die();

    EXPECT_STREQ("boss1", victim->querystr("kill/last_killer"))
        << "凶手挂 owner → 记账转给在线主人(92-94,99)";
    EXPECT_EQ(1, victim->query("pds")) << "PK 败场 +1";

    victim->move(nullptr);
    delete victim;
    delete room;
    g_user_list.erase(killer->object_id());
    delete killer;
    g_user_list.erase(owner->object_id());
    delete owner;
    g_db = nullptr;
}

// ---------------- revive：从角色容器逐层跳出（162） ----------------

TEST_F(CharStateTest, ReviveEscapesNestedCharContainer)
{
    CContainer room;
    CChar carrier;
    carrier.set_name("背夫", "carrier1");
    carrier.move(&room);

    ch->move(&carrier);       // 人在角色容器内
    ch->disable_player("x");  // 非 living → revive 走
    ch->revive(1);

    EXPECT_EQ(static_cast<CContainer*>(&room), ch->environment())
        << "逐层跳出角色容器落到外层(162)";
    ch->move(nullptr);
}
