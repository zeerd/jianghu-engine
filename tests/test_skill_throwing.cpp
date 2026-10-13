// test_skill_throwing.cpp - 投掷技能（src/skill/Throwing.cpp）与
//                          兵器效果（src/combat/WeaponEffect.cpp）的行为契约
//
// 偷袭（do_sneak）契约：目标/房间/权限/武器/队伍/法力六道守卫，命中与
// 躲避两种结局（未开战的命中只结仇不掉血——防磨 BOSS），每次出手消耗
// 一枚暗器（throw_weapon）。随机域用两类确定性夹具：
//   - 必中：throwing=1 → random(1) 恒 0，攻恒小于守 → 必躲；
//   - 必躲：throwing 巨大 + dodge=0 → 概率逼近 1，有限次重试收敛后断言。
// 掰武器（bash_weapon）的三种结局同用有限重试收敛。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>
#include <vector>

#include "Channel.h"
#include "Char.h"
#include "CombatTypes.h"
#include "Equip.h"
#include "FightRoom.h"
#include "Global.h"
#include "HeartBeat.h"  // g_destruct_list：暗器耗尽自毁后回收
#include "Item.h"
#include "Login.h"  // wizardp
#include "Skill.h"
#include "Throwing.h"
#include "User.h"
#include "WeaponEffect.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {

// 可堆叠物件：数量>1 搬入不触发"涉嫌复制"闸门（CAN_COMBINE 豁免）
class StackableItem : public CItem {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == CAN_COMBINE) return 1;
        return CItem::query_attr(attr);
    }
};

// 未开战命中会 kill_ob 开战——经 g_item_mgr 造真实战室（默认空指针）；
// 战室登记点由夹具统一回收
std::vector<CContainer*> g_created_fight_rooms;

CContainer* throwing_item_factory(const char* base_name)
{
    if (strcmp(base_name, "fight_room") == 0) {
        CFightRoom* fr = new CFightRoom();
        fr->set("base_name", base_name);
        g_created_fight_rooms.push_back(fr);
        return fr;
    }
    return nullptr;
}

}  // namespace

