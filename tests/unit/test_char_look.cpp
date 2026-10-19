// test_char_look.cpp - Unit tests for CharLook.cpp (观察 / 外观描述).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Item.h"  // CItem（inventory_look 堆叠显示）
#include "NpcMgr.h"
#include "Mapping.h"
#include "AutoFight.h"  // 敌意注视触发 berserk 的 call_out 清理
#include "LuaEngine.h"
#include "LuaBind.h"
#include "test_lua_env.h"

// inventory_look 是 protected 成员 → 派生暴露供直测
class LookExposedChar : public CChar {
public:
    using CChar::inventory_look;
};

class CharLookTest : public ::testing::Test {
protected:
    CChar* ob;  // 被观察者
    CUser* me;  // 观察者
    CContainer* room;
    void SetUp() override
    {
        ensure_lua_env();
        ob = new CChar();
        ob->set("id", "ob1");
        ob->set("name", "路人");
        ob->set("gender", "男性");
        ob->set("race", "人类");
        ob->set("age", (long)300);
        ob->set("per", (long)20);
        ob->set("hp", (long)100);
        ob->set("eff_hp", (long)100);
        ob->set("max_hp", (long)100);
        ob->set("mp", (long)50);
        ob->set("repute", (long)10);
        ob->set_skill("unarmed", 50);
        ob->set_skill("dodge", 30);

        me = new CUser();
        me->set("id", "me1");
        me->set("name", "我");
        me->set("gender", "男性");
        me->set("per", (long)20);
        me->set("repute", (long)10);
        me->set("grant", (long)30);

        room = new CContainer();
        room->set("name", "测试场景");
        ob->move(room);
        me->move(room);
    }
    void TearDown() override
    {
        ob->move(nullptr);
        me->move(nullptr);
        delete ob;
        delete me;
        delete room;
    }
};

// ---------------- eff_status_msg ----------------
TEST_F(CharLookTest, EffStatusMsg)
{
    EXPECT_STREQ(CChar::eff_status_msg(100), "看起来气血充盈，并没有受伤。");
    EXPECT_NE(CChar::eff_status_msg(50), nullptr);
    EXPECT_NE(CChar::eff_status_msg(3), nullptr);
    EXPECT_STREQ(CChar::eff_status_msg(3),
                 "受伤过重，已经有如风中残烛，随时都可能断气。");
}

// ---------------- do_look: self ----------------
TEST_F(CharLookTest, DoLook_Self)
{
    EXPECT_EQ(ob->do_look(ob), 0);  // me == this
}

// ---------------- do_look: other ----------------
TEST_F(CharLookTest, DoLook_Other) { EXPECT_EQ(ob->do_look(me), 0); }

// ---------------- do_look: same family ----------------
TEST_F(CharLookTest, DoLook_SameFamily)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "少林派");
    fam1.set("generation", (long)2);
    fam1.set("master_id", "m1");
    fam2.set("family_name", "少林派");
    fam2.set("generation", (long)2);
    fam2.set("master_id", "m1");
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

TEST_F(CharLookTest, DoLook_FamilyElder)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "少林派");
    fam1.set("generation", (long)1);
    fam1.set("master_id", "m1");
    fam2.set("family_name", "少林派");
    fam2.set("generation", (long)3);
    fam2.set("master_id", "m1");
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

TEST_F(CharLookTest, DoLook_FamilyJunior)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "少林派");
    fam1.set("generation", (long)3);
    fam1.set("master_id", "m1");
    fam2.set("family_name", "少林派");
    fam2.set("generation", (long)1);
    fam2.set("master_id", "m1");
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- do_look: couple ----------------
TEST_F(CharLookTest, DoLook_Couple)
{
    CMapping c1, c2;
    c1.set("id", "me1");
    c1.set("gender", "妻子");
    c2.set("id", "ob1");
    c2.set("gender", "丈夫");
    ob->set("couple", c1);
    me->set("couple", c2);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- do_look: beast race ----------------
TEST_F(CharLookTest, DoLook_Beast)
{
    ob->set("race", "野兽");
    EXPECT_EQ(ob->do_look(me), 0);  // skips the human description line
}

// ---------------- do_look: hostile (may trigger autofight) ----------------
TEST_F(CharLookTest, DoLook_Hostile)
{
    ob->set("repute", (long)-1000);
    me->set("repute", (long)1000);
    me->set("per", (long)5);
    ob->set("per", (long)30);
    ob->set("mp", (long)100);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- do_look: wizard sees details ----------------
TEST_F(CharLookTest, DoLook_WizardSeesExtras)
{
    me->set("grant", (long)100);  // wizardp(me) true
    ob->set("daily/family/少林派/daily", (long)50);
    ob->set_temp("apply/mf", (long)30);
    ob->set("misc/beekeeping/flower_supply", (long)7);
    ob->set("misc/leisure/spark_supply", (long)8);
    ob->set("misc/alms/alms_supply", (long)9);
    ob->set("con", (long)20);
    EXPECT_EQ(ob->do_look(me), 0);
}

TEST_F(CharLookTest, DoLook_SkillPro)
{
    CMapping inner, outer;
    CMapping pro;
    pro.set("damage", (long)10);
    pro.set("damage_point", (long)5);
    pro.set("dodge", (long)3);
    pro.set("dodge_point", (long)1);
    pro.set("parry", (long)2);
    pro.set("parry_point", (long)1);
    pro.set("force", (long)4);
    pro.set("force_point", (long)2);
    inner.set("unarmed", pro);
    outer.set("少林派", inner);
    ob->set("skill_pro", outer);
    ob->set("grant", (long)30);
    EXPECT_EQ(ob->do_look(ob), 0);  // self -> wizard-only block runs
}

TEST_F(CharLookTest, DoLook_WithInventory)
{
    CContainer* item = new CContainer();
    item->set("name", "铁剑");
    item->set("id", "sword");
    item->set("equipped", (long)1);
    item->move(ob);
    EXPECT_EQ(ob->do_look(me), 0);
    item->move(nullptr);
    delete item;
}

// ---------------- hooks.do_look: 调用点实参契约 ----------------
TEST_F(CharLookTest, DoLook_HookReceivesTargetAndObserver)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    // 安装探针前保存真实实现，断言前恢复
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_do_look = hooks.do_look\n"
                            "hooks.do_look = function(t, o) "
                            "_G.__look_cap = {t_id = t:object_id(), "
                            "o_id = o:object_id()} end"),
              LUA_OK);

    EXPECT_EQ(ob->do_look(me), 0);

    ASSERT_EQ(luaL_dostring(L,
                            "hooks.do_look = __orig_do_look "
                            "__orig_do_look = nil"),
              LUA_OK);
    lua_getglobal(L, "__look_cap");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "t_id");
    int64_t target_id = (int64_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "o_id");
    int64_t observer_id = (int64_t)lua_tointeger(L, -1);
    lua_pop(L, 2);
    luaL_dostring(L, "_G.__look_cap = nil");

    EXPECT_EQ(target_id, (int64_t)ob->object_id());
    EXPECT_EQ(observer_id, (int64_t)me->object_id());
}

