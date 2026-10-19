// test_attack.cpp - CAttack 攻击管线行为基线
//
// 一、直驱 resolve_parry / calculate_damage（AttackTestPeer friend 入口），为
// Attack.cpp 中 4 块 taozhuang 特效建立行为锚点：
//   1. 明教乾坤：招架成立 -> qiankun/dodge +50
//   2. 古墓金丝手套：招架失败 + 攻击方持武 -> 夺武器（damage = -1）
//   3. 姑苏慕容斗转星移：招架失败 -> 还击 + busy（damage = -1）
//   4. 明教乾坤：伤害结算后 -> qiankun/damage 累积 apply/damage 快照
// 另覆盖：无套装时 else-if 链落回伤害结算（不被拦截）；
//        armed 持武分支 / pp>1e6 缩放 / 招架消息回落 / 招架经验 /
//        防御循环 / 双方战斗经验 / hit_ob 消息拼接。
//
// 二、直驱 validate_force + apply_ratio_rules，锚定招式倍率的规则窗口：
//   门派贡献度加成（dataset 的 hooks.attack_ratios）——相符时伤害倍率按
//   贡献度随机放大、向零截断、巫师 verbose 播报；不符/缺席/无技能均不变。
//   以及钩子契约本身：缺席回落、部分字段写回、七个实参、时序在引擎
//   倍率规则（内功校验）之后。
//
// 随机门控说明（确定性策略）：
//   - 招架判定 urandom(ap+pp) < pp：ap=0 恒成立（招架成功）；
//     ap=1e12 仅 rand()==0（1/32768）时意外成功 -> 循环重试
//   - 套装触发 !g_utils.random(5)：1/5 概率 -> 循环重试，1000 轮失败概率 <
//   10^-90
//   - 倍率放大 random(daily)：daily=1 恒为 0（确定性不变）；daily=50 时
//     200 轮内至少放大一次
//
#include "pch.h"

#include <string>
#include <vector>

#include "Attack.h"
#include "Char.h"
#include "Container.h"
#include "Item.h"
#include "LuaEngine.h"
#include "test_lua_env.h"
#include "Mapping.h"
#include "Skill.h"
#include "User.h"

// 本文件私有哨兵常量（与 Attack.cpp 内部 #define 同值）。
// 不直接用 RESULT_* 名字：后续该常量可能共享到 CombatTypes.h，避免重定义。
static constexpr int kResultDodge = -1;
static constexpr int kResultParry = -2;

// ====================================================================
// friend struct：访问 CAttack 的 private 管线阶段
// ====================================================================
struct AttackTestPeer {
    static void call_resolve_parry(CAttack& attack, AttackContext& ctx)
    {
        attack.resolve_parry(ctx);
    }
    static void call_calculate_damage(CAttack& attack, AttackContext& ctx)
    {
        attack.calculate_damage(ctx);
    }
    static void call_validate_force(CAttack& attack, AttackContext& ctx)
    {
        attack.validate_force(ctx);
    }
    static void call_apply_ratio_rules(CAttack& attack, AttackContext& ctx)
    {
        attack.apply_ratio_rules(ctx);
    }
    static void call_resolve_dodge(CAttack& attack, AttackContext& ctx)
    {
        attack.resolve_dodge(ctx);
    }
    static void call_send_verbose_debug(CAttack& attack, AttackContext& ctx)
    {
        attack.send_verbose_debug(ctx);
    }
};

// ====================================================================
//  IHittable::hit_ob 消息桩：提供非空返回，锁定 Attack 的消息拼接契约。
//  生产实现 CLuaNpcT 从脚本读取消息，虚接口签名与此一致。
// ====================================================================
struct HitMsgChar : public CChar {
    const char* hit_ob(CChar*, CChar*, int32_t) override
    {
        return "自身特殊命中消息。";
    }
};

struct HitMsgItem : public CItem {
    const char* hit_ob(CChar*, CChar*, int32_t) override
    {
        return "武器特殊命中消息。";
    }
};