class ThrowingTest : public ::testing::Test {
protected:
    CContainer* room    = nullptr;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;
    MockDatabase* db = nullptr;  // 物件搬入/搬出玩家会触存盘（同 vendor 教训）
    // SetUp 时快照：TearDown 只回收本用例新增的自毁对象（耗尽暗器）
    std::vector<CContainer*> m_live_at_setup;
    std::vector<CContainer*> m_items;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        // 玩家搬入搬出链路会触存盘（g_db 空引用即崩），mock 接住
        db   = new MockDatabase();
        g_db = db;
        // 暗器耗尽自毁会走 g_item_mgr->remove_item；未开战命中的 kill_ob
        // 会经 g_item_mgr 造战室——测试进程默认空指针，夹具注入 mock
        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &throwing_item_factory;
        g_item_mgr        = mgr;
        room              = new CContainer();
        room->set_name("竹林", "throwing_test_room");
        room->set("infinity", 1);
    }

    void TearDown() override
    {
        // 1) 全部先脱房/脱身（此时对象都还活着：物品脱玩家会经旧环境
        //    触存盘，角色脱房会遍历列表——顺序错一步就是 UAF）
        for (auto* it : m_items) {
            it->set_heart_beat(0);
            it->move(nullptr);
        }
        // 2) 延迟析构列表：只回收本用例新增的对象（耗尽自毁的暗器）。
        //    被回收者可能同时挂在 m_items 里，记下防止第 4 步二次释放
        std::vector<CContainer*> reclaimed;
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                reclaimed.push_back(ob);
                delete ob;
            }
        }
        // 3) 造出来的战室（命中路径经工厂生成）
        for (auto* fr : g_created_fight_rooms) delete fr;
        g_created_fight_rooms.clear();
        // 4) 脱身后统一删除（跳过第 2 步已回收的自毁暗器）
        for (auto* it : m_items) {
            bool gone = false;
            for (auto* p : reclaimed) {
                if (p == it) gone = true;
            }
            if (gone) continue;
            delete it;
        }
        m_items.clear();
        delete room;
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr  = nullptr;
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    // 录制偷袭者（玩家）：接"准备偷袭/中暗器/躲过"等私聊文案
    StateRecordingUser* MakeSneaker(const char* id)
    {
        auto* u = new StateRecordingUser();
        u->set_name("镖师", id);
        u->set("mp", (long)1000);
        u->set("hp", (long)100);
        u->set("eff_hp", (long)100);
        u->set_skill("throwing", 100);
        u->move(room);
        u->delete_temp("client/loading");
        m_items.push_back(u);
        return u;
    }

    // 普通目标（NPC/路人；玩家目标会被"不能偷袭玩家"拦下）
    CChar* MakeTarget(const char* id)
    {
        CChar* c = new CChar();
        c->set_name("路人", id);
        c->set("hp", (long)100);
        c->set("eff_hp", (long)100);
        c->move(room);
        m_items.push_back(c);
        return c;
    }

    // 录制受害者（接被夺/被断的播报文案）
    StateRecordingUser* MakeVictim(const char* id)
    {
        auto* u = new StateRecordingUser();
        u->set_name("行商", id);
        u->set("hp", (long)100);
        u->set("eff_hp", (long)100);
        u->move(room);
        u->delete_temp("client/loading");
        m_items.push_back(u);
        return u;
    }

    // 造一枚暗器放入身上（amount=1 单发；>1 需可堆叠免"复制"闸门）
    CItem* GiveThrowing(CChar* who, int32_t amount, const char* id = "dart")
    {
        CItem* it = (amount > 1) ? new StackableItem() : new CItem();
        it->set_name("飞镖", id);
        it->set("base_name", "dart");
        it->set("skill_type", "throwing");
        it->set_amount(amount);
        it->move(who);
        m_items.push_back(it);
        return it;
    }

    // 造一柄可被夺/断的兵器并装备到手上（query_weapon 需 wield 登记 +
    // skill_type）
    CEquip* MakeWielded(CChar* who, const char* name, const char* skill_type,
                        int32_t rigidity)
    {
        auto* eq = new CEquip();
        eq->set_name(name, "equip");
        eq->set("base_name", "equip");
        eq->set("skill_type", skill_type);
        eq->set("rigidity", rigidity);
        CMapping apply;
        apply.set("damage", 1L);
        apply.set("desc", "攻击");
        eq->set("apply", apply);
        eq->set("wield_position", (int32_t)WIELD_HAND);
        eq->move(who);
        eq->do_action(CChar::DO_WIELD, who);
        m_items.push_back(eq);
        return eq;
    }

    void DiscardUser(StateRecordingUser* u)
    {
        u->set_heart_beat(0);
        u->move(nullptr);
        g_user_list.erase(u->object_id());
        delete u;
    }

    void DiscardChar(CChar* c)
    {
        c->set_heart_beat(0);
        c->move(nullptr);
        delete c;
    }
};

// =====================================================================
// create / query_action：技能注册与出招姿势
// =====================================================================
TEST_F(ThrowingTest, Create_RegistersSneakMagic)
{
    CThrowing sk;
    sk.create();
    EXPECT_STREQ("基本投掷", sk.name());
    EXPECT_EQ(CSkill::CAST_TO_OTHER, sk.query_magic_target("sneak"))
        << "偷袭以对方为目标";
    EXPECT_EQ(1, sk.query_magic_hostile("sneak")) << "偷袭缺省敌意";
}

TEST_F(ThrowingTest, QueryAction_ReturnsThrowPose)
{
    CThrowing sk;
    CMapping* m = sk.query_action(nullptr);
    ASSERT_NE(nullptr, m);
    EXPECT_STREQ("$N扬起$w向$n投去。", m->querystr("action"));
}

// =====================================================================
// get_first_throwing_weapon：装备优先、背包首个、按名点选
// =====================================================================
TEST_F(ThrowingTest, GetFirstThrowingWeapon_NullMe_ReturnsNull)
{
    EXPECT_EQ(nullptr, CThrowing::get_first_throwing_weapon(nullptr));
}

TEST_F(ThrowingTest, GetFirstThrowingWeapon_EquippedThrowingWins)
{
    auto* me    = MakeSneaker("gtw1");
    CItem* held = GiveThrowing(me, 1, "held_dart");
    MakeWielded(me, "金镖", "throwing", 100);  // 装备的也是投掷类

    CContainer* got = CThrowing::get_first_throwing_weapon(me);
    EXPECT_STREQ("金镖", got->name()) << "已装备的投掷武器优先";
}

