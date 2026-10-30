// test_combat_rules.cpp — 战斗规则询问点（CombatRules.cpp /
// hooks.combat_rule）测试
//
// 契约：C++ 只申报引擎时机（phase），规则归属数据集全在 Lua：
//   attack_gate：默认数据集（lua-default/data/hooks.lua）实现新手保护——
//   玩家互斗任一方等级 <75 → {reason="才入江湖，何必动刀动枪。"}，NPC 不受此限；
//   询问点收拢在 CChar::cmd_hit_ob，kill / fight_ob / AutoFight 一切动手路径
//   统一在此问一次（kill 命令层不自行询问）。
//   guard_response / kill_settle 由自定义数据集规则给出（本文件用例内改写）。
// 另验证可扩展性：另一数据集可在同一 phase 追加规则而 C++ 零改动。
// 钩子缺席回退由 CLuaHookCall 统一保证（call 失败按 nil）。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "CombatRules.h"
#include "Global.h"
#include "LuaEngine.h"
#include "Room.h"
#include "RoomMgr.h"
#include "User.h"
#include "mock/mock_database.h"
#include "test_lua_env.h"
#include "test_scoped_room.h"

namespace {

class CombatRulesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        // 用例会临时改写 hooks.combat_rule：进出本套件都必须是数据集实现
        ASSERT_EQ(luaL_dostring(CLuaEngine::instance()->get_state(),
                                "__orig_cr = hooks.combat_rule"),
                  LUA_OK);
    }

    void TearDown() override
    {
        EXPECT_EQ(luaL_dostring(CLuaEngine::instance()->get_state(),
                                "if __orig_cr then "
                                "hooks.combat_rule = __orig_cr end "
                                "__orig_cr = nil"),
                  LUA_OK);
    }

    static CUser* MakeUser(const char* name, const char* id)
    {
        CUser* u = new CUser();
        u->set_name(name, id);
        u->set("id", id);
        // 过数据集默认 attack_gate 的 75 级新手门：本套件的用例关心的是
        // 各自 phase 的规则命中，等级门不希望在此被意外触发。
        u->set("level", (long)80);
        return u;
    }
};

// 需要真实 g_db 的用例：fixture 注入 MockDatabase 并在 TearDown 复位，
// 致命断言提前返回也不会留下悬垂的 g_db。
class CombatRulesDbTest : public CombatRulesTest {
protected:
    void SetUp() override
    {
        CombatRulesTest::SetUp();
        g_db = &db;
    }

    void TearDown() override
    {
        g_db = nullptr;
        CombatRulesTest::TearDown();
    }

    MockDatabase db;
};

}  // namespace

// ---------------------------------------------------------------------------
// attack_gate：否决型询问（{reason=文案}）
// ---------------------------------------------------------------------------
TEST_F(CombatRulesTest, AttackGate_CorpsVictim_Allowed)
{
    CUser* me     = MakeUser("杀手", "ag_atk2");
    CUser* victim = MakeUser("帮众", "ag_vic2");
    victim->set("corps/id", "ag_corps");
    victim->set("corps/rank", "帮众");

    EXPECT_TRUE(combat_rule_reason("attack_gate", me, victim).empty());

    delete me;
    delete victim;
}

TEST_F(CombatRulesTest, AttackGate_HighPksVictim_Allowed)
{
    CUser* me     = MakeUser("杀手", "ag_atk3");
    CUser* victim = MakeUser("恶人", "ag_vic3");
    victim->set("pks", (long)5);

    EXPECT_TRUE(combat_rule_reason("attack_gate", me, victim).empty());

    delete me;
    delete victim;
}

// 默认数据集规则：玩家互斗任一方等级 <75 即否决（lua-default 的 attack_gate 声明）
TEST_F(CombatRulesTest, AttackGate_DefaultNewbieGate_LowLevelRejected)
{
    CUser* me     = MakeUser("生手", "ag_atk4");
    CUser* victim = MakeUser("新手", "ag_vic4");
    me->set("level", (long)50);

    EXPECT_EQ(combat_rule_reason("attack_gate", me, victim),
              "才入江湖，何必动刀动枪。")
        << "默认规则给出与原硬编码门一致的文案";

    delete me;
    delete victim;
}

// 默认规则只管玩家互斗：NPC 进攻玩家不受等级门限制（E2E 的 /kill 山鸡等路径）
TEST_F(CombatRulesTest, AttackGate_DefaultRule_NpcAttackerAllowed)
{
    CUser* victim = MakeUser("行路客", "ag_vic5");
    CChar* npc    = new CChar();
    npc->set_name("山鸡", "shiji_t");

    EXPECT_TRUE(combat_rule_reason("attack_gate", npc, victim).empty());

    delete npc;
    delete victim;
}

