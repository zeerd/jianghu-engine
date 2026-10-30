// test_auto_fight.cpp - NPC 自动战斗触发器（src/combat/AutoFight.cpp）
//
// auto_fight 只负责派发：按 type 选中回调并 call_out 排期（5+random(5)
// 秒后执行）。测试用 remove_call_out(func, 1) 立即评估，确定性地驱动
// 延期回调；start_* 的守卫（目标消失/不省人事/和平区）与杀戮/切磋两种
// 结局分别断言。
//
// 两个测试要点：
// 1) 玩家搬入房间会置 client/loading（等客户端切图应答），present() 视其
//    不可见——夹具代客户端应答（delete_temp），与 loc 指令同款；
// 2) 观察点用受害者收到的文案：set_killer 的"想杀死你"通知、狂暴切磋
//    分支的"陪我玩两手"邀战。不用 is_killing——它按 object_id 查表而
//    set_killer 记的是角色 id，名不一致（见交接报告）。
// 开战成功路径经 g_item_mgr 造真实 CFightRoom——夹具注入 CMockItemMgr
// 提供 fight_room 工厂（默认 g_item_mgr 为空指针）。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>
#include <vector>

#include "AutoFight.h"
#include "Channel.h"
#include "Char.h"
#include "Container.h"
#include "FightRoom.h"
#include "User.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

namespace {

// 造战室工场的登记点：TearDown 统一回收夹具期间创建的战室
std::vector<CContainer*> g_created_fight_rooms;

CContainer* af_item_factory(const char* base_name)
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

class AutoFightTest : public ::testing::Test {
protected:
    CContainer* room    = nullptr;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        room = new CContainer();
        room->set_name("试炼场", "auto_fight_arena");
        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &af_item_factory;
        g_item_mgr        = mgr;
    }

    void TearDown() override
    {
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr = nullptr;
        for (auto* fr : g_created_fight_rooms) delete fr;
        g_created_fight_rooms.clear();
        delete room;
    }

    // 构造一名入场参战者（普通角色）
    CChar* MakeChar(const char* name, const char* id)
    {
        CChar* c = new CChar();
        c->set_name(name, id);
        c->set("hp", (long)100);
        c->set("eff_hp", (long)100);
        c->move(room);
        return c;
    }

    // 玩家受害者：录制投递；玩家互斗的等级限制是数据集规则
    // （hooks.combat_rule attack_gate）：玩家进攻方需 >=75 级才通过默认规则，
    // NPC 进攻方不受此限。搬入房间后代答 client/loading，否则 present() 视其不可见。
    StateRecordingUser* MakeVictim(const char* name, const char* id,
                                   long level = 0)
    {
        StateRecordingUser* u = new StateRecordingUser();
        u->set_name(name, id);
        if (level) u->set("level", level);
        u->set("hp", (long)100);
        u->set("eff_hp", (long)100);
        u->move(room);
        u->delete_temp("client/loading");
        return u;
    }

    // 摘心跳、脱房后销毁（call_out 排期会把对象挂进心跳表）
    void DiscardChar(CChar* c)
    {
        c->set_heart_beat(0);
        c->move(nullptr);
        delete c;
    }

    // 玩家收尾（录制用户多一次登记摘除）
    void DiscardUser(StateRecordingUser* u)
    {
        u->set_heart_beat(0);
        u->move(nullptr);
        g_user_list.erase(u->object_id());
        delete u;
    }
};

// =====================================================================
// auto_fight 派发：每种 type 排期各自的回调；未知类型落主动攻击档
// =====================================================================
TEST_F(AutoFightTest, TypeDispatch_SchedulesMatchingCallback)
{
    struct {
        const char* type;
        void (*func)(CContainer*, int32_t, int32_t);
    } cases[] = {
        {"berserk", CAutoFight::start_berserk},
        {"hatred", CAutoFight::start_hatred},
        {"vendetta", CAutoFight::start_vendetta},
        {"aggressive", CAutoFight::start_aggressive},
        {"unknown_type", CAutoFight::start_aggressive},  // 默认档
    };

    for (auto& c : cases) {
        // 玩家挑事（auto_fight 要求至少一方是玩家），普通角色作目标
        auto* me   = MakeVictim("兽", "af_dispatch_me");
        CChar* obj = MakeChar("客", "af_dispatch_obj");

        CAutoFight::auto_fight(me, obj, c.type);
        EXPECT_EQ(1, me->query_temp("looking_for_trouble")) << c.type;

        // 立即评估排期节点：只有匹配的回调会复位挑事标记
        me->remove_call_out(c.func, 1);
        EXPECT_EQ(0, me->query_temp("looking_for_trouble"))
            << "排期并执行的回调应复位标记: " << c.type;

        DiscardUser(me);
        DiscardChar(obj);
    }
}

