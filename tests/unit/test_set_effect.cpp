// test_set_effect.cpp - CSetEffect 阶段分派单元测试
//
// 覆盖 dispatch 的阶段过滤、真实套装规则的匹配矩阵（套装键/门派/招架
// 映射）与脚本钩子契约（接管/写回/缺席回落/参数传递）。管线级特效行为
// 由 test_attack.cpp 按行为基线覆盖，此处只测分派框架本身。
//
// 自定义钩子测试在断言前恢复 boot 加载的参考实现，测试顺序无关。
//
#include "pch.h"

#include "Attack.h"
#include "Char.h"
#include "Container.h"
#include "Item.h"
#include "LuaEngine.h"
#include "test_lua_env.h"
#include "Mapping.h"
#include "SetEffect.h"

class SetEffectTest : public ::testing::Test {
protected:
    CContainer room;
    CChar me;
    CChar victim;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "set_eff_room");
        me.set_name("attacker", "atk_se");
        victim.set_name("defender", "def_se");
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

    static void MapParry(CChar* ch, const char* skill)
    {
        ch->query_skill_map()->set("parry", skill);
    }

    AttackContext MakeCtx()
    {
        AttackContext ctx;
        ctx.me     = &me;
        ctx.victim = &victim;
        return ctx;
    }
};

// ====================================================================
// matches 谓词矩阵：套装键 / 门派 / 招架映射 任一不符即不触发
// ====================================================================
TEST_F(SetEffectTest, Matches_MissingSetKey_DoesNotFire)
{
    SetFamily(&victim, "明教");
    MapParry(&victim, "qiankun_danuoyi");  // 无 taozhuang/m

    AttackContext ctx = MakeCtx();
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryLanded));
    EXPECT_EQ(victim.query_temp("qiankun/dodge"), 0);
}

TEST_F(SetEffectTest, Matches_FamilyMismatch_DoesNotFire)
{
    victim.set_temp("taozhuang/m", 1);
    SetFamily(&victim, "古墓派");
    MapParry(&victim, "qiankun_danuoyi");

    AttackContext ctx = MakeCtx();
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryLanded));
    EXPECT_EQ(victim.query_temp("qiankun/dodge"), 0);
}

TEST_F(SetEffectTest, Matches_ParrySkillMismatch_DoesNotFire)
{
    SetFamily(&victim, "明教");
    victim.set_temp("taozhuang/m", 1);
    MapParry(&victim, "douzhuan_xingyi");  // 明教规则要求 qiankun_danuoyi

    AttackContext ctx = MakeCtx();
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryLanded));
    EXPECT_EQ(victim.query_temp("qiankun/dodge"), 0);
}

// 武器门控：套装键/门派匹配但攻击方空手时，古墓夺武器规则不触发
TEST_F(SetEffectTest, Matches_RequireWeapon_UnarmedDoesNotFire)
{
    SetFamily(&victim, "古墓派");
    victim.set_temp("taozhuang/u_4", 1);

    AttackContext ctx = MakeCtx();
    ctx.damage        = 5;

    // 门控若失效，每次误触概率 1/5；200 轮使漏检概率低于 1e-19
    bool fired = false;
    for (int i = 0; i < 200 && !fired; i++) {
        fired = CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed);
    }
    EXPECT_FALSE(fired) << "空手时夺武器一次都不应触发";
    EXPECT_EQ(ctx.damage, 5);
    EXPECT_EQ(ctx.result.find("抓住"), std::string::npos);
}

// ====================================================================
// 脚本钩子契约：接管 / 写回 / 缺席回落 / 参数传递
// ====================================================================
class SetEffectHookTest : public SetEffectTest {
protected:
    void InstallHook(const char* fn_src)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        luaL_dostring(L, "__orig_set_effect = hooks.set_effect");
        EXPECT_EQ(luaL_dostring(L, fn_src), LUA_OK);
    }

    void RestoreHook()
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        luaL_dostring(L,
                      "hooks.set_effect = __orig_set_effect "
                      "__orig_set_effect = nil _G.__se_cap = nil");
    }

    static bool ReadCapPhase(std::string& phase, bool& has_weapon)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        lua_getglobal(L, "__se_cap");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        lua_getfield(L, -1, "phase");
        const char* p = lua_tostring(L, -1);
        phase         = p ? p : "";
        lua_pop(L, 1);
        lua_getfield(L, -1, "weapon");
        has_weapon = lua_toboolean(L, -1) != 0;
        lua_pop(L, 2);
        return true;
    }

    static bool ReadCapInt(const char* key, long long& out)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        lua_getglobal(L, "__se_cap");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        lua_getfield(L, -1, key);
        bool isnum = lua_isnumber(L, -1) != 0;
        out        = (long long)lua_tointeger(L, -1);
        lua_pop(L, 2);
        return isnum;
    }
};