// ---------------------------------------------------------------------------
// guard_response：命中型询问（{yes=true}）
// ---------------------------------------------------------------------------
TEST_F(CombatRulesTest, GuardResponse_NoCorps_NoHit)
{
    CUser* me     = MakeUser("散客", "gr_atk2");
    CUser* victim = MakeUser("路人", "gr_vic2");

    EXPECT_FALSE(combat_rule_yes("guard_response", me, victim));

    delete me;
    delete victim;
}

TEST_F(CombatRulesTest, GuardResponse_Stranger_NoHit)
{
    ScopedRoom room("gr_corps3");
    ASSERT_NE(room.get(), nullptr);

    CUser* me     = MakeUser("帮徒", "gr_atk3");
    CUser* victim = MakeUser("无辜", "gr_vic3");
    me->set("corps/id", "gr_corps3");

    EXPECT_FALSE(combat_rule_yes("guard_response", me, victim));

    delete me;
    delete victim;
}

// ---------------------------------------------------------------------------
// kill_settle：命中型询问 + 总舵 enemy 名单递减
// ---------------------------------------------------------------------------
TEST_F(CombatRulesTest, KillSettle_NoCorps_NoHit)
{
    CUser* killer = MakeUser("散客", "ks_atk3");
    CUser* victim = MakeUser("路人", "ks_vic3");

    EXPECT_FALSE(combat_rule_yes("kill_settle", killer, victim));

    delete killer;
    delete victim;
}

TEST_F(CombatRulesDbTest, KillSettle_NotListed_NoHit)
{
    db.SetQueryResultMatching("obj_lock", MockDatabase::MakeSingleRow({"1"}));

    ScopedRoom room("ks_corps4");
    ASSERT_NE(room.get(), nullptr);

    CUser* killer = MakeUser("帮徒", "ks_atk4");
    CUser* victim = MakeUser("无辜", "ks_vic4");
    killer->set("corps/id", "ks_corps4");

    EXPECT_FALSE(combat_rule_yes("kill_settle", killer, victim));

    delete killer;
    delete victim;
}

// ---------------------------------------------------------------------------
// join_battle：NPC 方代表筛选（{yes=true} = 加入筛选跳过该对象）
// ---------------------------------------------------------------------------
TEST_F(CombatRulesTest, JoinBattle_MingjiaoWithoutFlag_NotExempt)
{
    CUser* me = MakeUser("参战者", "jb_atk4");
    CChar* mj = new CChar();
    mj->set_name("明教弟子", "jb_mj2");
    mj->set("family/family_name", "明教");

    EXPECT_FALSE(combat_rule_yes("join_battle", me, mj))
        << "剧情未开战时不豁免";

    delete mj;
    delete me;
}

// ---------------------------------------------------------------------------
// 可扩展性：另一数据集在同一 phase 追加规则，C++ 原语零改动
// ---------------------------------------------------------------------------
TEST_F(CombatRulesTest, UnknownPhase_NoOpinion)
{
    CUser* me     = MakeUser("甲", "up_a");
    CUser* victim = MakeUser("乙", "up_b");

    // 引擎未申报的 phase：Lua 无分支可走 → 空串 / false
    EXPECT_TRUE(combat_rule_reason("no_such_phase", me, victim).empty());
    EXPECT_FALSE(combat_rule_yes("no_such_phase", me, victim));

    delete me;
    delete victim;
}

TEST_F(CombatRulesTest, SecondDatasetRule_AddsReasonWithoutCppChange)
{
    CUser* me     = MakeUser("杀手", "ds_atk");
    CUser* victim = MakeUser("封禁者", "ds_vic");
    victim->set("test_ban", (long)1);  // 另一数据集的自有键

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_cr = hooks.combat_rule\n"
                            "hooks.combat_rule = function(phase, actor, "
                            "target)\n"
                            "  if phase == 'attack_gate' and "
                            "target:query('test_ban') ~= 0 then\n"
                            "    return { reason = '此人受契约保护。' }\n"
                            "  end\n"
                            "  return __orig_cr(phase, actor, target)\n"
                            "end"),
              LUA_OK);

    // 新规则先命中（良民规则因 pks<5 本也会拒绝，这里用高 pks 排除干扰）
    victim->set("pks", (long)9);
    EXPECT_EQ(combat_rule_reason("attack_gate", me, victim),
              "此人受契约保护。");

    delete me;
    delete victim;
}
