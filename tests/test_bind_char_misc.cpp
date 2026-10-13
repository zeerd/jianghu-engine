// test_bind_char_misc.cpp — remaining char/game binds: emote, setup_char,

// user active/pk, throwing, drop_all, container lifecycle guards.

#include "test_bind_common.h"

#include "User.h"

#include "Equip.h"

#include "Money.h"

#include "Npc.h"  // l_setup CNpc 分支         // CMCoin（现金不落）

#include "test_state_recorder.h"  // StateRecordingUser（add_menu_item 投递观察）

#include "test_lua_env.h"  // drop_all→command() 会触 command_gate 钩子

class BindCharMiscTest : public BindTest {
protected:
    CChar me;

    CContainer env;

    void SetUp() override

    {
        BindTest::SetUp();

        me.set_name("me", "me1");

        me.set("id", "me1");

        me.set("gender", "男性");

        env.set_name("env", "env1");

        me.move(&env);
    }

    void TearDown() override

    {
        me.move(nullptr);

        BindTest::TearDown();
    }
};

// ---- CharLook.cpp: do_emote ----

TEST_F(BindCharMiscTest, DoEmoteNoVerb)

{
    lua_bind_push_object(L, &me);

    lua_pushnil(L);

    lua_pushstring(L, "");  // empty verb -> early return 0

    int k = call(l_do_emote, 3);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);
}

TEST_F(BindCharMiscTest, DoEmoteUnknownVerb)

{
    lua_bind_push_object(L, &me);

    lua_pushnil(L);

    lua_pushstring(L, "notanemote_zzz");

    int k = call(l_do_emote, 3);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);
}

// ---- SetupChar.cpp: setup_char ----

TEST_F(BindCharMiscTest, SetupChar)

{
    lua_bind_push_object(L, &me);

    CallVoid(l_setup_char, 1);

    EXPECT_STREQ("人类", me.querystr("race"));
}

// ---- User.cpp: do_active (safe), ready_pk (needs environment) ----

TEST_F(BindCharMiscTest, do_active)

{
    extern int g_current_time;  // 推进游戏时钟观测 idle

    CUser u;

    u.set_name("u", "u1");

    g_current_time = 1000;

    EXPECT_EQ(1000, u.query_idle());  // 未激活：起点 0

    lua_bind_push_object(L, &u);

    EXPECT_EQ(0, CallRetCount(l_do_active, 1));

    EXPECT_EQ(0, u.query_idle());  // do_active 记录起点 = now

    g_current_time = 0;
}

TEST_F(BindCharMiscTest, ReadyPk)

{
    env.set("city", (int32_t)1);

    env.set("base_name", "foshan");

    CUser u;

    u.set_name("u", "u2");

    u.move(&env);

    lua_bind_push_object(L, &u);

    CallVoid(l_ready_pk, 1);

    // pending/PK is set to current_time (0 in tests) so assert the city,

    // which is stored as a string and is deterministic.

    EXPECT_STREQ("foshan", u.querystr_temp("pending/PK_city"));

    u.move(nullptr);
}

// ---- Throwing.cpp ----

TEST_F(BindCharMiscTest, GetFirstThrowingWeaponNone)

{
    lua_bind_push_object(L, &me);

    int k = call(l_get_first_throwing_weapon, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));  // no throwing weapon equipped

    lua_pop(L, k);
}

TEST_F(BindCharMiscTest, ThrowWeaponGuard)

{
    // weapon (arg 3) absent -> me&&weapon guard skips ThrowWeapon（void

    // 无返回）

    lua_bind_push_object(L, &me);

    lua_bind_push_object(L, &me);

    EXPECT_EQ(0, CallRetCount(l_throw_weapon, 2));
}

// ---- Char.cpp: l_setup 分发（无效/room/npc/char/非角色容器） ----

TEST_F(BindCharMiscTest, SetupBindDispatch)

{
    ASSERT_NE(ensure_lua_env(), nullptr);  // room->setup 的 setup_city 走引擎

    // CRoom 分支(481-484)

    CRoom sroom;

    sroom.set_name("分发房", "setup_room1");

    lua_bind_push_object(L, &sroom);

    EXPECT_EQ(0, CallRetCount(l_setup, 1)) << "CRoom 分发(481-484)";

    // CNpc 分支(485-488)

    CNpc snpc;

    snpc.set_name("分发怪", "setup_npc1");

    lua_bind_push_object(L, &snpc);

    EXPECT_EQ(0, CallRetCount(l_setup, 1)) << "CNpc 分发(485-488)";

    // CChar 分支(489-493)：setup = enable_player + setup_char

    me.set("disabled_x", (long)1);  // 占位无关；setup 自身可观察：

    lua_bind_push_object(L, &me);

    EXPECT_EQ(0, CallRetCount(l_setup, 1)) << "CChar 分发(489-493)";

    EXPECT_EQ('\0', me.querystr_temp("disabled")[0]) << "setup 后解除禁用态";

    // 皆不匹配 → 末尾 return(494)

    lua_bind_push_object(L, &env);

    EXPECT_EQ(0, CallRetCount(l_setup, 1)) << "非角色容器走末尾(494)";
}

// ---- Char.cpp: l_command 非角色对象 → false(431) ----

TEST_F(BindCharMiscTest, CommandBind_NonCharArg_ReturnsFalse)

{
    lua_bind_push_object(L, &env);  // CContainer 非 CChar → !ch

    lua_pushstring(L, "look");

    int k = call(l_command, 2);

    ASSERT_EQ(1, k) << "返回一个布尔";

    EXPECT_FALSE(lua_toboolean(L, -1)) << "非角色对象返回 false(431)";

    lua_pop(L, k);
}