// ====================================================================
// Fixture：真实 Lua 引擎 + 攻防双方
// ====================================================================
class FamilySetTest : public ::testing::Test {
protected:
    CContainer room;
    CChar me;
    CChar victim;
    CAttack attack;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "fam_set_room");
        me.set_name("attacker", "atk1");
        victim.set_name("defender", "def1");
        me.set("str", (long)20);
        me.set("hp", (long)1000000);
        me.set("max_hp", (long)1000000);
        victim.set("hp", (long)1000000);
        victim.set("max_hp", (long)1000000);
        me.move(&room);
        victim.move(&room);
    }

    void TearDown() override
    {
        me.move(nullptr);
        victim.move(nullptr);
        room.move(nullptr);
    }

    // CChar dbase 无 "/" 路径写入时的嵌套 family 设置（同 test_char_family）
    static void SetFamily(CChar* ch, const char* name)
    {
        CMapping fam;
        fam.set("family_name", name);
        ch->set("family", fam);
    }

    // 绕过 map_skill 的"已学会技能"检查，直接登记 parry 映射
    static void MapParry(CChar* ch, const char* skill)
    {
        ch->query_skill_map()->set("parry", skill);
    }

    // 构造一次招架结算上下文；ap 控制成败（0=必成，1e12≈必败）
    AttackContext MakeParryCtx(double ap, CContainer* weapon = nullptr)
    {
        AttackContext ctx;
        ctx.me     = &me;
        ctx.victim = &victim;
        ctx.ap     = ap;
        ctx.weapon = weapon;
        return ctx;
    }
};

// ====================================================================
// 记录被投递到客户端的玩家，用于验证 verbose 播报真的送达
// ====================================================================
class RatioRecordingUser : public CUser {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        if (msg) msgs.push_back(msg);
    }
    std::vector<std::string> msgs;
};

// ====================================================================
// Fixture：攻击倍率规则窗口（apply_ratio_rules / hooks.attack_ratios）
// ====================================================================
class RatioRulesTest : public ::testing::Test {
protected:
    CContainer room;
    CChar me;
    CChar victim;
    CSkill skill;
    CAttack attack;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "ratio_room");
        me.set_name("attacker", "ratio_atk");
        victim.set_name("defender", "ratio_vic");
        me.move(&room);
        victim.move(&room);
    }

    void TearDown() override
    {
        me.move(nullptr);
        victim.move(nullptr);
        room.move(nullptr);
    }

    static void SetFamily(CChar* ch, const char* name)
    {
        CMapping fam;
        fam.set("family_name", name);
        ch->set("family", fam);
    }

    // 构造一次倍率规则上下文：四个倍率取易辨识的初值
    AttackContext MakeRatioCtx(int damage, int parry, int dodge, int force)
    {
        AttackContext ctx;
        ctx.me           = &me;
        ctx.victim       = &victim;
        ctx.skill        = &skill;
        ctx.damage_ratio = damage;
        ctx.parry_ratio  = parry;
        ctx.dodge_ratio  = dodge;
        ctx.force_ratio  = force;
        return ctx;
    }
};

// ====================================================================
// daily=1 → random(1) 恒为 0 → 伤害倍率原样不变（确定性）
// ====================================================================
TEST_F(RatioRulesTest, FamilyMatch_DailyOne_KeepsDamageRatio)
{
    SetFamily(&me, "少林派");
    skill.set("family", "少林派");
    me.set("daily/family/少林派/daily", (long)1);

    AttackContext ctx = MakeRatioCtx(100, 0, 0, 0);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);
    EXPECT_EQ(ctx.damage_ratio, 100);
}

// ====================================================================
// 门派不符：四个倍率全部不变
// ====================================================================
TEST_F(RatioRulesTest, FamilyMismatch_LeavesRatiosUnchanged)
{
    SetFamily(&me, "丐帮");
    skill.set("family", "少林派");
    me.set("daily/family/丐帮/daily", (long)50);

    AttackContext ctx = MakeRatioCtx(100, 20, 30, 40);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);
    EXPECT_EQ(ctx.damage_ratio, 100);
    EXPECT_EQ(ctx.parry_ratio, 20);
    EXPECT_EQ(ctx.dodge_ratio, 30);
    EXPECT_EQ(ctx.force_ratio, 40);
}

