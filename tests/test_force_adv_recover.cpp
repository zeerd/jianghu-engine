// test_hitmsg_sample_recover.cpp - 复元术（recover）可用性回归测试
//
// 目标：证明 base="force" 的 Lua 技能脚本在 create() 之后，真的能施放复元术。
//
// 背景：复元术由 C++ 基类 CForce::create() 通过 add_magic("recover",
// do_recover) 注册进 m_MagicList（带真实函数指针），并非由 Lua 脚本 add_magic
// 注册。 因此 Cast("recover") 会走 CLuaSkillT::Cast 的"未命中 Lua 法术表 → 回退
// CSkill::Cast → 命中 m_MagicList → 调用 CForce::do_recover"这条真实路径。
//
// 这条测试同时充当"去掉 CLuaSkillT::create 中强制 T::create()"重构的守卫：
//   - 重构前：wrapper 无条件先跑 T::create()，复元术必然注册 → 通过。
//   - 重构后：wrapper 改为回退语义，hitmsg_sample.lua 需在 create 中显式
//     self:base_create() 才会注册复元术 → 若漏加，本测试失败。
//
// 脚本来源：test_config.ini 的 script_root = lua-sample，
//          → lua-sample/skill/hitmsg_sample.lua（base = "force"）。
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Container.h"
#include "Force.h"     // CForce::hit_ob / do_recover
#include "LuaSkill.h"  // CLuaForce / CLuaSkillT
#include "IniParser.h"
#include "ServerConfig.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"  // 玩家出手扣蓝的观察面

// ====================================================================
// Test fixture：初始化真实 Lua 引擎（lua-sample + lua-default）
// ====================================================================
class ForceAdvRecoverTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    void TearDown() override {}

    // 造一个"内力充沛、已受伤、会基本内功"的角色，满足 do_recover 前置条件
    static CChar* MakeWoundedChar()
    {
        CChar* me = new CChar();
        me->set("mp", (long)200);
        me->set("max_mp", (long)200);
        me->set("hp", (long)50);
        me->set("eff_hp", (long)100);
        me->set("max_hp", (long)100);
        me->set_skill("force", 100);
        return me;
    }
};

// ====================================================================
// 核心用例：create() 后复元术真的可用（能施放 + 回血 + 扣内力）
// ====================================================================
TEST_F(ForceAdvRecoverTest, Recover_Usable_AfterCreate)
{
    CLuaForce* skill = new CLuaForce("hitmsg_sample");
    skill->create();

    CChar* me         = MakeWoundedChar();
    int32_t hp_before = me->query("hp");
    int32_t mp_before = me->query("mp");

    // "recover" 不在 Lua 法术表，Cast 回退 CSkill::Cast 命中基类注册的法术
    int ret = skill->cast("recover", me, NULL);

    EXPECT_EQ(ret, 1)
        << "复元术应能成功施放（证明 CForce::create 已注册 recover）";
    EXPECT_GT(me->query("hp"), hp_before) << "施放复元术后 hp 应上升";
    EXPECT_LT(me->query("mp"), mp_before) << "施放复元术应消耗内力";

    delete me;
    delete skill;
}

// ====================================================================
// 反向锚定：未 create() 时复元术不可用（证明上一条断言非恒真）
// ====================================================================
TEST_F(ForceAdvRecoverTest, Recover_Missing_WithoutCreate)
{
    CLuaForce* skill = new CLuaForce("hitmsg_sample");
    // 故意不调用 create()：m_MagicList / m_luaMagicList 均为空

    CChar* me = MakeWoundedChar();
    int ret   = skill->cast("recover", me, NULL);

    EXPECT_EQ(ret, 0) << "未 create 时不应存在 recover 法术";
    EXPECT_EQ(me->query("hp"), 50) << "未施放成功则 hp 不变";

    delete me;
    delete skill;
}

