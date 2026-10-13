/**
 * @file SkillMgr.cpp
 * @brief 技能 / 状态管理  */

#include "SkillMgr.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "Skill.h"
#include "Condition.h"
#include "../util/Logger.h"

//---------------------------------------------------------
// 技能加载
//---------------------------------------------------------
std::unique_ptr<CSkill> create_skill(string skill_name);

CSkill* load_skill(const char* base_name)
{
    iterator_skill p = g_skill_list.find(base_name);
    if (p != g_skill_list.end()) return p->second;

    std::unique_ptr<CSkill> sk = create_skill(base_name);
    if (!sk) return 0;
    sk->set("base_name", base_name);
    sk->create();
    CSkill* ret             = sk.release();
    g_skill_list[base_name] = ret;
    return ret;
}

//---------------------------------------------------------
// 状态加载
//---------------------------------------------------------
std::unique_ptr<CCondition> create_condition(string base_name);
CCondition* load_condition(const char* base_name)
{
    iterator_condition p = g_condition_list.find(base_name);
    if (p != g_condition_list.end()) return p->second;

    std::unique_ptr<CCondition> cnd = create_condition(base_name);
    if (!cnd) return 0;
    cnd->set("base_name", base_name);
    CCondition* ret             = cnd.release();
    g_condition_list[base_name] = ret;
    return ret;
}

//---------------------------------------------------------
// 技能释放
//---------------------------------------------------------
void skill_release()
{
    iterator_skill p;
    for (p = g_skill_list.begin(); p != g_skill_list.end(); p++)
        delete p->second;

    g_skill_list.clear();
}

//---------------------------------------------------------
// 状态释放
//---------------------------------------------------------
void condition_release()
{
    iterator_condition p;
    for (p = g_condition_list.begin(); p != g_condition_list.end(); p++)
        delete p->second;

    g_condition_list.clear();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// load_skill(name) -> object - 加载技能
int l_load_skill(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    lua_bind_push_object(L, load_skill(name));
    return 1;
}