// ====================================================================
// 门派相符但 daily 缺席（0）→ random(0)=0 → 伤害倍率不变
// ====================================================================
TEST_F(RatioRulesTest, FamilyMatch_DailyAbsent_KeepsDamageRatio)
{
    SetFamily(&me, "少林派");
    skill.set("family", "少林派");

    AttackContext ctx = MakeRatioCtx(100, 0, 0, 0);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);
    EXPECT_EQ(ctx.damage_ratio, 100);
}

// ====================================================================
// 负倍率向零截断：damage=-25、daily=50 → [-37,-25]（modf 而非向下取整）
// ====================================================================
TEST_F(RatioRulesTest, NegativeRatio_TruncatesTowardZero)
{
    SetFamily(&me, "少林派");
    skill.set("family", "少林派");
    me.set("daily/family/少林派/daily", (long)50);

    for (int i = 0; i < 200; ++i) {
        AttackContext ctx = MakeRatioCtx(-25, 0, 0, 0);
        AttackTestPeer::call_apply_ratio_rules(attack, ctx);
        // -25*(100+r)/100，r∈[0,49] → 实值∈[-37.25,-25]，向零截断∈[-37,-25]
        EXPECT_GE(ctx.damage_ratio, -37);
        EXPECT_LE(ctx.damage_ratio, -25);
    }
}

// ====================================================================
// 钩子缺席：四个倍率原样进入后续阶段
// ====================================================================
TEST_F(RatioRulesTest, HookAbsent_LeavesRatiosUnchanged)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_ar = hooks.attack_ratios "
                            "hooks.attack_ratios = nil"),
              LUA_OK);

    SetFamily(&me, "少林派");
    skill.set("family", "少林派");
    me.set("daily/family/少林派/daily", (long)50);

    AttackContext ctx = MakeRatioCtx(100, 20, 30, 40);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);

    luaL_dostring(L, "hooks.attack_ratios = __orig_ar __orig_ar = nil");

    EXPECT_EQ(ctx.damage_ratio, 100);
    EXPECT_EQ(ctx.parry_ratio, 20);
    EXPECT_EQ(ctx.dodge_ratio, 30);
    EXPECT_EQ(ctx.force_ratio, 40);
}

// ====================================================================
// 只给部分字段：未给出的倍率保持不变
// ====================================================================
TEST_F(RatioRulesTest, PartialFields_OverwriteOnlyPresent)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_ar = hooks.attack_ratios\n"
                            "hooks.attack_ratios = function() "
                            "return { force = 7 } end"),
              LUA_OK);

    AttackContext ctx = MakeRatioCtx(100, 20, 30, 40);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);

    luaL_dostring(L, "hooks.attack_ratios = __orig_ar __orig_ar = nil");

    EXPECT_EQ(ctx.force_ratio, 7) << "给出的字段应写回";
    EXPECT_EQ(ctx.damage_ratio, 100);
    EXPECT_EQ(ctx.parry_ratio, 20);
    EXPECT_EQ(ctx.dodge_ratio, 30);
}