// ---------------- inventory_look：标记/堆叠显示(36-41) ----------------
TEST_F(CharLookTest, InventoryLookFlagAndAmount)
{
    CItem* pile = new CItem();
    pile->set_name("石头", "look_stone");

    // flag=1 且成堆 → "……3……石头"(36-39)
    pile->set_amount(3);
    LookExposedChar viewer;
    const char* stacked = viewer.inventory_look(pile, 1);
    EXPECT_NE(strstr(stacked, "3"), nullptr) << "成堆显示数量(38-39)";
    EXPECT_NE(strstr(stacked, pile->name()), nullptr) << "成堆含物品名(39)";

    // flag=1 单件 → 直接名字(41)
    pile->set_amount(1);
    EXPECT_STREQ(viewer.inventory_look(pile, 1), pile->name())
        << "单件直名(41)";

    // flag=0 → 跳过 else-if(36)：返回空串（既非 equipped 又无 flag）
    EXPECT_EQ('\0', viewer.inventory_look(pile, 0)[0])
        << "无 flag 且未装备 → 空";

    pile->move(nullptr);
    delete pile;
}

// ---------------- family 描述：同辈女性支(118) ----------------
TEST_F(CharLookTest, LookFamilySameGenFemale)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "同门派");
    fam1.set("generation", (long)2);
    fam1.set("master_id", "m1");
    fam2.set("family_name", "同门派");
    fam2.set("generation", (long)2);
    fam2.set("master_id", "m1");
    ob->set("family", fam1);
    me->set("family", fam2);
    ob->set("gender", "女性");  // 非男/无 → 走 118 师姐/师妹支
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- family 描述：对方是自己的师父(130) ----------------
TEST_F(CharLookTest, LookFamilyTargetIsMaster)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "同门派");
    fam1.set("generation", (long)1);
    fam2.set("family_name", "同门派");
    fam2.set("generation", (long)3);
    fam2.set("master_id", "ob1");  // 我的师父就是被观察者(ob id=ob1)
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- family 描述：入门更早的师伯(135) ----------------
TEST_F(CharLookTest, LookFamilySeniorByEnterTime)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "同门派");
    fam1.set("generation", (long)2);
    fam1.set("enter_time", (long)1);
    fam2.set("family_name", "同门派");
    fam2.set("generation", (long)3);
    fam2.set("master_id", "m1");
    fam2.set("enter_time", (long)2);
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- family 描述：对方是自己的弟子(142-143) ----------------
TEST_F(CharLookTest, LookFamilyTargetIsDisciple)
{
    CMapping fam1, fam2;
    fam1.set("family_name", "同门派");
    fam1.set("generation", (long)2);
    fam1.set("master_id", "me1");  // 对方拜我为师(me id=me1)
    fam2.set("family_name", "同门派");
    fam2.set("generation", (long)1);
    ob->set("family", fam1);
    me->set("family", fam2);
    EXPECT_EQ(ob->do_look(me), 0);
}

// ---------------- 敌意注视：声望对立 + 魅力差 → 转头盯视 + PK引战(206-212)
// ----------------
TEST_F(CharLookTest, LookHostileStareTriggersBerserk)
{
    extern int g_current_time;
    int saved_time = g_current_time;
    g_current_time = 1000000;

    // 声望一正一负且差值 > ob.mp*20（ob.mp=50 → 需 >1000）
    me->set("repute", (long)1000);
    ob->set("repute", (long)-1000);
    me->set("age", (long)30);  // 双方 >15 才进入魅力判断(209)
    me->set("per", (long)5);   // 观察者魅力低 → 激怒(209)

    EXPECT_EQ(ob->do_look(me), 0);
    EXPECT_GT(me->query_temp("pending/PK"), 0) << "对立注视记下 PK 预备(211)";

    // 清理 auto_fight 挂在被观察者(ob)上的 call_out/心跳，防悬垂
    ob->remove_call_out(CAutoFight::start_berserk, 1);
    ob->set_heart_beat(0);
    me->remove_call_out(CAutoFight::start_berserk, 1);
    me->set_heart_beat(0);
    me->delete_temp("pending/PK");
    g_current_time = saved_time;
}