TEST_F(ThrowingTest, GetFirstThrowingWeapon_InventoryScanAndNamePick)
{
    auto* me = MakeSneaker("gtw2");
    GiveThrowing(me, 1, "dart_a");  // 先入包（列表后见）
    GiveThrowing(me, 1, "dart_b");  // 后入包（列表首个）
    CItem* other = new CItem();
    other->set_name("药瓶", "yaoping");
    other->move(me);
    m_items.push_back(other);

    // 未指定名：取背包中首个投掷物（列表头优先）
    CContainer* first = CThrowing::get_first_throwing_weapon(me);
    EXPECT_STREQ("dart_b", first->querystr("id"));

    // 指定名：点选命名的那枚
    CContainer* named = CThrowing::get_first_throwing_weapon(me, "dart_a");
    EXPECT_STREQ("dart_a", named->querystr("id"));

    // 指定名不存在：回落到首个
    CContainer* fallback = CThrowing::get_first_throwing_weapon(me, "no_such");
    EXPECT_STREQ("dart_b", fallback->querystr("id"));
}

// =====================================================================
// do_sneak 守卫：目标/房间/权限/武器/队伍/法力
// =====================================================================
TEST_F(ThrowingTest, DoSneak_NoTargetAndSelf_Rejected)
{
    auto* me = MakeSneaker("snk1");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, nullptr));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你想偷袭谁"));

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, me));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你想偷袭谁"));
}

TEST_F(ThrowingTest, DoSneak_PeaceRoom_Rejected)
{
    room->set("no_fight", 1);
    auto* me    = MakeSneaker("snk2");
    CChar* targ = MakeTarget("snk2_t");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, targ));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("禁止偷袭"));
}

TEST_F(ThrowingTest, DoSneak_CannotTargetWizardOrPlayer)
{
    auto* me      = MakeSneaker("snk3");
    CChar* wizard = MakeTarget("snk3_w");
    wizard->set("grant", (long)CChar::GRANT_WIZARD);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, wizard));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能偷袭巫师"));

    // 玩家目标同样被拦（必须是真玩家：userp 判定）
    auto* player         = MakeVictim("snk3_p");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, player));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能偷袭玩家"));
}

TEST_F(ThrowingTest, DoSneak_WithoutWeapon_Rejected)
{
    auto* me    = MakeSneaker("snk4");
    CChar* targ = MakeTarget("snk4_t");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, targ));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("没有暗器"));
}

TEST_F(ThrowingTest, DoSneak_SameTeamWhileFighting_Rejected)
{
    auto* me    = MakeSneaker("snk5");
    CChar* targ = MakeTarget("snk5_t");
    GiveThrowing(me, 1);
    me->set_temp("fight/is_fighting", 1);
    me->set_temp("fight/fight_team", 0);
    targ->set_temp("fight/is_fighting", 1);
    targ->set_temp("fight/fight_team", 0);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, targ));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("本方队员"));
}

TEST_F(ThrowingTest, DoSneak_LowMp_Rejected)
{
    auto* me    = MakeSneaker("snk6");
    CChar* targ = MakeTarget("snk6_t");
    CItem* dart = GiveThrowing(me, 1);
    me->set("mp", (long)99);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CThrowing::do_sneak(me, targ));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("法力不足"));
    EXPECT_EQ(1, dart->query_amount()) << "被拒的偷袭不得消耗暗器";
}

// =====================================================================
// do_sneak 结局：必躲（throwing=1 → random(1) 恒 0）
// =====================================================================
TEST_F(ThrowingTest, DoSneak_DeterministicMiss_CostsMpAndBusy)
{
    auto* me    = MakeSneaker("snk7");
    CChar* targ = MakeTarget("snk7_t");
    CItem* dart = GiveThrowing(me, 5);
    me->set_skill("throwing", 1);  // random(1) 恒 0：攻恒不超守
    me->set("mp", (long)200);

    EXPECT_EQ(1, CThrowing::do_sneak(me, targ));
    EXPECT_TRUE(me->has("准备偷袭")) << "出手前有预告";
    EXPECT_TRUE(me->has("侧身躲过")) << "躲避结局文案";
    EXPECT_EQ(170, me->query("mp")) << "落空也耗 30 法力";
    EXPECT_TRUE(me->is_busy()) << "落空进入忙乱";
    EXPECT_EQ(4, dart->query_amount()) << "每出手消耗一枚暗器";
}