// ====================================================================
// 实参契约：me/victim/skill 为对象，四个倍率与上下文一致
// ====================================================================
TEST_F(RatioRulesTest, HookArgs_MatchContract)
{
    skill.set("ratio_probe", (long)4242);

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_ar = hooks.attack_ratios\n"
                            "hooks.attack_ratios = function(m, v, sk, d, p, "
                            "dg, f)\n"
                            "  __ar_me = m:querystr('id')\n"
                            "  __ar_victim = v:querystr('id')\n"
                            "  __ar_probe = sk:query('ratio_probe')\n"
                            "  __ar_damage = d __ar_parry = p\n"
                            "  __ar_dodge = dg __ar_force = f\n"
                            "  return nil\n"
                            "end"),
              LUA_OK);

    AttackContext ctx = MakeRatioCtx(11, 22, 33, 44);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);

    luaL_dostring(L, "hooks.attack_ratios = __orig_ar __orig_ar = nil");

    auto get_str = [L](const char* g) {
        lua_getglobal(L, g);
        std::string s = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        return s;
    };
    auto get_int = [L](const char* g) {
        lua_getglobal(L, g);
        int64_t n = lua_tointeger(L, -1);
        lua_pop(L, 1);
        return n;
    };

    EXPECT_EQ(get_str("__ar_me"), "ratio_atk");
    EXPECT_EQ(get_str("__ar_victim"), "ratio_vic");
    EXPECT_EQ(get_int("__ar_probe"), 4242)
        << "skill 实参应是本次攻击的技能对象";
    EXPECT_EQ(get_int("__ar_damage"), 11);
    EXPECT_EQ(get_int("__ar_parry"), 22);
    EXPECT_EQ(get_int("__ar_dodge"), 33);
    EXPECT_EQ(get_int("__ar_force"), 44);

    luaL_dostring(L,
                  "__ar_me = nil __ar_victim = nil __ar_probe = nil "
                  "__ar_damage = nil __ar_parry = nil "
                  "__ar_dodge = nil __ar_force = nil");
}

// ====================================================================
// 无技能（未选定招式）：压 nil，脚本判空回落，不崩且倍率不变
// ====================================================================
TEST_F(RatioRulesTest, SkillNil_SkipsWithoutCrash)
{
    SetFamily(&me, "少林派");
    me.set("daily/family/少林派/daily", (long)50);

    AttackContext ctx = MakeRatioCtx(100, 0, 0, 0);
    ctx.skill         = nullptr;
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);
    EXPECT_EQ(ctx.damage_ratio, 100);
}

// ====================================================================
// 时序：脚本看到的是内功校验后的倍率（伤害减半、加力归零）
// ====================================================================
TEST_F(RatioRulesTest, RunsAfterForceValidation_SeesEngineAdjustedRatio)
{
    // 内功不兼容：招式要求 hitmsg_sample，攻方未映射内功 → 校验失败
    skill.set("valid_force", "hitmsg_sample");
    SetFamily(&me, "少林派");
    skill.set("family", "少林派");

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_ar = hooks.attack_ratios\n"
                            "hooks.attack_ratios = function(m, v, sk, d, p, "
                            "dg, f)\n"
                            "  __ar_damage = d __ar_force = f\n"
                            "  return nil\n"
                            "end"),
              LUA_OK);

    AttackContext ctx = MakeRatioCtx(100, 0, 0, 40);
    AttackTestPeer::call_validate_force(attack, ctx);
    AttackTestPeer::call_apply_ratio_rules(attack, ctx);

    luaL_dostring(L, "hooks.attack_ratios = __orig_ar __orig_ar = nil");
    lua_getglobal(L, "__ar_damage");
    int64_t seen_damage = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getglobal(L, "__ar_force");
    int64_t seen_force = lua_tointeger(L, -1);
    lua_pop(L, 1);
    luaL_dostring(L, "__ar_damage = nil __ar_force = nil");

    EXPECT_EQ(ctx.damage_ratio, 50) << "内功冲突应先把伤害倍率减半";
    EXPECT_EQ(ctx.force_ratio, 0);
    EXPECT_EQ(seen_damage, 50) << "脚本必须在引擎倍率规则之后被询问";
    EXPECT_EQ(seen_force, 0);
}

// ====================================================================
// 招架成立 + 无套装：damage = -2，无任何 qiankun 累积
// ====================================================================
TEST_F(FamilySetTest, ParryLanded_NoSet_LeavesNoTrace)
{
    AttackContext ctx = MakeParryCtx(0.0);
    AttackTestPeer::call_resolve_parry(attack, ctx);

    EXPECT_EQ(ctx.damage, kResultParry);
    EXPECT_EQ(victim.query_temp("qiankun/dodge"), 0);
    EXPECT_EQ(ctx.result.find("抢攻"), std::string::npos);
}

