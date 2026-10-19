// test_reward.cpp — CReward 击杀结算（killer_reward）
//
// 契约：玩家击杀玩家时，经验损失由 CReward::victim_lose 按双方实战经验算出；
// 攻击方总舵仇敌名单命中数据集 kill_settle 规则则损失加倍。结算同时累加
// 受害者的 kill/pklose、pds 与 death/* 快照，并清掉总舵名单上的该次仇。
// 玩家击杀 NPC 时：NK 奖励经 nk_gain 算出后按 level*4+100
// 封顶入账（公共算法）； 封顶前的彩票掉落（pk 房 cp/*）、经验倍率（pk 房
// exp）、朱果掉落（pk 房 宝物） 以及 lu32 悬赏结算属数据集内容，由
// RewardNpcKillTest 锁定其可观察行为。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"
#include <cstring>
#include <memory>
#include "Char.h"
#include "CombatRules.h"
#include "CReward.h"
#include "Global.h"
#include "Npc.h"
#include "Room.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "IniParser.h"    // g_ini_data（share_exp 翻转）
#include "PartyConfig.h"  // CPartyConfig::init
#include "test_lua_env.h"
#include "test_scoped_room.h"
#include "test_state_recorder.h"  // 分红/播报的可观察面
extern int g_current_time;
extern MAP2USER g_user_list;
namespace {
class RewardTest : public ::testing::Test {
protected:
    // 装载 dataset 的 hooks.lua：kill_settle 规则由 hooks.combat_rule 提供
    void SetUp() override { ensure_lua_env(); }
    // 经验取值刻意让基础损失恰为 (4000+15000)/150 = 126 点
    static CUser* MakeUser(const char* name, const char* id, long exp)
    {
        CUser* u = new CUser();
        u->set_name(name, id);
        u->set("id", id);
        u->set("level", (long)10);
        u->set("combat_exp", exp);
        return u;
    }
};
// 玩家击杀 NPC 的整条结算链。掉落判定全部取确定性参数：
//   彩票 = random(cp/random)==0，取 cp/random=1
//   恒命中（CUtils::random(1)==0）； 朱果 = random(1000-int*18)==0，取 int=60
//   时参数为负恒命中，宝物=3 则配额已满恒不掉。
// 物品经 CMockItemMgr 工厂构造，不依赖真实物品数据集。
class RewardNpcKillTest : public ::testing::Test {
protected:
    CMockItemMgr mgr;
    // 全局管理器是进程级契约：SetUp 覆盖前必须保存，TearDown 恢复原值。
    // （此前直接置 nullptr，把"进程启动值"抹成了空，后续所有套件的
    //   save/restore 链一路传播空指针，凡走 find_object→g_item_mgr
    //   的路径全部踩空——见 CmdMasterDeepTest 回归）
    IItemMgr* saved_mgr = nullptr;
    std::unique_ptr<ScopedRoom> pk;
    CUser* killer = nullptr;
    CUser* master = nullptr;
    CNpc* victim  = nullptr;
    void SetUp() override
    {
        ensure_lua_env();
        g_current_time = 1000000;
        pk             = std::make_unique<ScopedRoom>("pk");
        ASSERT_NE(pk->get(), nullptr);
        saved_mgr        = g_item_mgr;
        g_item_mgr       = &mgr;
        mgr.load_factory = [](const char* base_name) -> CContainer* {
            CContainer* it = new CContainer();
            if (strcmp(base_name, "caipiao") == 0)
                it->set_name("彩票", "cai piao");
            else
                it->set_name("朱果", "sacred_fruit");
            it->set("value", (long)1);
            return it;
        };
    }
    void TearDown() override
    {
        g_user_list.clear();
        if (master) {
            master->move(nullptr);
            delete master;
            master = nullptr;
        }
        if (killer) {
            killer->move(nullptr);
            // 掉落物挂在击杀者背包里，无人回收，先清掉
            for (auto* ob : *killer->get_item_list()) delete ob;
            killer->get_item_list()->clear();
            delete killer;
        }
        // 掉落物随房间销毁无人回收，先清场再摘房间；
        // pk 房跨用例缓存，本套件写入的数据集标志必须复位
        if (pk) {
            for (auto* ob : *pk->get()->get_item_list()) delete ob;
            pk->get()->get_item_list()->clear();
            pk->get()->del("cp/random");
            pk->get()->del("cp/number");
            pk->get()->del("cp/index");
            pk->get()->del("宝物");
            pk->get()->del("exp");
            pk.reset();
        }
        delete victim;
        g_item_mgr = saved_mgr;  // 恢复进程原值，不得置空
    }
    // level=10 → NK 封顶 140；combat_exp=100 → 组队公式 dx=44
    CUser* MakeKiller(const char* id)
    {
        killer = new CUser();
        killer->set_name("猎手", id);
        killer->set("level", (long)10);
        killer->set("combat_exp", (long)100);
        killer->set("int", (long)60);
        killer->move(pk->get());
        return killer;
    }
    // nkgain=200 与 cx=100 → dx=44、times 钳到 2 → 封顶前奖励 400；nkgain=10 →
    // 奖励 0
    CNpc* MakeNpcVictim(const char* name, const char* id, long nkgain)
    {
        victim = new CNpc();
        victim->set_name(name, id);
        victim->set("nkgain", nkgain);
        victim->set("level", (long)5);
        return victim;
    }
};
// kill_settle 要对总舵房间 LockData/Load/Save，需要 g_db 指向 Mock
class RewardDbTest : public RewardTest {
protected:
    void SetUp() override
    {
        RewardTest::SetUp();
        db.SetQueryResultMatching("obj_lock",
                                  MockDatabase::MakeSingleRow({"1"}));
        g_db = &db;
    }
    void TearDown() override
    {
        g_db = nullptr;
        RewardTest::TearDown();
    }
    MockDatabase db;
};
}  // namespace
TEST_F(RewardTest, KillerReward_NoSettle_BasicLoss)
{
    ScopedRoom pk("pk");
    ASSERT_NE(pk.get(), nullptr);
    CUser* killer = MakeUser("杀手", "rw_nosettle_k", 1000);
    CUser* victim = MakeUser("良民", "rw_nosettle_v", 15000);
    g_reward.killer_reward(killer, victim);
    EXPECT_EQ(victim->query("combat_exp"), 14874) << "损失 126 点经验";
    EXPECT_EQ(victim->query("kill/pklose"), 126);
    EXPECT_EQ(victim->query("pds"), 1);
    EXPECT_EQ(victim->query("death/skill_loss"), 1);
    delete victim;
    delete killer;
}
// ---------------------------------------------------------------------------
// 玩家击杀 NPC：NK 奖励封顶入账（公共算法）
// nkgain=200、cx=100 → 封顶前 400 → 封顶 140（level*4+100）
// ---------------------------------------------------------------------------
TEST_F(RewardNpcKillTest, NpcKill_RewardCappedByLevel)
{
    CUser* k = MakeKiller("rw_npc_cap_k");
    pk->get()->set("宝物", (long)3);  // 朱果配额已满：掉落恒不触发
    CNpc* v = MakeNpcVictim("流寇", "rw_npc_cap_v", (long)200);
    g_reward.killer_reward(k, v);
    EXPECT_EQ(k->query("combat_exp"), 100 + 140) << "奖励封顶 140 后入账";
    EXPECT_EQ(k->query("kill/nkgain"), 140);
    EXPECT_EQ(k->query("mks"), 1);
    EXPECT_EQ(k->query("repute"), 0) << "击杀者等级高于 NPC，不倒扣声望";
    EXPECT_EQ(pk->get()->query("宝物"), 3) << "配额未变";
    EXPECT_EQ(pk->get()->present("sacred_fruit"), nullptr);
}
// ---------------------------------------------------------------------------
// 彩票掉落：cp/random=1 恒命中，号码在 pk 标志房递增，彩票入击杀者背包
// ---------------------------------------------------------------------------
// 受害者无 start_city（非城市出身 NPC）→ 不发彩票
TEST_F(RewardNpcKillTest, NpcKill_NoStartCity_NoLottery)
{
    CUser* k = MakeKiller("rw_npc_nocp_k");
    pk->get()->set("宝物", (long)3);
    pk->get()->set("cp/random", (long)1);
    CNpc* v = MakeNpcVictim("流寇", "rw_npc_nocp_v", (long)200);
    g_reward.killer_reward(k, v);
    EXPECT_EQ(k->present("cai piao"), nullptr);
    EXPECT_EQ(pk->get()->query("cp/number"), 0) << "未开奖则号码计数不动";
}
// 目标不符 → 悬赏不结算
TEST_F(RewardNpcKillTest, NpcKill_Lu32WrongTarget_NoPay)
{
    CUser* k = MakeKiller("rw_npc_lu32bad_k");
    k->set_temp("lu32/overtime", (long)g_current_time + 1);
    k->set_temp("lu32/name", "流寇");
    k->set_temp("lu32/id", "rw_npc_lu32_other");
    k->set_temp("lu32/exp", (long)500);
    k->set_temp("lu32/finish", (long)201);
    pk->get()->set("宝物", (long)3);
    pk->get()->set("cp/random", (long)0);
    CNpc* v = MakeNpcVictim("流寇", "rw_npc_lu32bad_v", (long)200);
    g_reward.killer_reward(k, v);
    EXPECT_EQ(k->query("combat_exp"), 100 + 140);
    EXPECT_EQ(k->query_temp("lu32/exp"), 500) << "任务未完成，悬赏保留";
    EXPECT_EQ(k->query_temp("lu32/finish"), 201) << "未结算则不动连击计数";
}
// ---------------------------------------------------------------------------
// killer_reward 的师徒/组队分流：击杀者是宠物（挂 party/master）时，
// 经验按谁的实战经验算出取决于主人是否在同一场战斗。两条路的 cx 取不同值，
// 只有一条能算出 50 点奖励，从而可观察地证明走了哪条。
// victim 的 last_fainted_from 必须指向该次击杀的主人，否则 nk_gain 的
// “晕倒归属”守卫会先行返回 0，分流逻辑根本无从执行。
// ---------------------------------------------------------------------------
// 主人在线、同队同房间且在打 → 按主人算奖励（主人 cx=100 → 50 点；按宠物算则为
// 0）
TEST_F(RewardNpcKillTest, NpcKill_PetKills_RoutesRewardThroughMaster)
{
    CUser* k = MakeKiller("rw_pet_k");
    k->set("combat_exp", (long)1000);  // 按宠物自己算会被钳成 0 点
    k->set_temp("party/master", "rw_pet_master");
    k->set_temp("fight/fight_team", (long)7);
    k->set_temp("fight/is_fighting", (long)1);
    pk->get()->set("宝物", (long)3);
    pk->get()->set("cp/random", (long)0);
    master = new CUser();
    master->set_name("主人", "rw_pet_master");
    master->set("level", (long)10);
    master->set("combat_exp", (long)100);
    master->set_temp("fight/fight_team", (long)7);
    master->set_temp("fight/is_fighting", (long)1);
    master->move(pk->get());
    g_user_list[master->object_id()] = master;
    CNpc* v = MakeNpcVictim("流寇", "rw_pet_v", (long)60);
    v->set_temp("last_fainted_from", "rw_pet_master");
    g_reward.killer_reward(k, v);
    EXPECT_EQ(k->query("combat_exp"), 1000 + 50)
        << "奖励按主人的实战经验算出后发给击杀者";
    EXPECT_EQ(k->query("kill/nkgain"), 50);
    EXPECT_EQ(k->query("mks"), 1);
}
// 主人在另一场战斗 → 回退按击杀者自己算（宠物 cx=100 → 50 点；按主人算则为 0）
TEST_F(RewardNpcKillTest, NpcKill_PetKills_MasterInOtherFight_UsesKiller)
{
    CUser* k = MakeKiller("rw_pet2_k");
    k->set("combat_exp", (long)100);
    k->set_temp("party/master", "rw_pet2_master");
    k->set_temp("fight/fight_team", (long)7);
    k->set_temp("fight/is_fighting", (long)1);
    pk->get()->set("宝物", (long)3);
    pk->get()->set("cp/random", (long)0);
    master = new CUser();
    master->set_name("主人", "rw_pet2_master");
    master->set("level", (long)10);
    master->set("combat_exp", (long)1000);
    master->set_temp("fight/fight_team", (long)9);  // 不同队伍
    master->set_temp("fight/is_fighting", (long)1);
    master->move(pk->get());
    g_user_list[master->object_id()] = master;
    CNpc* v = MakeNpcVictim("流寇", "rw_pet2_v", (long)60);
    v->set_temp("last_fainted_from", "rw_pet2_k");
    g_reward.killer_reward(k, v);
    EXPECT_EQ(k->query("combat_exp"), 100 + 50)
        << "主人不在同一场战斗，奖励按击杀者自己的实战经验算";
    EXPECT_EQ(k->query("kill/nkgain"), 50);
}
// ==========================================================================
// 组队分红 / PK 罪行 / 档位表 —— 经公共入口（killer_reward/nk_gain/pk_reward）
// 驱动 private 的 distribute_party_exp 与档位阶梯。
// ==========================================================================
class RewardPartyTest : public ::testing::Test {
protected:
    StateRecordingUser* killer;
    CChar* victim;
    CContainer* room;
    std::vector<CUser*> allies;  // 室内盟友（夹具统一回收）
    int m_saved_time = 0;
    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        m_saved_time   = g_current_time;
        g_current_time = 1000000;  // pending/PK、kill_time 可观测
        // 分享用例可能翻转 share_exp，逐用例复位为默认开
        g_ini_data["party"].clear();
        CPartyConfig::init();
        room = new CContainer();
        room->set_name("旷野", "reward_party_room");
        room->set("infinity", 1);
        killer = new StateRecordingUser();
        killer->set_name("豪客", "rk1");
        killer->set("id", "rk1");
        killer->set("level", (long)1);  // 低于受害者才触发声望扣减
        killer->set("combat_exp", (long)0);
        killer->set("repute", (long)0);
        killer->set("hp", (long)100);
        killer->move(room);
        killer->delete_temp("client/loading");
        victim = new CChar();
        victim->set_name("悍匪", "rv1");
        victim->set("id", "rv1");
        victim->set("level", (long)5);
        victim->set("combat_exp", (long)15000);
        victim->set("repute", (long)200);
        victim->set("hp", (long)100);
        victim->move(room);
    }
    void TearDown() override
    {
        for (CUser* a : allies) {
            a->set_heart_beat(0);
            a->move(nullptr);
            delete a;
        }
        allies.clear();
        victim->move(nullptr);
        delete victim;
        killer->move(nullptr);
        g_user_list.erase(killer->object_id());
        delete killer;
        room->move(nullptr);
        delete room;
        g_current_time = m_saved_time;
        g_ini_data["party"].clear();
        CPartyConfig::init();  // 兜底复位共享配置
    }
    // 室内盟友：带战斗中标（进入 nk_gain 的组队收集循环）
    StateRecordingUser* MakeAlly(const char* id, long exp, long team)
    {
        auto* u = new StateRecordingUser();
        u->set_name("盟友", id);
        u->set("id", id);
        u->set("level", (long)10);
        u->set("combat_exp", exp);
        u->set("hp", (long)100);
        u->set_temp("fight/fight_team", team);
        u->set_temp("fight/is_fighting", 1);
        u->move(room);
        u->delete_temp("client/loading");
        allies.push_back(u);
        return u;
    }
};
// ---------------- victim_lose：巫师豁免与高开方分支 ----------------
TEST_F(RewardPartyTest, VictimLose_WizardMismatchAndHighExpBranch)
{
    victim->set("grant", (long)CMainObject::GRANT_WIZARD);
    EXPECT_EQ(0, g_reward.victim_lose(killer, victim))
        << "巫师/凡人混杀不计损失(29)";
    victim->del("grant");
    killer->set("combat_exp", (long)50000);  // 高于 victim → 36 分支
    EXPECT_EQ(25, g_reward.victim_lose(killer, victim));
    killer->set("combat_exp", (long)1000);  // 低于 victim → else 分支
    EXPECT_EQ(126, g_reward.victim_lose(killer, victim));
}
// ---------------- NK 档位表：700000→600 … 100→70（2×档位，封顶 700 不触）
// ----------------
TEST_F(RewardPartyTest, NpcKillReward_TierTable)
{
    killer->set("level", (long)150);  // 封顶 700，2×最高档 600 也不触
    struct {
        long exp;
        int want;
    } cases[] = {{700000, 600}, {400000, 510}, {150000, 390}, {50000, 300},
                 {20000, 198},  {6000, 150},   {3000, 100},   {100, 70}};
    for (auto& c : cases) {
        CChar* v = new CChar();
        v->set_name("野怪", "tier_v");
        v->set("level", (long)5);
        v->set("combat_exp", c.exp);
        killer->set("combat_exp", (long)0);  // cx 归零，档位公式不被上一轮污染
        killer->clear();
        g_reward.killer_reward(killer, v);
        char want[64];
        snprintf(want, sizeof(want), "获得%d点NK经验", c.want);
        EXPECT_TRUE(killer->has(want)) << "victim exp=" << c.exp;
        delete v;
    }
}
// ---------------- nk_gain：非我晕倒的经验拒发 ----------------
TEST_F(RewardPartyTest, NkGain_FaintedByOtherNoParty_Denied)
{
    victim->set_temp("last_fainted_from", "outsider");
    EXPECT_EQ(0, g_reward.nk_gain(killer, victim))
        << "别人打晕的怪，无队者不得经验(183-191)";
}
// ---------------- nk_gain：朋友链放行 + 四人分红 ----------------
TEST_F(RewardPartyTest, NkGain_PartyFriendTeam_DistributesFourWay)
{
    killer->set("combat_exp", (long)0);
    killer->set_temp("party/number", 3);
    killer->set_temp("fight/fight_team", 1);
    killer->set_temp("fight/is_fighting", 1);
    victim->set_temp("last_fainted_from", "outsider");  // 走 194 朋友分支
    victim->set("combat_exp", (long)700000);            // 高档位 300
    auto* outsider                     = MakeAlly("outsider", 100, 1);
    auto* a2                           = MakeAlly("rp_ally2", 200, 1);
    auto* a3                           = MakeAlly("rp_ally3", 300, 1);
    g_user_list[outsider->object_id()] = outsider;  // find_online 回查
    killer->set_temp("party/friend_outsider", 1);
    killer->set_temp("party/friend_rp_ally2", 1);
    killer->set_temp("party/friend_rp_ally3", 1);
    // 收集循环要跳过：非角色物件与非玩家角色
    auto* bystander = new CChar();
    bystander->set_name("路人", "rw_by");
    bystander->move(room);
    auto* trash = new CItem();
    trash->set_name("杂物", "rw_trash");
    trash->move(room);
    EXPECT_EQ(0, g_reward.nk_gain(killer, victim))
        << "组队分红型击杀不把 reward 回传(273-276)";
    EXPECT_EQ(-20, killer->query("repute")) << "低等级击杀高声望者扣声望(231)";
    EXPECT_GT(outsider->query("combat_exp"), 100) << "队员1分得经验";
    EXPECT_TRUE(outsider->has("NK经验"));
    EXPECT_GT(a2->query("combat_exp"), 200) << "队员2分得经验";
    EXPECT_GT(a3->query("combat_exp"), 300) << "队员3分得经验";
    EXPECT_EQ(0, killer->query_temp("party/battle_count"))
        << "分红后清计数(159)";
    g_user_list.erase(outsider->object_id());
    bystander->move(nullptr);
    delete bystander;
    trash->move(nullptr);
    delete trash;
}
// ---------------- nk_gain：队长链路放行后独享 ----------------
TEST_F(RewardPartyTest, NkGain_PartyMasterBranch_SoloReward)
{
    killer->set_temp("party/master", "rm1");
    killer->set_temp("fight/fight_team", 1);
    killer->set_temp("fight/is_fighting", 1);
    victim->set_temp("last_fainted_from", "rm1");
    victim->set("combat_exp", (long)700000);
    auto* master                     = MakeAlly("rm1", 500, 1);
    g_user_list[master->object_id()] = master;
    EXPECT_GT(g_reward.nk_gain(killer, victim), 0)
        << "队长链路校验通过后独享奖励(213-226)";
    EXPECT_EQ(-20, killer->query("repute"));
    g_user_list.erase(master->object_id());
}
// ---------------- nk_gain：share_exp 关闭与夫妻豁免 ----------------
TEST_F(RewardPartyTest, NkGain_ShareExpOff_PartyDenied_CoupleExempt)
{
    g_ini_data["party"]["share_exp"] = "no";
    CPartyConfig::init();
    killer->set_temp("party/number", 2);
    victim->set_temp("last_fainted_from", "rk1");
    EXPECT_EQ(0, g_reward.nk_gain(killer, victim))
        << "关闭分享且组队 → 不发经验(174-177)";
    killer->set_temp("party/couple", 1);  // 夫妻组队豁免
    victim->set("combat_exp", (long)700000);
    EXPECT_GT(g_reward.nk_gain(killer, victim), 0) << "夫妻组队不受限";
    g_ini_data["party"].clear();
    CPartyConfig::init();
}
// ---------------- pk_reward：结算钩子派发（pk_reward 钩子契约）
// ---------------- 罪行档位与悬捕条件归数据集实现；C++ 只保留
// guilt/repute/播报并派发
// combat_rule("pk_reward")。断言：恰好派发一次、actor/target 正确、
// 派发时 kill/guilt 已是本次更新后的终值。
TEST_F(RewardPartyTest, PkReward_DispatchesPkRewardHook)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(luaL_dostring(L,
                            "__orig_cr = hooks.combat_rule;"
                            "pkp_calls = 0; pkp_guilt = -1; pkp_vmark = -1;"
                            "hooks.combat_rule = function(phase,a,t,d)"
                            "  if phase == 'pk_reward' then"
                            "    pkp_calls = pkp_calls + 1"
                            "    pkp_guilt = a:query('kill/guilt')"
                            "    pkp_vmark = t:query_temp('pkp/victim_mark')"
                            "    a:set_temp('pkp/dispatched', 1)"
                            "  end"
                            "  if __orig_cr then "
                            "return __orig_cr(phase,a,t,d) end"
                            ";end"),
              LUA_OK);
    auto lua_long = [L](const char* name) -> long {
        lua_getglobal(L, name);
        long v = (long)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return v;
    };

    victim->set_temp("pkp/victim_mark", 7);
    killer->set_temp("pending/PK_city", "临安城");
    g_reward.pk_reward(killer, victim);

    EXPECT_EQ(50, killer->query("kill/guilt")) << "城内 PK +50";
    EXPECT_GT(killer->query("kill/free"), 0) << "开启 24h 犯罪计时";
    EXPECT_EQ(0, victim->query("kill/guilt")) << "旧罪被清";
    EXPECT_TRUE(killer->has("你PK玩家")) << "声望播报";
    EXPECT_EQ(0, killer->query("repute")) << "(vi-ki)*10 恒 0";

    EXPECT_EQ(1, (int)killer->query_temp("pkp/dispatched"))
        << "pk_reward 派发且 actor=killer（标记落在 killer 上）";
    EXPECT_EQ(1, (int)lua_long("pkp_calls")) << "每次 pk_reward 恰好派发一次";
    EXPECT_EQ(7, (int)lua_long("pkp_vmark")) << "target=victim";
    EXPECT_EQ(50, (int)lua_long("pkp_guilt"))
        << "派发时 kill/guilt 已是更新后终值（先更新后派发）";

    EXPECT_EQ(
        luaL_dostring(L, "hooks.combat_rule = __orig_cr; __orig_cr = nil"),
        LUA_OK);
}
// ---------------- killer_reward：PK 结算（kill_settle 放行 → 双倍损失）
// ----------------
TEST_F(RewardPartyTest, KillerReward_PkSettle_TitlesAndRoomTell)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(luaL_dostring(L,
                            "__orig_cr = hooks.combat_rule;"
                            "hooks.combat_rule = function(phase,a,t,d)"
                            "  if phase == 'kill_settle' then "
                            "return {yes=true} end"
                            "  if __orig_cr then "
                            "return __orig_cr(phase,a,t,d) end"
                            ";end"),
              LUA_OK);

    // PK 结算只对玩家-玩家生效：受害者必须是玩家
    auto* pvic = new StateRecordingUser();
    pvic->set_name("仇家", "rw_pk_v");
    pvic->set("id", "rw_pk_v");
    pvic->set("level", (long)10);
    pvic->set("combat_exp", (long)15000);
    pvic->set("family/family_name", "丐帮");
    pvic->move(room);
    pvic->delete_temp("client/loading");

    room->set("PK", 1);  // pk_chang → tell_room 分支
    killer->set("level", (long)10);
    killer->set("family/family_name", "武当");
    pvic->set("kill/last_killer", "rk1");  // 354 覆写 gain（随后 355 归零）
    pvic->set("kill/last_lose", (long)200);
    killer->set_temp("pending/PK", 100);
    pvic->set_temp("pending/PK", 50);  // vi<ki → pk=0 → 387 短路

    g_reward.killer_reward(killer, pvic);
    EXPECT_EQ(15000 - 252, pvic->query("combat_exp"))
        << "lose=126 经 kill_settle 双倍后入账(461)";
    EXPECT_EQ(252, pvic->query("kill/pklose"));
    EXPECT_EQ(1, pvic->query("pds"));
    EXPECT_EQ(0, pvic->query_temp("pending/PK")) << "双侧 PK 标被清(381-383)";
    EXPECT_EQ(0, killer->query_temp("pending/PK"));
    EXPECT_EQ(0, killer->query("kill/free"))
        << "387：pkreward=1 时不追打 pk_reward";
    EXPECT_EQ(g_current_time, pvic->query("kill/kill_time"));
    EXPECT_EQ(0, killer->query("combat_exp"))
        << "355 无条件把 gain 归零（实测语义）";
    EXPECT_TRUE(killer->has("杀死仇敌"))
        << "pk_chang → tell_room 播报(400,409)";

    // 阶段B：kill_settle 不命中 → pkreward=0；仅凶手带 PK 标 → 387 追打
    EXPECT_EQ(luaL_dostring(L,
                            "hooks.combat_rule = function(phase,a,t,d)"
                            "  if phase == 'kill_settle' then "
                            "return {yes=false} end"
                            "  if __orig_cr then "
                            "return __orig_cr(phase,a,t,d) end"
                            ";end"),
              LUA_OK);
    killer->set_temp("pending/PK", 100);
    g_reward.killer_reward(killer, pvic);
    EXPECT_GT(killer->query("kill/free"), 0)
        << "387：pkreward=0 且 pk=1 → 走 pk_reward";
    EXPECT_EQ(
        luaL_dostring(L, "hooks.combat_rule = __orig_cr; __orig_cr = nil"),
        LUA_OK);

    pvic->move(nullptr);
    g_user_list.erase(pvic->object_id());
    delete pvic;
}
TEST_F(RewardPartyTest, NpcKillReward_HookBonusOverrides)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(luaL_dostring(L,
                            "__orig_kb = hooks.kill_bonus;"
                            "hooks.kill_bonus = function(k,v,r)"
                            "  return {reward = r + 7}"
                            "end"),
              LUA_OK);
    killer->set("level", (long)150);
    CChar* v = new CChar();
    v->set_name("野怪", "tier_h");
    v->set("level", (long)5);
    v->set("combat_exp", (long)100);  // 档 35 → reward=70 → 钩子 +7
    g_reward.killer_reward(killer, v);
    EXPECT_EQ(77, killer->query("combat_exp")) << "脚本 reward 覆盖入账(429)";
    EXPECT_TRUE(killer->has("获得77点NK经验"));
    EXPECT_EQ(luaL_dostring(L, "hooks.kill_bonus = __orig_kb; __orig_kb = nil"),
              LUA_OK);
    delete v;
}
// ---------------- kill_bonus：奖励为 0 时挂点仍被咨询（固定奖励不失效）
// ---------------- 击杀奖励是数据集规则（如 1 级练级山鸡固定 10000
// 经验），不得因 nk_gain 的上游因素（组队分红回传 0、晕倒归属拒发、高 cx
// 经验折算钳成 0）算出 0 而失效：handle_npc_kill_reward 必须先咨询 kill_bonus
// 挂点，脚本返回 固定奖励即可入账并发提示；之后 (仍) 为 0
// 的奖励保持静默——不入账、不 发提示、挂点缺席时行为与从前一致。
TEST_F(RewardPartyTest, NpcKillReward_ZeroReward_StillConsultsHook)
{
    ensure_lua_env();
    lua_State* L  = CLuaEngine::instance()->get_state();
    auto kb_calls = [&]() -> long {
        long n = -1;
        if (luaL_dostring(L, "return kb_calls or -1") == LUA_OK)
            n = (long)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return n;
    };
    killer->set("level", (long)150);           // NK 封顶 700
    killer->set("combat_exp", (long)1000000);  // cx 远高 → 折算奖励 0
    CChar* v = new CChar();
    v->set_name("山鸡", "zero_v");
    v->set("level", (long)1);
    v->set("combat_exp", (long)10);  // 档位 NK 基数仅 35 → times 钳成 0

    // 挂点缺席：0 奖励静默（不入账、不发提示）
    g_reward.killer_reward(killer, v);
    EXPECT_EQ(1000000, killer->query("combat_exp"))
        << "无挂点覆盖则 0 奖励不入账";
    EXPECT_FALSE(killer->has("NK经验")) << "无挂点覆盖则 0 奖励不发提示";

    // 挂点在场：C++ 算出 0 也必须被咨询，固定奖励照常入账并发提示
    EXPECT_EQ(luaL_dostring(L,
                            "kb_calls = 0;"
                            "__orig_kb = hooks.kill_bonus;"
                            "hooks.kill_bonus = function(k,v,r)"
                            "  kb_calls = kb_calls + 1"
                            "  return {reward = 10000}"
                            "end"),
              LUA_OK);
    killer->clear();
    g_reward.killer_reward(killer, v);
    EXPECT_EQ(1, kb_calls())
        << "0 奖励也必须咨询挂点，固定奖励不得因上游因素失效";
    EXPECT_EQ(1000000 + 10000, killer->query("combat_exp"))
        << "挂点写入的固定奖励入账（不按 nk_gain 的 0 丢弃）";
    EXPECT_TRUE(killer->has("获得10000点NK经验"));
    EXPECT_EQ(luaL_dostring(L,
                            "hooks.kill_bonus = __orig_kb; __orig_kb = nil;"
                            "kb_calls = nil"),
              LUA_OK);
    delete v;
}
// ---------------- killer_reward：凡人杀玩家 → 惩罚 + 谣言 ----------------
TEST_F(RewardPartyTest, KillerReward_NpcSlaysUser_PenaltyApplied)
{
    auto* vic = new StateRecordingUser();
    vic->set_name("倒霉蛋", "rw_vic");
    vic->set("level", (long)10);
    vic->set("combat_exp", (long)15000);
    vic->move(room);
    vic->delete_temp("client/loading");
    auto* butcher = new CChar();
    butcher->set_name("屠夫", "rw_but");
    butcher->move(room);
    g_reward.killer_reward(butcher, vic);  // 470 的 rumor 广播随之执行
    EXPECT_EQ(93, vic->query("death/combat_exp_loss"))
        << "(15000/40)/4 = 93(469)";
    EXPECT_EQ(15000 - 93, vic->query("combat_exp"));
    butcher->move(nullptr);
    delete butcher;
    vic->move(nullptr);
    g_user_list.erase(vic->object_id());
    delete vic;
}