// =====================================================================
// do_sneak 结局：命中（未开战只结仇不掉血；开战掉血可淬毒）
// =====================================================================
TEST_F(ThrowingTest, DoSneak_HitOutOfCombat_MakesEnemiesWithoutDamage)
{
    auto* me    = MakeSneaker("snk8");
    CChar* targ = MakeTarget("snk8_t");
    GiveThrowing(me, 2000);
    me->set_skill("throwing", 1000000);  // 巨大随机域：有限次重试必然命中
    me->set("mp", (long)1000000);

    bool hit = false;
    for (int i = 0; i < 200 && !hit; i++) {
        me->set("mp", (long)1000000);
        if (CThrowing::do_sneak(me, targ) == 1 && me->has("中暗器了")) {
            hit = true;
        }
        me->clear();
    }
    EXPECT_TRUE(hit) << "巨大 throwing 域内必然命中";
    EXPECT_EQ(100, targ->query("hp"))
        << "未开战的偷袭只结仇不掉血（防磨 BOSS 规则）";
}

TEST_F(ThrowingTest, DoSneak_HitWhileFighting_DamagesAndPoisons)
{
    auto* me    = MakeSneaker("snk9");
    CChar* targ = MakeTarget("snk9_t");
    CItem* dart = GiveThrowing(me, 2000);
    me->set_skill("throwing", 1000000);
    me->set_skill("poison", 10);  // 淬毒流：出手预告带毒、命中附毒
    me->set("mp", (long)1000000);
    me->set_temp("fight/is_fighting", 1);
    me->set_temp("fight/fight_team", 0);
    targ->set_temp("fight/is_fighting", 1);
    targ->set_temp("fight/fight_team", 1);  // 异队：不触发"本方队员"守卫

    bool hit   = false;
    int sneaks = 0;
    for (int i = 0; i < 200 && !hit; i++) {
        me->set("mp", (long)1000000);
        sneaks++;
        if (CThrowing::do_sneak(me, targ) == 1 && targ->query("hp") < 100) {
            hit = true;
        }
    }
    EXPECT_TRUE(hit) << "开战偷袭必然命中";
    EXPECT_TRUE(me->has("淬了毒的")) << "毒攻流的出手预告";
    EXPECT_LT(targ->query("hp"), 100) << "开战偷袭结算暗器伤害";
    EXPECT_NE(0, targ->query_condition("feng_poison")) << "淬毒命中附毒";
    EXPECT_EQ(2000 - sneaks, dart->query_amount()) << "每次出手消耗一枚暗器";
}

// =====================================================================
// throw_weapon：暗器消耗（单发提示用尽并自毁、多发静默减量、绑定/免夺保全）
// =====================================================================
TEST_F(ThrowingTest, ThrowWeapon_SingleUse_TellsAndSelfDestructs)
{
    auto* me    = MakeSneaker("tw1");
    CChar* targ = MakeTarget("tw1_t");
    CItem* dart = GiveThrowing(me, 1);

    CWeaponEffect::throw_weapon(me, targ, dart, 0);
    EXPECT_TRUE(me->has("用完了")) << "最后一枚用尽要提示";
    EXPECT_EQ(nullptr, me->present("dart")) << "耗尽的暗器自毁离身";
    // 自毁对象进延迟列表：TearDown 统一回收
}

TEST_F(ThrowingTest, ThrowWeapon_MultiUse_DecrementsSilently)
{
    auto* me    = MakeSneaker("tw2");
    CChar* targ = MakeTarget("tw2_t");
    CItem* dart = GiveThrowing(me, 5);

    CWeaponEffect::throw_weapon(me, targ, dart, 0);
    EXPECT_EQ(4, dart->query_amount());
    EXPECT_FALSE(me->has("用完了")) << "尚有存货不提示";
}

TEST_F(ThrowingTest, ThrowWeapon_BoundOrNoDuo_Kept)
{
    auto* me    = MakeSneaker("tw3");
    CChar* targ = MakeTarget("tw3_t");

    CItem* bound = GiveThrowing(me, 3, "bound_dart");
    bound->set("owner", "someone");
    CWeaponEffect::throw_weapon(me, targ, bound, 0);
    EXPECT_EQ(3, bound->query_amount()) << "绑定暗器不消耗";

    CItem* noduo = GiveThrowing(me, 3, "noduo_dart");
    noduo->set("no_duo", 1);
    CWeaponEffect::throw_weapon(me, targ, noduo, 0);
    EXPECT_EQ(3, noduo->query_amount()) << "免夺暗器不消耗";
}