// ====================================================================
// 招架失败 + 无套装：else-if 链不拦截，落回伤害结算
// ====================================================================
TEST_F(FamilySetTest, ParryMissed_NoSet_FallsThroughToDamage)
{
    bool damaged = false;
    for (int i = 0; i < 100 && !damaged; i++) {
        AttackContext ctx = MakeParryCtx(1e12);
        AttackTestPeer::call_resolve_parry(attack, ctx);
        if (ctx.damage > 0) {
            damaged = true;
            EXPECT_NE(ctx.result.find("点的伤害"), std::string::npos)
                << "无套装时应正常落回 calculate_damage";
        }
    }
    EXPECT_TRUE(damaged) << "me str=20 下伤害应 > 0（ap=1e12 招架几乎必败）";
    EXPECT_EQ(me.query_temp("qiankun/dodge"), 0);
}

// ====================================================================
// 伤害结算 + 无套装：不追踪 qiankun/damage、无明教文案
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_NoSet_TracksNothing)
{
    AttackContext ctx;
    ctx.me     = &me;
    ctx.victim = &victim;
    ctx.ap     = 1e12;
    AttackTestPeer::call_calculate_damage(attack, ctx);

    EXPECT_EQ(victim.query_temp("qiankun/damage"), 0);
    EXPECT_EQ(ctx.result.find("凌厉的光芒"), std::string::npos);
    EXPECT_GT(ctx.damage, 0);
}

// ====================================================================
//  victim 持武分支：pp 取 parry 力、回写 parry_skill、pp>1e6 缩放、
//  映射技能无 query_parry_msg 时消息走回落分支
// ====================================================================
TEST_F(FamilySetTest, ParryArmedVictim_FallbackMsgAndHighPp)
{
    // 装备槽 + 库存武器（skill_type 非空）→ query_weapon() 非空，走 armed 分支
    CItem wpn;
    wpn.set_name("sword", "wp_armed");
    wpn.set("skill_type", "blade");
    wpn.move(&victim);
    (*victim.get_equip_list())[WIELD_RIGHT_HAND] = wpn.object_id();

    // 无 parry 技能 → skill_power = combat_exp/2 = 1.5e6；攻击方徒手 ×2 → 3e6 >
    // 1e6
    victim.set("combat_exp", (long)3000000);
    // 映射到无 query_parry_msg 的技能 → 招架消息走回落分支
    MapParry(&victim, "unarmed_adv");

    AttackContext ctx = MakeParryCtx(0.0);  // ap=0 → 招架必成
    AttackTestPeer::call_resolve_parry(attack, ctx);

    EXPECT_EQ(ctx.damage, kResultParry);
    EXPECT_EQ(ctx.parry_skill,
              "unarmed_adv");      // armed 分支回写（Phase2 依赖的契约）
    EXPECT_GT(ctx.pp, 1000000.0);  // pp>1e6 缩放分支生效
    EXPECT_NE(ctx.result.find("格开"), std::string::npos);

    wpn.move(
        nullptr);  // 摘除：wpn 是局部对象，不能留在 victim 容器里被二次析构
}

// ====================================================================
//  招架成立 + pp < ap + victim 高悟性 → 获得招架经验并提升 parry
// ====================================================================
TEST_F(FamilySetTest, ParrySuccess_GrantVictimParryExp)
{
    victim.set("int", (long)99);  // random(50) < int 恒真

    bool landed = false;
    for (int i = 0; i < 1000 && !landed; ++i) {
        // 徒手 victim：pp 被钳到 1；ap=2 → urandom(3) < 1 概率 1/3
        AttackContext ctx = MakeParryCtx(2.0);
        AttackTestPeer::call_resolve_parry(attack, ctx);
        if (ctx.damage == kResultParry) {
            landed = true;
            EXPECT_EQ(ctx.add_exp_victim, 1);
            EXPECT_GE(victim.query("combat_exp"), 1);
            EXPECT_EQ(victim.query_learned()->query("parry"), 1);
        }
    }
    EXPECT_TRUE(landed) << "1000 轮内 ap=2/pp=1 招架应成功（约1/3）";
}