// ====================================================================
// 脚本侧法术完整性：create() 后 Lua 自注册的 heal/roar 仍在
// （证明"基类 create"与"脚本 create"两者都执行了）
// ====================================================================
TEST_F(ForceAdvRecoverTest, Lua_Magics_Intact_AfterCreate)
{
    CLuaForce* skill = new CLuaForce("hitmsg_sample");
    skill->create();

    // hitmsg_sample.lua 通过 add_magic 注册了 heal / roar 两个 Lua 法术
    EXPECT_GE(skill->get_magic_count(), 2)
        << "脚本 create 应注册 heal/roar 两个 Lua 法术";

    std::string names;
    for (int i = 0; i < skill->get_magic_count(); i++) {
        names += skill->get_magic_name(i);
        names += ",";
    }
    EXPECT_NE(names.find("heal"), std::string::npos) << "应包含 heal";
    EXPECT_NE(names.find("roar"), std::string::npos) << "应包含 roar";

    delete skill;
}

// ====================================================================
// Phase A 查询接口契约：query_magic_hostile / query_magic_target
//   复元术由 C++ 基类注册为自身调理（非敌意、CAST_TO_ME）；
//   未注册法术保守按敌意返回 1、target 返回 CAST_TO_RANDOM。
// ====================================================================
TEST_F(ForceAdvRecoverTest, MagicQueryContract)
{
    CLuaForce* skill = new CLuaForce("hitmsg_sample");
    skill->create();

    EXPECT_EQ(skill->query_magic_hostile("recover"), 0)
        << "复元术是自身调理，应标非敌意";
    EXPECT_EQ(skill->query_magic_target("recover"), CSkill::CAST_TO_ME);

    // 未注册法术：hostile 保守按敌意，target 落回 CAST_TO_RANDOM
    EXPECT_EQ(skill->query_magic_hostile("no_such_magic"), 1);
    EXPECT_EQ(skill->query_magic_target("no_such_magic"),
              CSkill::CAST_TO_RANDOM);

    delete skill;
}

// ====================================================================
// 施法内功前置开关 force_no_need_prepare：
//   关（默认）-> 法术要求特定内功，未装备则拒绝；
//   开         -> 只要学会了该内功（无论是否装备）即放行。
// ====================================================================
namespace {

void ApplyPrepareSwitch(bool force_no_need_prepare)
{
    ini_load("tests/test_config.ini");
    g_ini_data["server"]["force_no_need_prepare"]
        = force_no_need_prepare ? "1" : "0";
    CServerConfig::init();
}

// 法术限定必须配合“基本内功”，但角色未把内功装备到 force 槽
CLuaForce* MakeRestrictedSkill()
{
    CLuaForce* skill = new CLuaForce("hitmsg_sample");
    skill->create();
    skill->set("valid_force", "force");
    return skill;
}

}  // namespace

TEST_F(ForceAdvRecoverTest, PrepareGate_BlocksWhenSwitchOff)
{
    ApplyPrepareSwitch(false);

    CLuaForce* skill = MakeRestrictedSkill();
    CChar* me        = MakeWoundedChar();

    EXPECT_EQ(skill->cast("recover", me, NULL), 0)
        << "未装备限定内功时应被施法前置拦住";

    delete me;
    delete skill;

    ApplyPrepareSwitch(false);  // 还原默认值
}

TEST_F(ForceAdvRecoverTest, PrepareGate_SkippedWhenSwitchOn)
{
    ApplyPrepareSwitch(true);

    CLuaForce* skill  = MakeRestrictedSkill();
    CChar* me         = MakeWoundedChar();
    int32_t hp_before = me->query("hp");

    EXPECT_EQ(skill->cast("recover", me, NULL), 1)
        << "force_no_need_prepare 打开时，已学会内功即可施法";
    EXPECT_GT(me->query("hp"), hp_before);

    delete me;
    delete skill;

    ApplyPrepareSwitch(false);  // 还原默认值
}