// =====================================================================
// bash_weapon：格挡反击三结局（脱手/险脱/折断）与四种免判
// =====================================================================
TEST_F(ThrowingTest, BashWeapon_Guards_LeaveWeaponUntouched)
{
    auto* me     = MakeSneaker("bw1");
    auto* vic    = MakeVictim("bw1_v");
    auto* stick  = MakeWielded(vic, "木棍", "staff", 0);
    CItem* sword = MakeWielded(me, "长剑", "sword", 0);

    // 兵器为空 / 非格挡结果 / 受害者空手 / 免夺，全都不动对方兵器
    CWeaponEffect::bash_weapon(me, vic, nullptr, RESULT_PARRY);
    CWeaponEffect::bash_weapon(me, vic, sword, 0 /* 非 PARRY */);
    EXPECT_EQ(static_cast<CContainer*>(vic), stick->environment());

    auto* unarmed_vic = MakeVictim("bw1_v2");
    CWeaponEffect::bash_weapon(me, unarmed_vic, sword, RESULT_PARRY);
    EXPECT_EQ(static_cast<CContainer*>(me), sword->environment());

    stick->set("no_duo", 1);
    CWeaponEffect::bash_weapon(me, vic, sword, RESULT_PARRY);
    EXPECT_EQ(static_cast<CContainer*>(vic), stick->environment())
        << "免夺兵器不被击落";
}

TEST_F(ThrowingTest, BashWeapon_OverwhelmingForce_WeaponFliesOut)
{
    auto* me    = MakeSneaker("bw2");
    auto* vic   = MakeVictim("bw2_v");
    auto* stick = static_cast<CItem*>(
        MakeWielded(vic, "木棍", "staff", 0));      // 守方零刚度：wdp=0
    MakeWielded(me, "玄铁重刃", "sword", 1000000);  // 攻方巨力

    bool flown = false;
    for (int i = 0; i < 300 && !flown; i++) {
        CWeaponEffect::bash_weapon(me, vic, me->query_weapon(), RESULT_PARRY);
        if (stick->environment() == room) flown = true;
    }
    EXPECT_TRUE(flown) << "压倒性力量终会击飞兵器";
    EXPECT_TRUE(vic->has("脱手飞出")) << "受害方收到脱手播报";
}

TEST_F(ThrowingTest, BashWeapon_EdgeForce_WeaponAlmostFlies)
{
    auto* me  = MakeSneaker("bw3");
    auto* vic = MakeVictim("bw3_v");
    MakeWielded(me, "青钢剑", "sword", 1000);  // wap∈[0,1000)

    // 三结局（脱手/险脱/折断）是同一随机公式的相邻阈值，无法参数互斥：
    // 循环里观察首个结局，命中"险脱"即收；兵器离手则重新上架再试
    bool shook = false;
    for (int i = 0; i < 100 && !shook; i++) {
        auto* stick = MakeWielded(vic, "木棍", "staff", 600);  // wdp∈[0,600)
        CWeaponEffect::bash_weapon(me, vic, me->query_weapon(), RESULT_PARRY);
        if (vic->has("险些脱手")) {
            shook = true;
            EXPECT_EQ(static_cast<CContainer*>(vic), stick->environment())
                << "险脱不离手";
            break;
        }
        // 脱手/折断：兵器已离手，清播报重新上架
        vic->clear();
        stick->set_heart_beat(0);
        stick->move(nullptr);
    }
    EXPECT_TRUE(shook) << "势均力敌时有险脱分支";
}

TEST_F(ThrowingTest, BashWeapon_SlightEdge_WeaponBreaks)
{
    auto* me    = MakeSneaker("bw4");
    auto* vic   = MakeVictim("bw4_v");
    auto* stick = static_cast<CItem*>(
        MakeWielded(vic, "脆弱木枪", "staff", 10));  // wdp=10：断线在 (5,10]
    MakeWielded(me, "精铁刀", "sword", 10);          // wap∈[0,10)

    bool broke = false;
    for (int i = 0; i < 300 && !broke; i++) {
        CWeaponEffect::bash_weapon(me, vic, me->query_weapon(), RESULT_PARRY);
        if (vic->has("断为两截")) broke = true;
    }
    EXPECT_TRUE(broke) << "小幅优势终会击断兵器";
    EXPECT_EQ(static_cast<CContainer*>(room), stick->environment())
        << "断刃落地";
    EXPECT_NE(std::string::npos, std::string(stick->name()).find("断掉的"))
        << "断刃更名";
    EXPECT_EQ(0, stick->query("value")) << "断刃归零";
}