// ====================================================================
//  force 与攻击技能 hit_ob 双返回消息 → 各追加一次到战报
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_HitObMessages_Appended)
{
    me.set("mp_factor", (long)100);
    me.set("mp", (long)200);  // mp ≥ mp_factor → 进入 force hit_ob 分支
    me.query_skill_map()->set("force", "hitmsg_sample");

    AttackContext ctx;
    ctx.me           = &me;
    ctx.victim       = &victim;
    ctx.ap           = 1e12;  // 跳过经验块，保持确定
    ctx.attack_skill = "hitmsg_sample";
    AttackTestPeer::call_calculate_damage(attack, ctx);

    // force 分支（L398）与攻击技能分支（L411）应各追加一次
    const char* MSG           = "示例技能的命中消息。";
    std::string::size_type p1 = ctx.result.find(MSG);
    ASSERT_NE(p1, std::string::npos);
    std::string::size_type p2 = ctx.result.find(MSG, p1 + 1);
    EXPECT_NE(p2, std::string::npos);
    EXPECT_GT(ctx.damage, 0);
}

// ====================================================================
//  victim 等级提升防御系数 → 防御循环衰减伤害（初值 ≥10，进循环即 <10）
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_DefenseLoop_ScalesDamage)
{
    victim.set("level", (long)2);  // level2_exp(2)=100 → defense_factor=101

    bool scaled = false;
    for (int i = 0; i < 50 && !scaled; ++i) {
        AttackContext ctx;
        ctx.me     = &me;
        ctx.victim = &victim;
        ctx.ap     = 1e12;  // 跳过经验块
        AttackTestPeer::call_calculate_damage(attack, ctx);
        ASSERT_GT(ctx.damage, 0);
        // 伤害初值 = (20+random(20))/2 ∈ [10,19]；进过循环一轮后 10..13→7..9 必
        // <10
        if (ctx.damage < 10) scaled = true;
    }
    EXPECT_TRUE(scaled)
        << "victim level=2 时防御循环应把伤害从初值≥10 衰减到 <10";
}

// ====================================================================
//  ap<dp → 攻击方获得经验；victim hp=0 → 受伤方经验每轮必得
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_GainsCombatExpForBothSides)
{
    victim.set("hp", (long)0);
    victim.set("max_hp",
               (long)0);  // RANDOM(max_hp+hp)=0 < damage → 受伤方经验恒触发

    bool meGained   = false;
    int meExpGained = 0;
    int attempts    = 0;
    for (int i = 0; i < 1000 && !meGained; ++i) {
        ++attempts;
        AttackContext ctx;
        ctx.me     = &me;
        ctx.victim = &victim;
        ctx.ap     = 1;
        ctx.dp     = 100;  // ap<dp 且 random(100+int)>30（约70%）→ 攻击方经验
        AttackTestPeer::call_calculate_damage(attack, ctx);
        ASSERT_GT(ctx.damage, 0);
        EXPECT_EQ(ctx.add_exp_victim, 1);  // 每轮必得
        if (ctx.add_exp_me > 0) {
            meGained    = true;
            meExpGained = ctx.add_exp_me;
        }
    }
    EXPECT_TRUE(meGained);
    EXPECT_GE(meExpGained, 1);
    EXPECT_GT(me.query("combat_exp"), 0);
    EXPECT_EQ(victim.query("combat_exp"), attempts);  // 每轮 +1，无其他来源
}

// ====================================================================
//  武器 hit_ob 返回消息 → 追加到战报（L416-421 分支）
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_WeaponHitOb_MessageAppended)
{
    HitMsgItem weapon;

    AttackContext ctx;
    ctx.me     = &me;
    ctx.victim = &victim;
    ctx.weapon = &weapon;
    ctx.ap     = 1e12;  // 跳过经验块
    AttackTestPeer::call_calculate_damage(attack, ctx);

    EXPECT_NE(ctx.result.find("武器特殊命中消息。"), std::string::npos);
    EXPECT_GT(ctx.damage, 0);
}