// 接管：handled=true → 分派返回 true，damage 写回
TEST_F(SetEffectHookTest, HookHandled_InterceptsAndWritesBack)
{
    InstallHook(
        "hooks.set_effect = function() "
        "return {handled = true, damage = 777} end");
    AttackContext ctx = MakeCtx();
    EXPECT_TRUE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed));
    RestoreHook();
    EXPECT_EQ(ctx.damage, 777);
}

// 未接管：效果写回但分派返回 false，调用方照常伤害结算
TEST_F(SetEffectHookTest, HookNotHandled_WritesAndFallsThrough)
{
    InstallHook(
        "hooks.set_effect = function() "
        "return {handled = false, damage = 888, text = 'X'} end");
    AttackContext ctx = MakeCtx();
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed));
    RestoreHook();
    EXPECT_EQ(ctx.damage, 888);
    EXPECT_NE(ctx.result.find("X"), std::string::npos);
}

// 缺席：分派静默返回 false，ctx 不变
TEST_F(SetEffectHookTest, HookAbsent_FallsThrough)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_set_effect = hooks.set_effect "
                            "hooks.set_effect = nil"),
              LUA_OK);
    AttackContext ctx = MakeCtx();
    ctx.damage        = 5;
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed));
    luaL_dostring(L,
                  "hooks.set_effect = __orig_set_effect "
                  "__orig_set_effect = nil");
    EXPECT_EQ(ctx.damage, 5);
}

// 实参序：phase 字符串与武器标志按契约传入
TEST_F(SetEffectHookTest, HookArgs_PhaseAndWeaponFlagPassed)
{
    InstallHook(
        "hooks.set_effect = function(phase, me, victim, has_weapon, ...) "
        "_G.__se_cap = {phase = phase, weapon = has_weapon} "
        "return nil end");

    AttackContext ctx = MakeCtx();
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::ParryMissed));
    std::string phase;
    bool has_weapon = true;
    ASSERT_TRUE(ReadCapPhase(phase, has_weapon));
    EXPECT_EQ(phase, "parry_missed");
    EXPECT_FALSE(has_weapon);

    CContainer weapon;
    ctx.weapon = &weapon;
    EXPECT_FALSE(CSetEffect::dispatch(ctx, SetEffectPhase::PostDamage));
    EXPECT_TRUE(ReadCapPhase(phase, has_weapon));
    EXPECT_EQ(phase, "post_damage");
    EXPECT_TRUE(has_weapon);
    RestoreHook();
}

// post_attack 通知阶段：按阶段名与完整实参序派发
TEST_F(SetEffectHookTest, PostAttack_DispatchesPhaseAndArgs)
{
    InstallHook(
        "hooks.set_effect = function(phase, me, victim, has_weapon, "
        "parry_skill, parry_msg, damage, damage_base) "
        "_G.__se_cap = {phase = phase, weapon = has_weapon, "
        "dmg = damage, base = damage_base} "
        "return nil end");

    AttackContext ctx = MakeCtx();
    ctx.damage        = 42;
    ctx.damage_base   = 17;
    CContainer weapon;
    ctx.weapon = &weapon;

    CSetEffect::notify(ctx, SetEffectPhase::PostAttack);

    std::string phase;
    bool has_weapon = false;
    ASSERT_TRUE(ReadCapPhase(phase, has_weapon));
    EXPECT_EQ(phase, "post_attack");
    EXPECT_TRUE(has_weapon);
    long long dmg  = 0;
    long long base = 0;
    EXPECT_TRUE(ReadCapInt("dmg", dmg));
    EXPECT_EQ(dmg, 42);
    EXPECT_TRUE(ReadCapInt("base", base));
    EXPECT_EQ(base, 17);
    RestoreHook();
}

// post_attack 为通知型：脚本返回值不消费，ctx 不被写回
TEST_F(SetEffectHookTest, PostAttack_ResultNotConsumed)
{
    InstallHook(
        "hooks.set_effect = function() "
        "return {handled = true, damage = 999, text = 'Y'} end");

    AttackContext ctx = MakeCtx();
    ctx.damage        = 5;
    CSetEffect::notify(ctx, SetEffectPhase::PostAttack);
    RestoreHook();
    EXPECT_EQ(ctx.damage, 5);
    EXPECT_EQ(ctx.result.find("Y"), std::string::npos);
}