// ====================================================================
// hit_ob：内功反震结算 —— 玩家扣蓝、档位夹取、负伤反震与伤害反加
// ====================================================================
TEST_F(ForceAdvRecoverTest, HitOb_UserAttacker_DeductsMpAndAccruesBonus)
{
    auto* me = new StateRecordingUser();  // userp → 43 行先扣 mp_factor
    me->set_name("内家", "fo_me1");
    me->set("mp", (long)900);
    me->set("mp_factor", (long)30);
    me->set_skill("force", 1);  // random(1) 恒 0 → 必入 bonus 累加
    CChar* vic = new CChar();
    vic->set_name("靶子", "fo_v1");
    vic->set("mp", (long)0);
    vic->set_skill("force", 0);

    CForce f;
    int32_t bonus = 0;
    EXPECT_EQ(nullptr, f.hit_ob(me, vic, bonus));
    EXPECT_EQ(870, me->query("mp")) << "出手先扣 mp_factor(43)";
    EXPECT_EQ(59, bonus) << "870/30+30-0=59 累加进 damage_bonus(86-87)";

    // 负 bonus 输入：bonus+damage<0 → 归零(84/85)
    int32_t bonus2 = -100;
    EXPECT_EQ(nullptr, f.hit_ob(me, vic, bonus2));
    EXPECT_EQ(0, bonus2) << "负和归零";

    me->move(nullptr);
    delete me;
    delete vic;
}

TEST_F(ForceAdvRecoverTest, HitOb_NpcClampAndNegativeNoReversal)
{
    // 非玩家出手不扣蓝；受害者内力被 3×mp 夹取(51/53)后伤害为负、
    // 且反震条件确定不成立 → 负值只落进 damage_bonus 再归零(58/59/77/78)
    CChar* me = new CChar();
    me->set_name("铁人", "fo_me2");
    me->set("hp", (long)100);
    me->set("mp", (long)300);
    me->set("mp_factor", (long)-100);
    me->set_skill("force", 0);
    CChar* vic = new CChar();
    vic->set_name("厚朴", "fo_v2");
    vic->set("mp", (long)1000);
    vic->set_skill("force", 0);  // random(1) 恒 0 → 0 > 0 恒假，不反震

    CForce f;
    int32_t bonus = 0;
    EXPECT_EQ(nullptr, f.hit_ob(me, vic, bonus));
    EXPECT_EQ(0, bonus) << "负伤害进 bonus 后归零(77/78)";
    EXPECT_EQ(300, me->query("mp")) << "非玩家不扣蓝(43 短路)";
    EXPECT_EQ(100, me->query("hp")) << "未反震不掉血";

    delete me;
    delete vic;
}

TEST_F(ForceAdvRecoverTest, HitOb_ReversalChatTiers)
{
    // mp/30 + factor < 0 触发反震：|damage| 落四档播报(66/68/70/else)
    struct {
        long factor;
        const char* want;
    } cases[] = {
        {-11, "闷哼"}, {-20, "退了两步"}, {-30, "五六步"}, {-100, "眼前一黑"}};
    CChar* vic = new CChar();
    vic->set_name("厚朴", "fo_v3");
    vic->set("mp", (long)0);
    vic->set_skill("force", 30);  // random(31)>0 → 单次命中 30/31

    for (auto& c : cases) {
        CChar* me = new CChar();
        me->set_name("铁人", "fo_me3");
        me->set("mp", (long)300);
        me->set("mp_factor", c.factor);
        me->set_skill("force", 0);
        CForce f;
        int32_t bonus = 0;
        char* r       = nullptr;
        for (int i = 0; i < 30 && !r; i++) {  // 随机反震：有限次收敛
            r = f.hit_ob(me, vic, bonus);
        }
        ASSERT_NE(nullptr, r) << "factor=" << c.factor << " 应触发反震";
        EXPECT_NE(nullptr, strstr(r, c.want)) << "factor=" << c.factor;
        EXPECT_LT(me->query("hp"), 100) << "反震掉血(63/64)";
        delete me;
    }
    delete vic;
}

// ====================================================================
// do_recover：内力不足时按现有内力缩比治疗(104/105)
// ====================================================================
TEST_F(ForceAdvRecoverTest, Recover_LowMp_ScalesHealDown)
{
    CChar* me = MakeWoundedChar();
    // query_skill 无 raw 参数返回 raw/2：置 200 → 可见值 100 → n=50
    me->set_skill("force", 200);
    me->set("mp", (long)25);  // 25 < 50 → 走缩比支

    EXPECT_EQ(1, CForce::do_recover(me, NULL));
    EXPECT_EQ(0, me->query("mp")) << "n 收敛为现有 mp → 扣光(105)";
    EXPECT_EQ(75, me->query("hp")) << "q=50*25/50=25 治疗量(104)";

    delete me;
}