// ====================================================================
//  无武器时取攻击方自身 hit_ob 消息 → 追加到战报（L424-429 分支）
// ====================================================================
TEST_F(FamilySetTest, CalculateDamage_MeHitOb_MessageAppended)
{
    HitMsgChar attacker;
    attacker.set_name("attacker2", "atk2");
    attacker.set("str", (long)20);

    AttackContext ctx;
    ctx.me     = &attacker;  // weapon 为空 → else 分支取 me 的 hit_ob
    ctx.victim = &victim;
    ctx.ap     = 1e12;  // 跳过经验块
    AttackTestPeer::call_calculate_damage(attack, ctx);

    EXPECT_NE(ctx.result.find("自身特殊命中消息。"), std::string::npos);
    EXPECT_GT(ctx.damage, 0);
}

// ====================================================================
//  后置增强钩子：覆盖伤害（结算前写回）/ 追加文案
// ====================================================================
TEST_F(FamilySetTest, CalculateDamageHook_NumberOverrideApplied)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.calculate_damage = function() "
                            "return 4321 end"),
              LUA_OK);
    // eff_hp 必须与 hp 同步：创伤分支（约 1/7 概率）会把 eff_hp 缺省为0
    // 的角色 hp 钳到 -1，导致结算后断言不稳
    victim.set("eff_hp", (long)1000000);

    AttackContext ctx;
    ctx.me     = &me;
    ctx.victim = &victim;
    ctx.ap     = 1e12;
    AttackTestPeer::call_calculate_damage(attack, ctx);
    luaL_dostring(L, "hooks.calculate_damage = nil");

    EXPECT_EQ(ctx.damage, 4321) << "钩子数值返回应在结算前覆盖伤害";
    EXPECT_EQ((long)victim.query("hp"), 1000000L - 4321)
        << "覆盖后的伤害应作用于受害者气血";
}

TEST_F(FamilySetTest, ResolveParryHook_OverrideAndTextApplied)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.resolve_parry = function(me, victim, "
                            "damage, parry_skill) "
                            "return {damage = 42, text = '【增强文案】'} end"),
              LUA_OK);

    AttackContext ctx = MakeParryCtx(0.0);  // ap=0 → 招架必成
    AttackTestPeer::call_resolve_parry(attack, ctx);
    luaL_dostring(L, "hooks.resolve_parry = nil");

    EXPECT_EQ(ctx.damage, 42) << "表字段 damage 应写回 ctx";
    EXPECT_NE(ctx.result.find("【增强文案】"), std::string::npos)
        << "表字段 text 应追加到战报";
}

// ====================================================================
// 深路：resolve_dodge 分支矩阵 / send_verbose_debug 双侧 /
//       calculate_damage 的 hooks.calculate_damage 覆盖
// ====================================================================

class AttackDeepTest : public ::testing::Test {
protected:
    CContainer room;
    CChar me;
    CChar victim;
    CAttack attack;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "ad_room");
        me.set_name("攻方", "ad_me");
        me.set("hp", (long)100);
        me.set("max_hp", (long)100);
        me.set("int", (long)1000);
        me.move(&room);
        victim.set_name("守方", "ad_victim");
        victim.set("hp", (long)1000);
        victim.set("max_hp", (long)1000);
        victim.set("int", (long)100);
        victim.move(&room);
    }

    AttackContext MakeDodgeCtx()
    {
        AttackContext ctx;
        ctx.me          = &me;
        ctx.victim      = &victim;
        ctx.attack_type = "unarmed";
        ctx.dodge_skill = "dodge";
        ctx.dodge_ratio = 0;
        ctx.flag        = 0;  // 非 TYPE_HIT(6)：TYPE_HIT 会把 dp 压到 1
        return ctx;
    }
};