// =====================================================================
// auto_fight 准入：至少一方是玩家；NPC 不打 NPC
// =====================================================================
TEST_F(AutoFightTest, NpcVsNpc_NotScheduled)
{
    CChar* npc_me  = MakeChar("狼", "af_npc_me");
    CChar* npc_obj = MakeChar("羊", "af_npc_obj");

    CAutoFight::auto_fight(npc_me, npc_obj, "hatred");
    EXPECT_EQ(0, npc_me->query_temp("looking_for_trouble"))
        << "NPC 不得对 NPC 排期自动战斗";

    // 确实没有排期：立即评估也不会有任何回调执行（标记保持 0）
    npc_me->remove_call_out(CAutoFight::start_hatred, 1);
    EXPECT_EQ(0, npc_me->query_temp("looking_for_trouble"));

    DiscardChar(npc_me);
    DiscardChar(npc_obj);
}

TEST_F(AutoFightTest, NpcVsUser_Scheduled)
{
    CChar* npc_me = MakeChar("狼", "af_npc_vs_user");
    auto* obj     = MakeVictim("客", "af_npc_vs_user_obj");

    CAutoFight::auto_fight(npc_me, obj, "hatred");
    EXPECT_EQ(1, npc_me->query_temp("looking_for_trouble"))
        << "NPC 挑战玩家应排期";

    DiscardChar(npc_me);
    DiscardUser(obj);
}

// =====================================================================
// auto_fight 去重：挑事标记未复位前只排期首个请求
// =====================================================================
TEST_F(AutoFightTest, DedupeWhilePending_SchedulesOnlyFirst)
{
    auto* me   = MakeVictim("狼", "af_dedupe_me");
    CChar* obj = MakeChar("客", "af_dedupe_obj");

    CAutoFight::auto_fight(me, obj, "hatred");
    CAutoFight::auto_fight(me, obj, "berserk");  // 标记未复位：应被去重

    // 狂暴节点不存在：立即评估无效果，标记保持 1
    me->remove_call_out(CAutoFight::start_berserk, 1);
    EXPECT_EQ(1, me->query_temp("looking_for_trouble"))
        << "第二次排期应被去重：狂暴回调不存在";

    // 首个仇恨节点存在：评估后复位标记
    me->remove_call_out(CAutoFight::start_hatred, 1);
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardUser(me);
    DiscardChar(obj);
}

