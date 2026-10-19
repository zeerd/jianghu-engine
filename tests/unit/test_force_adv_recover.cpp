// test_hitmsg_sample_recover.cpp - 复元术（recover）可用性回归测试
//
// 目标：证明 base="force" 的 Lua 技能脚本在 create() 之后，真的能施放复元术。
//
// 背景：复元术由基本内功的 Lua 脚本（lua-default/skill/basic/force.lua）用
//        add_magic("recover", …, "do_recover") 注册。本技能声明可当内功用
//        （valid_enable），引擎在 create 收尾时把基础内功的法术继承过来，
//        因此 Cast("recover") 走"命中继承项 → 转发归属技能 → 执行基本内功
//        脚本里的 do_recover"这条真实路径。
//
// 这条测试同时充当"法术继承机制"的守卫：
//   - 基础内功脚本漏掉 add_magic("recover", …) → 继承不到 → 本测试失败；
//   - 技能漏声明 valid_enable → 继承不到 → 本测试失败。
//
// 脚本来源：test_config.ini 的 script_root = lua-sample，
//          → lua-sample/skill/hitmsg_sample.lua（base = "force"）。
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Container.h"
#include "LuaSkill.h"  // CLuaSkill
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
    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
    skill->create();

    CChar* me         = MakeWoundedChar();
    int32_t hp_before = me->query("hp");
    int32_t mp_before = me->query("mp");

    // "recover" 继承自基本内功 → 转发到基本内功脚本的 do_recover
    int ret = skill->cast("recover", me, NULL);

    EXPECT_EQ(ret, 1) << "复元术应能成功施放（证明已继承到基本内功的 recover）";
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
    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
    // 故意不调用 create()：法术表为空

    CChar* me = MakeWoundedChar();
    int ret   = skill->cast("recover", me, NULL);

    EXPECT_EQ(ret, 0) << "未 create 时不应存在 recover 法术";
    EXPECT_EQ(me->query("hp"), 50) << "未施放成功则 hp 不变";

    delete me;
    delete skill;
}

// ====================================================================
// 脚本侧法术完整性：create() 后 Lua 自注册的 heal/roar 仍在，
// 且继承来的 recover 也在（证明"脚本 create"与"法术继承"两者都执行了）
// ====================================================================
TEST_F(ForceAdvRecoverTest, Lua_Magics_Intact_AfterCreate)
{
    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
    skill->create();

    // hitmsg_sample.lua 通过 add_magic 注册了 heal / roar 两个 Lua 法术
    EXPECT_GE(skill->get_magic_count(), 3)
        << "脚本 create 应注册 heal/roar，并继承到基本内功的 recover";

    std::string names;
    for (int i = 0; i < skill->get_magic_count(); i++) {
        names += skill->get_magic_name(i);
        names += ",";
    }
    EXPECT_NE(names.find("heal"), std::string::npos) << "应包含 heal";
    EXPECT_NE(names.find("roar"), std::string::npos) << "应包含 roar";
    EXPECT_NE(names.find("recover"), std::string::npos)
        << "应包含继承来的 recover";

    delete skill;
}

// ====================================================================
// 回退语义：脚本未定义 create 时，技能名回落为脚本名（数据键）
//   便于定位是哪个脚本缺 create（不再有"按族给默认中文名"的 C++ 兜底）
// ====================================================================
TEST_F(ForceAdvRecoverTest, ScriptWithoutCreate_FallsBackToScriptName)
{
    CLuaSkill* skill = new CLuaSkill("tc_force");
    skill->create();

    EXPECT_STREQ("tc_force", skill->name())
        << "无 create 的脚本，技能名回落为脚本名";
    EXPECT_EQ(0, skill->get_magic_count())
        << "回退定义不带法术（法术由脚本注册）";

    delete skill;
}

// ====================================================================
// Phase A 查询接口契约：query_magic_hostile / query_magic_target
//   复元术由基本内功注册为自身调理（非敌意、CAST_TO_ME），继承后属性一致；
//   未注册法术保守按敌意返回 1、target 返回 CAST_TO_RANDOM。
// ====================================================================
TEST_F(ForceAdvRecoverTest, MagicQueryContract)
{
    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
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
    ini_load("tests/unit/test_config.ini");
    g_ini_data["server"]["force_no_need_prepare"]
        = force_no_need_prepare ? "1" : "0";
    CServerConfig::init();
}

