/** @file CharLevel.cpp @brief implementation of CChar (CharLevel). */

#include <windows.h>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Skill.h"
#include "CmdHandle.h"
#include "Room.h"
#include "Condition.h"
#include "Channel.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "AutoFight.h"
#include "CombatConfig.h"
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"
#include "../lua/LuaEngine.h"
#include <cmath>
#include <vector>

int CChar::level2skill(mapping_value_t lv)
{
    // 等级域封顶 NORMAL_MAX_LEVEL：入口收窄一次，对照表按 int 级数推导
    const level_t lv32 = static_cast<level_t>(lv);

    // 等级及技能限制对照表
    // 1,低于5级每级+4
    if (lv32 < 5) return (lv32 + 1) * 4;
    // 2,低于25级每级+3
    if (lv32 < 25) return 24 + (lv32 - 5) * 3;
    // 3,之后每级+3.5
    return 84 + (lv32 - 25) * 7 / 2;
}

skill_level_t CChar::skill2level(mapping_value_t sk)
{
    const skill_level_t sk32 = static_cast<skill_level_t>(sk);

    if (sk32 > 84) {
        return (sk32 - 84) * 2 / 7 + 25;
    }
    if (sk32 > 24) {
        return (sk32 - 24) / 3 + 5;
    }
    return sk32 / 4 - 1;
}

// 等级、经验值、属性，技能点对照表
//
// 数据唯一来源：lua-xxxx/data/level_exp.lua
// C++ 首次调用 level2_exp() 时通过 require("data.level_exp") 从 Lua
// 加载并缓存。 C++ 中不保留任何硬编码的经验值数据。

static std::vector<exp_t> s_level_exp;
static bool s_level_exp_loaded = false;

// 重置 level_exp 缓存（供测试使用）。
// 下次调用 level2_exp() 会重新从 Lua 加载。
void CChar::reset_level_exp_cache()
{
    s_level_exp.clear();
    s_level_exp_loaded = false;
}

// 算法生成近似经验值表（Lua 不可用时的备选方案）。
// 三段式模型：Lv1-75 幂函数(lv^2.8)，Lv76-157 幂函数(lv^3.2)，Lv158-175 指数。
// 与原始手工调数值的最大偏差约 45%（低等级与 Lv158+ 指数段），Lv76-157 偏差 <
// 5%。
static void generate_level_exp_approx(exp_t* out, int count)
{
    for (int i = 0; i < count; i++) {
        int lv = i + 1;
        if (lv <= 5) {
            // 低等级：手工调数值太小，公式偏差大，用简单二次近似
            // 实际值: 0, 100, 350, 750, 1300
            out[i] = static_cast<exp_t>(50.0 * lv * lv - 50.0 * lv);
        }
        else if (lv <= 75) {
            // 幂函数拟合: exp ≈ 8.16 * lv^2.81
            out[i] = static_cast<exp_t>(8.16 * pow((double)lv, 2.81));
        }
        else if (lv <= 157) {
            // 幂函数拟合: exp ≈ 1.99 * lv^3.21（最大误差 1.2%）
            out[i] = static_cast<exp_t>(1.99 * pow((double)lv, 3.21));
        }
        else {
            // 指数拟合（对数线性最小二乘）: exp ≈ e^(0.304593 * lv - 31.61993)
            double v = exp(0.304593 * lv - 31.61993);
            const double exp_max
                = static_cast<double>((std::numeric_limits<exp_t>::max)());
            if (v >= exp_max) v = exp_max;
            out[i] = static_cast<exp_t>(v);
        }
    }
}

static void ensure_level_exp_loaded()
{
    if (s_level_exp_loaded) return;
    s_level_exp_loaded = true;

    s_level_exp.resize(NORMAL_MAX_LEVEL, 0);

    // 尝试从 Lua 加载精确数据
    bool loaded = false;
    if (g_LuaEngine) {
        lua_State* L = g_LuaEngine->get_state();
        if (L) {
            // require("data.level_exp")
            lua_getglobal(L, "require");
            if (lua_isfunction(L, -1)) {
                lua_pushstring(L, "data.level_exp");
                if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_istable(L, -1)) {
                    for (int i = 1; i <= NORMAL_MAX_LEVEL; i++) {
                        lua_rawgeti(L, -1, i);
                        if (lua_isnumber(L, -1)) {
                            s_level_exp[i - 1]
                                = static_cast<exp_t>(lua_tointeger(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                    loaded = true;
                }
                lua_settop(L, 0);
            }
            else {
                lua_pop(L, 1);
            }
        }
    }

    if (!loaded) {
        // Lua 不可用或加载失败：用算法生成近似表
        generate_level_exp_approx(s_level_exp.data(), NORMAL_MAX_LEVEL);
        LOG_WARN("level_exp: Lua 加载失败，使用算法近似生成");
    }
    else {
        LOG_INFO("level_exp: 从 Lua 加载完成（data.level_exp）");
    }
}

exp_t CChar::level2_exp(mapping_value_t lv)
{
    ensure_level_exp_loaded();

    if (lv < 1) return 0;
    if (lv >= NORMAL_MAX_LEVEL)
        return static_cast<exp_t>(1917735009 + (int64_t)lv * 100000000);
    return s_level_exp[static_cast<size_t>(lv - 1)];
}

// 根据经验值计算等级
level_t CChar::exp2level(exp_t exp)
{
    level_t lv = 0;
    while (level2_exp(lv) <= exp) lv++;

    return lv - 1;
}

// 死亡技能惩罚：当前策略为死亡不掉技能，直接返回成功。
int CChar::skill_death_penalty() { return 1; }

int CChar::skill_death_recover()
{
    int32_t amount;

    const char* skill_h = _M.querystr("death/skill_name");

    if (DIFFERSTR(skill_h, "all")) {
        int32_t level_h = query_skill(skill_h, 1);

        if (!learned.query(skill_h)) {
            amount = (level_h + 1) * (level_h + 1) * 3 / 4;
            learned.set(skill_h, amount);
        }
        else {
            skills.set(skill_h, level_h + 1);
        }
    }
    else  // 全恢复一级
    {
        CVector v;
        skills.keys(v);

        for (int i = 0; i < v.count(); i++) {
            const char* sk      = v[i];
            skill_level_t level = static_cast<skill_level_t>(skills[sk]);
            if (!learned.query(sk)) {
                amount = (level + 1) * (level + 1) * 3 / 4;
                learned.set(sk, amount);
            }
            else {
                skills.set(sk, level + 1);
            }
        }
    }

    flush_my_skill(this);

    return 1;
}

// 消除判师记录
void CChar::remove_betrayer(CChar* me)
{
    mapping_value_t lv = me->query("betrayer");
    me->del("betrayer");
    me->add("erase/betrayer", lv);

    // 全部武功降lv * 20级
    CMapping* skills  = me->query_skills();
    CMapping* learned = me->query_learned();

    lv *= 20;

    CVector v;

    skills->keys(v);
    for (int i = 0; i < v.count(); i++) {
        learned->del(v[i]);
        me->map_skill(v[i]);
        me->prepare_skill(v[i]);
        if ((*skills)[v[i]] > lv)
            (*skills)[v[i]] -= lv;
        else
            skills->del(v[i]);
    }

    me->flush_my_skill(me);

    LOG_INFO("%s判师，消除判师记录，全部技能降%lld级！", me->name(1),
             (long long)lv);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:remove_betrayer() - 清除判师记录（CChar 静态方法，作用于 self）
int l_remove_betrayer(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar::remove_betrayer(ch);
    return 0;
}