// =====================================================================
// start_aggressive / start_vendetta / start_hatred：杀戮触发与守卫
// （受害者收到的"想杀死你"通知是 kill_ob 的可观察回执）
// =====================================================================
TEST_F(AutoFightTest, StartAggressive_KillsTargetOnSight)
{
    CChar* me = MakeChar("刺客", "af_agg_me");
    auto* obj = MakeVictim("路人", "af_agg_obj");
    me->set_temp("looking_for_trouble", 1);

    CAutoFight::start_aggressive(me, obj->object_id(), 0);

    EXPECT_TRUE(obj->has("想杀死你")) << "主动攻击应通知受害者被盯上";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"))
        << "回调走完应复位挑事标记";

    DiscardChar(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartVendetta_KillsTarget)
{
    CChar* me = MakeChar("刺客", "af_vend_me");
    auto* obj = MakeVictim("仇家", "af_vend_obj");
    me->set_temp("looking_for_trouble", 1);

    CAutoFight::start_vendetta(me, obj->object_id(), 0);
    EXPECT_TRUE(obj->has("想杀死你")) << "敌意触发应通知受害者";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardChar(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartHatred_KillsTarget)
{
    CChar* me = MakeChar("刺客", "af_hate_me");
    auto* obj = MakeVictim("宿敌", "af_hate_obj");
    me->set_temp("looking_for_trouble", 1);

    CAutoFight::start_hatred(me, obj->object_id(), 0);
    EXPECT_TRUE(obj->has("想杀死你")) << "仇恨触发应通知受害者";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardChar(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartHatred_TargetGone_NoAction)
{
    CChar* me = MakeChar("刺客", "af_hate_gone_me");
    me->set_temp("looking_for_trouble", 1);

    // 目标从未入场：回调在复位标记之前就因"目标消失"返回
    CChar* obj = new CChar();
    obj->set_name("幽灵", "af_hate_gone_obj");
    int32_t gone_id = obj->object_id();
    delete obj;

    CAutoFight::start_hatred(me, gone_id, 0);
    EXPECT_EQ(1, me->query_temp("looking_for_trouble"))
        << "目标消失时早退，标记复位行不得执行";

    DiscardChar(me);
}

TEST_F(AutoFightTest, StartAggressive_PeaceRoom_NoKill)
{
    room->set("no_fight", 1);
    CChar* me = MakeChar("刺客", "af_agg_peace_me");
    auto* obj = MakeVictim("路人", "af_agg_peace_obj");
    me->set_temp("looking_for_trouble", 1);

    CAutoFight::start_aggressive(me, obj->object_id(), 0);
    EXPECT_FALSE(obj->has("想杀死你")) << "和平区不得触发杀戮";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"))
        << "和平区短路在标记复位之后";

    DiscardChar(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartVendetta_NotLiving_NoKill)
{
    CChar* me = MakeChar("刺客", "af_vend_dead_me");
    auto* obj = MakeVictim("仇家", "af_vend_dead_obj");
    me->set_temp("disabled", "1");  // IS_LIVING 为假

    CAutoFight::start_vendetta(me, obj->object_id(), 0);
    EXPECT_FALSE(obj->has("想杀死你")) << "不省人事不得触发杀戮";

    DiscardChar(me);
    DiscardUser(obj);
}

// =====================================================================
// start_berserk：狂暴扫描——NPC 退避、玩家杀气压制、内力不敌转切磋
// （杀戮/切磋两分支的文案互斥："去死吧" vs "陪我玩两手"）
// =====================================================================
TEST_F(AutoFightTest, StartBerserk_NpcMe_ScansAndWithdraws)
{
    CChar* me = MakeChar("狂兽", "af_berserk_npc_me");
    auto* obj = MakeVictim("路人", "af_berserk_npc_obj");
    me->set_temp("looking_for_trouble", 1);

    CAutoFight::start_berserk(me, obj->object_id(), 0);
    // NPC 扫视后退避：不杀也不开战（!is_user(me) 守卫）
    EXPECT_FALSE(obj->has("想杀死你"));
    EXPECT_FALSE(obj->has("陪我玩两手"));
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardChar(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartBerserk_UserWithRepute_KillsTarget)
{
    auto* me = static_cast<StateRecordingUser*>(
        MakeVictim("狂徒", "af_berserk_kill_me", 75));
    // 受害者必须录制：set_killer 的"想杀死你"通知发给目标本人
    auto* obj = MakeVictim("路人", "af_berserk_kill_obj");
    me->set("mp", 0L);
    me->set("repute", -100L);  // shen = 100：杀气外露
    me->set("score", 0L);      // shen > score → 杀戮分支

    CAutoFight::start_berserk(me, obj->object_id(), 0);
    EXPECT_TRUE(obj->has("想杀死你")) << "杀气压过评分：直接下杀手";
    EXPECT_FALSE(obj->has("陪我玩两手")) << "杀戮分支不开口切磋";

    DiscardUser(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartBerserk_UserCalm_FightsTarget)
{
    auto* me = static_cast<StateRecordingUser*>(
        MakeVictim("狂徒", "af_berserk_fight_me", 75));
    auto* obj = MakeVictim("路人", "af_berserk_fight_obj", 75);
    me->set("mp", 0L);
    me->set("repute", -1L);  // shen = 1
    me->set("score", 10L);   // shen <= score → 切磋分支

    CAutoFight::start_berserk(me, obj->object_id(), 0);
    // 切磋分支只 fight_ob 不下杀手；文案与杀戮分支互斥
    EXPECT_FALSE(obj->has("想杀死你")) << "切磋不下杀手";
    EXPECT_TRUE(obj->has("陪我玩两手")) << "切磋分支开口邀战";
    // 双方过等级门槛后开战（同环境、异队）
    EXPECT_TRUE(me->is_fighting(obj)) << "切磋分支应开战";

    DiscardUser(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartBerserk_UserMpHigh_Suppresses)
{
    auto* me = static_cast<StateRecordingUser*>(
        MakeVictim("狂徒", "af_berserk_mp_me", 75));
    auto* obj = MakeVictim("路人", "af_berserk_mp_obj");
    me->set("mp", 100000L);  // 内力远胜杀气：压住冲动
    me->set("repute", -100L);

    CAutoFight::start_berserk(me, obj->object_id(), 0);
    EXPECT_FALSE(obj->has("想杀死你")) << "内力压制杀气：不触发杀戮";
    EXPECT_FALSE(obj->has("陪我玩两手")) << "被压制时也不开口切磋";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardUser(me);
    DiscardUser(obj);
}

TEST_F(AutoFightTest, StartBerserk_PeaceRoom_ReturnsBeforeScan)
{
    room->set("no_fight", 1);
    auto* me = static_cast<StateRecordingUser*>(
        MakeVictim("狂徒", "af_berserk_peace_me", 75));
    auto* obj = MakeVictim("路人", "af_berserk_peace_obj");
    me->set("mp", 0L);
    me->set("repute", -100L);

    CAutoFight::start_berserk(me, obj->object_id(), 0);
    EXPECT_FALSE(obj->has("想杀死你")) << "和平区直接退避";
    EXPECT_EQ(0, me->query_temp("looking_for_trouble"));

    DiscardUser(me);
    DiscardUser(obj);
}