// 法术限定必须配合“基本内功”，但角色未把内功装备到 force 槽
CLuaSkill* MakeRestrictedSkill()
{
    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
    skill->create();
    skill->set("valid_force", "force");
    return skill;
}

}  // namespace

TEST_F(ForceAdvRecoverTest, PrepareGate_BlocksWhenSwitchOff)
{
    ApplyPrepareSwitch(false);

    CLuaSkill* skill = MakeRestrictedSkill();
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

    CLuaSkill* skill  = MakeRestrictedSkill();
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
// 施法内功前置闸门对「Lua 注册法术」同样生效 + hooks.char_rule("cast_force")
// 通用 hook 放行路径（前世内功）。
// ====================================================================
namespace {

// 带真实 Lua 法术（probe）的样本技能，valid_force = "force"（基本内功）
CLuaSkill* MakeProbeSkill()
{
    CLuaSkill* skill = new CLuaSkill("cast_gate_sample");
    skill->create();
    return skill;
}

}  // namespace

TEST_F(ForceAdvRecoverTest, LuaMagic_GateBlocksWithoutForce)
{
    CLuaSkill* skill = MakeProbeSkill();
    CChar* me        = MakeWoundedChar();  // 已学会 force，但未映射到 force 槽

    EXPECT_EQ(skill->cast("probe", me, NULL), 0)
        << "Lua 法术也应被施法内功前置闸门拦住（未映射限定内功）";

    delete me;
    delete skill;
}

TEST_F(ForceAdvRecoverTest, LuaMagic_GatePassesWhenMapped)
{
    CLuaSkill* skill = MakeProbeSkill();
    CChar* me        = MakeWoundedChar();
    me->map_skill("force", "force");

    EXPECT_EQ(skill->cast("probe", me, NULL), 1)
        << "映射限定内功后 Lua 法术可施放";

    delete me;
    delete skill;
}

TEST_F(ForceAdvRecoverTest, LuaMagic_GatePassesViaCharRuleHook)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(
        luaL_dostring(L,
                      "__orig_char_rule = hooks.char_rule;"
                      "hooks.char_rule = function(phase, actor, target) "
                      "if phase == \"cast_force\" then return {yes = true} end "
                      "return nil end"),
        LUA_OK);

    CLuaSkill* skill = MakeProbeSkill();
    CChar* me        = MakeWoundedChar();  // 未映射限定内功

    EXPECT_EQ(skill->cast("probe", me, NULL), 1)
        << "hooks.char_rule(\"cast_force\") "
           "放行时，未映射也可施法（前世内功路径）";

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.char_rule = __orig_char_rule; __orig_char_rule = nil"),
        LUA_OK);

    delete me;
    delete skill;
}

// ====================================================================
// hit_ob：内功反震结算 —— 玩家扣蓝、档位夹取、负伤反震与伤害反加
//   实现只有一份：lua-default/skill/basic/force.lua 的 hit_ob
// ====================================================================
namespace {

// 基本内功技能：Lua 脚本实现 hit_ob
CLuaSkill* MakeBasicForce()
{
    CLuaSkill* skill = new CLuaSkill("force");
    skill->create();
    return skill;
}

}  // namespace

TEST_F(ForceAdvRecoverTest, HitOb_UserAttacker_DeductsMpAndAccruesBonus)
{
    auto* me = new StateRecordingUser();  // userp → 先扣 mp_factor
    me->set_name("内家", "fo_me1");
    me->set("mp", (long)900);
    me->set("mp_factor", (long)30);
    me->set_skill("force", 1);  // random(1) 恒 0 → 必入 bonus 累加
    CChar* vic = new CChar();
    vic->set_name("靶子", "fo_v1");
    vic->set("mp", (long)0);
    vic->set_skill("force", 0);

    CLuaSkill* skill = MakeBasicForce();
    int32_t bonus    = 0;
    EXPECT_STREQ("", skill->hit_ob(me, vic, bonus));
    EXPECT_EQ(870, me->query("mp")) << "出手先扣 mp_factor";
    EXPECT_EQ(59, bonus) << "870/30+30-0=59 累加进 damage_bonus";

    // 负 bonus 输入：bonus+damage<0 → 归零
    int32_t bonus2 = -100;
    EXPECT_STREQ("", skill->hit_ob(me, vic, bonus2));
    EXPECT_EQ(0, bonus2) << "负和归零";

    delete skill;
    me->move(nullptr);
    delete me;
    delete vic;
}