// ---- CmdHandle.cpp: drop_all (empty inventory -> no-op) ----

TEST_F(BindCharMiscTest, DropAllEmpty)

{
    lua_bind_push_object(L, &me);

    int k = call(l_drop_all, 1);

    EXPECT_GE(k, 0);

    if (k >= 0) lua_pop(L, k);
}

// ---- Container.cpp lifecycle: create / purge / clear ----

TEST_F(BindCharMiscTest, CreatePurgeClear)

{
    CContainer c;

    c.set_name("c", "c1");

    lua_bind_push_object(L, &c);

    lua_pushinteger(L, 0);

    CallVoid(l_create, 2);

    lua_bind_push_object(L, &c);

    CallVoid(l_purge, 1);  // non-treasure -> no-op, returns 0 values

    CChar ch;

    ch.set("k", (int32_t)5);

    lua_bind_push_object(L, &ch);

    CallVoid(l_clear, 1);

    EXPECT_EQ(0, ch.query("k"));
}

// set_heart_beat / begin_fb touch the global heartbeat list -> guard only

TEST_F(BindCharMiscTest, HeartBeatGuard)

{
    // flag 0 → 从心跳链表移除；void 转发、pcall 不抛（链表无公开查询口）

    CContainer c;

    c.set_name("hb", "hb1");

    lua_bind_push_object(L, &c);

    lua_pushinteger(L, 0);

    EXPECT_EQ(0, CallRetCount(l_set_heart_beat, 2));
}

// ---- CmdHandle.cpp: drop_all（有容量时的清扫语义） ----

TEST_F(BindCharMiscTest, DropAll_InventorySweep)

{
    ASSERT_NE(ensure_lua_env(), nullptr);  // drop_all→me->command 走派发链

    // command() 会把环境 dynamic_cast 成 IActionHandler → 必须真实房间

    CRoom room;

    room.set_name("清扫间", "da_room");

    room.set("max_items", (long)1000);  // count>0 → 走 142 落地支

    me.move(&room);

    // 现金留下（128）、普通件落地（142-143）、装备先卸再落（132-134）

    CMCoin* cash = new CMCoin();

    cash->create();

    cash->set_amount(50);

    cash->move(&me);

    CItem* junk = new CItem();

    junk->set_name("杂物", "da_junk");

    junk->move(&me);

    auto* eq = new CEquip();

    eq->set_name("佩刀", "da_sword");

    eq->set("base_name", "da_sword");

    CMapping apply;

    apply.set("damage", 1L);

    apply.set("desc", "攻击");

    eq->set("apply", apply);

    eq->set("wield_position", (int32_t)WIELD_HAND);

    eq->move(&me);

    eq->do_action(CChar::DO_WIELD, &me);

    ASSERT_NE(0, eq->query("equipped")) << "前置：已装备";

    lua_bind_push_object(L, &me);

    EXPECT_EQ(1, call(l_drop_all, 1)) << "drop_all 返回 true";

    EXPECT_EQ(&room, junk->environment()) << "普通件落地(142-143)";

    EXPECT_EQ(&room, eq->environment()) << "装备卸落后落地(132-134,142)";

    EXPECT_EQ(0, eq->query("equipped")) << "落地前已卸装(133)";

    EXPECT_EQ(&me, cash->environment()) << "现金不落(128)";

    cash->move(nullptr);

    delete cash;

    junk->move(nullptr);

    delete junk;

    eq->move(nullptr);

    delete eq;

    me.move(nullptr);  // 栈房析构前必须摘人（UAF 预防）
}

// ---- l_drop_all 绑定守卫：非对象/非角色容器 → false ----

TEST_F(BindCharMiscTest, DropAllBind_NonObjectArg_ReturnsFalse)

{
    lua_pushstring(L, "not an object");

    EXPECT_EQ(0, CallInt(l_drop_all, 1)) << "非对象参数 → false(351-352)";
}

TEST_F(BindCharMiscTest, DropAllBind_ContainerArg_ReturnsFalse)

{
    CContainer c;

    c.set_name("容器", "dbc1");

    lua_bind_push_object(L, &c);

    EXPECT_EQ(0, CallInt(l_drop_all, 1)) << "非角色容器 → false(360)";
}

// ---- ItemInterface.cpp: add_menu_item 派发(58-65) + get_info 锁定段(112-113)
// ----
TEST_F(BindCharMiscTest, ItemAddMenuAndLockedInfo)
{
    StateRecordingUser viewer;
    viewer.set_name("看客", "ii_viewer");
    viewer.move(&env);

    CItem* item = new CItem();
    item->set_name("铁匣", "ii_box");
    item->move(&env);

    // add_menu_item：组 QUESTION 报文并投递给 me(58-65)
    viewer.clear();  // 入场消息不计入
    item->add_menu_item("查看", "look", &viewer);
    EXPECT_EQ(1, viewer.sent) << "菜单项已投递(64 send_command)";
    EXPECT_TRUE(viewer.has("&c=look&")) << "报文携带命令串(62)";
    EXPECT_TRUE(viewer.has("&s=")) << "报文携带展示名(62)";

    // get_info：status=locked → 附带锁定原因(112-113)
    item->set("status", "locked");
    item->set("reason", "被官府封了");
    char info[512];
    info[0] = 0;
    item->get_info(info, &viewer);
    EXPECT_NE(strstr(info, "l="), nullptr) << "锁定原因随信息带出(112-113)";

    viewer.move(nullptr);
    item->move(nullptr);
    delete item;
}