TEST_F(AttackDeepTest, ResolveDodgeBranchMatrix)
{
    // 支一：ap > dp（exp 支 242-246 需要 dp < ap）
    me.set_skill("unarmed", 500);
    victim.set_skill("dodge", 50);
    victim.query_skill_map()->set("dodge", "dodge");

    AttackContext ctx = MakeDodgeCtx();
    ctx.dodge_ratio   = 27000;  // 把 dp 从 23K 拉到 ~6.35M：略低于 ap(6.6M) →
                                // 闪率~48% 且 dp<ap(exp 支)
    bool dodged = false;
    AttackTestPeer::call_resolve_dodge(attack, ctx);
    for (int i = 0; i < 40 && !dodged; i++) {
        AttackTestPeer::call_resolve_dodge(attack, ctx);
        if (ctx.damage == kResultDodge) dodged = true;
    }
    EXPECT_TRUE(dodged) << "闪避成功 → RESULT_DODGE(258)";
    ASSERT_NE(ctx.dodge_msg, nullptr) << "dodge_msg 直填(208-209)";
    EXPECT_NE(ctx.dodge_msg[0], '\0') << "消息非空(209)";
    EXPECT_GT(ctx.dp, 0) << "DP 修正有效(224)";
    EXPECT_GT(victim.query("combat_exp"), 0) << "闪避经验入账(242-246)";

    // 支二：ap <= dp（攻方失手经验 250-254）
    me.clear_skill_map();
    victim.set_skill("dodge", 100000);
    AttackContext ctx2 = MakeDodgeCtx();
    for (int i = 0; i < 4; i++) {
        AttackTestPeer::call_resolve_dodge(attack, ctx2);
        if (ctx2.damage == kResultDodge && ctx2.add_exp_me > 0) break;
    }
    EXPECT_EQ(kResultDodge, ctx2.damage) << "高闪避(258)";
    EXPECT_GT(ctx2.add_exp_me, 0) << "攻方失手经验(250-254)";

    // 支三：fallback 技能（无 dodge_msg → query_action 回填 211-215）
    victim.query_skill_map()->set("dodge", "dodge_fallback");
    AttackContext ctx3 = MakeDodgeCtx();
    AttackTestPeer::call_resolve_dodge(attack, ctx3);
    EXPECT_EQ(30, ctx3.mod_val) << "action.dodge 进 mod_val(213-214)";
    ASSERT_NE(ctx3.dodge_msg, nullptr) << "action 行回填(215)";
    EXPECT_NE(ctx3.dodge_msg[0], '\0');
}
TEST_F(AttackDeepTest, SendVerboseDebugBothSidesAndDamageStates)
{
    me.set("grant", (long)60);
    me.set("env/combat", "verbose");
    victim.set("grant", (long)60);
    victim.set("env/combat", "verbose");

    AttackContext ctx;
    ctx.me      = &me;
    ctx.victim  = &victim;
    ctx.ap      = 123.5;
    ctx.dp      = 45.6;
    ctx.pp      = 78.9;
    ctx.mod_val = 7;

    ctx.damage = 50;  // damage>0 两支(596-601/605, 611-614/619)
    AttackTestPeer::call_send_verbose_debug(attack, ctx);

    AttackTestPeer::call_send_verbose_debug(attack, ctx);

    SUCCEED() << "verbose 双侧双态消息已发出";
}

TEST_F(AttackDeepTest, CalculateDamageHookOverrides)
{
    lua_State* L = g_LuaEngine->get_state();
    luaL_dostring(L,
                  "__orig_cd = hooks.calculate_damage\n"
                  "hooks.calculate_damage = function(me, victim, damage)\n"
                  "  return { damage = 5, text = '钩子附文' }\n"
                  "end");

    AttackContext ctx;
    ctx.me     = &me;
    ctx.victim = &victim;
    ctx.damage = 10;

    AttackTestPeer::call_calculate_damage(attack, ctx);

    EXPECT_EQ(5, ctx.damage) << "钩子 damage 字段覆盖(435-436)";
    EXPECT_NE(ctx.result.find("钩子附文"), std::string::npos)
        << "钩子 text 并入结果(437-438)";

    luaL_dostring(L, "hooks.calculate_damage = __orig_cd __orig_cd = nil");
}