TEST_F(ForceAdvRecoverTest, HitOb_NpcClampAndNegativeNoReversal)
{
    // 非玩家出手不扣蓝；受害者内力被 3×mp 夹取后伤害为负、
    // 且反震条件确定不成立 → 负值只落进 damage_bonus 再归零
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

    CLuaSkill* skill = MakeBasicForce();
    int32_t bonus    = 0;
    EXPECT_STREQ("", skill->hit_ob(me, vic, bonus));
    EXPECT_EQ(0, bonus) << "负伤害进 bonus 后归零";
    EXPECT_EQ(300, me->query("mp")) << "非玩家不扣蓝";
    EXPECT_EQ(100, me->query("hp")) << "未反震不掉血";

    delete skill;
    delete me;
    delete vic;
}

TEST_F(ForceAdvRecoverTest, HitOb_ReversalChatTiers)
{
    // mp/30 + factor < 0 触发反震：|damage| 落四档播报
    struct {
        long factor;
        const char* want;
    } cases[] = {
        {-11, "闷哼"}, {-20, "退了两步"}, {-30, "五六步"}, {-100, "眼前一黑"}};
    CChar* vic = new CChar();
    vic->set_name("厚朴", "fo_v3");
    vic->set("mp", (long)0);
    vic->set_skill("force", 30);  // random(31)>0 → 单次命中 30/31

    CLuaSkill* skill = MakeBasicForce();
    for (auto& c : cases) {
        CChar* me = new CChar();
        me->set_name("铁人", "fo_me3");
        me->set("hp", (long)100);
        me->set("mp", (long)300);
        me->set("mp_factor", c.factor);
        me->set_skill("force", 0);
        int32_t bonus = 0;
        char* r       = nullptr;
        for (int i = 0; i < 30 && !(r && r[0]); i++) {  // 随机反震：有限次收敛
            r = skill->hit_ob(me, vic, bonus);
        }
        ASSERT_NE(nullptr, r) << "factor=" << c.factor << " 应触发反震";
        ASSERT_TRUE(r[0]) << "factor=" << c.factor << " 应播报反震文案";
        EXPECT_NE(nullptr, strstr(r, c.want)) << "factor=" << c.factor;
        EXPECT_LT(me->query("hp"), 100) << "反震掉血";
        delete me;
    }
    delete skill;
    delete vic;
}

// ====================================================================
// 高级内功继承基本内功的反震
//   高级内功脚本没有自己的 hit_ob → 回落到基础技能（基本内功）的实现，
//   与法术继承同构：高级技能顶替基础技能时，基础技能的能力仍然在。
// ====================================================================
TEST_F(ForceAdvRecoverTest, HitOb_AdvancedForceInheritsBasicReversal)
{
    CLuaSkill* adv = new CLuaSkill("force_adv_sample");
    adv->create();
    EXPECT_FALSE(adv->has_hit_ob()) << "高级内功脚本没有自己的 hit_ob";

    auto* me = new StateRecordingUser();  // 玩家出手：先扣 mp_factor
    me->set_name("铁人", "fo_me4");
    me->set("hp", (long)100);
    me->set("mp", (long)900);
    me->set("mp_factor", (long)30);
    me->set_skill("force", 1);
    CChar* vic = new CChar();
    vic->set_name("靶子", "fo_v4");
    vic->set("mp", (long)0);
    vic->set_skill("force", 0);

    int32_t bonus = 0;
    EXPECT_STREQ("", adv->hit_ob(me, vic, bonus));
    EXPECT_EQ(59, bonus) << "高级内功应继承基本内功的反震伤害加成";
    EXPECT_EQ(870, me->query("mp")) << "出手先扣 mp_factor";

    delete adv;
    me->move(nullptr);
    delete me;
    delete vic;
}

// ====================================================================
// 复元术：内力不足时按现有内力缩比治疗
// ====================================================================
TEST_F(ForceAdvRecoverTest, Recover_LowMp_ScalesHealDown)
{
    CChar* me = MakeWoundedChar();
    // query_skill 无 raw 参数返回 raw/2：置 200 → 可见值 100 → n=50
    me->set_skill("force", 200);
    me->set("mp", (long)25);  // 25 < 50 → 走缩比支

    CLuaSkill* skill = new CLuaSkill("hitmsg_sample");
    skill->create();

    EXPECT_EQ(1, skill->cast("recover", me, NULL));
    EXPECT_EQ(0, me->query("mp")) << "n 收敛为现有 mp → 扣光";
    EXPECT_EQ(75, me->query("hp")) << "q=50*25/50=25 治疗量";

    delete skill;
    delete me;
}
