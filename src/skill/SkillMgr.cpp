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

// 摘除单个技能，并连带摘除继承自它的技能（传递闭包）。
// 继承项的归属指针指向基础技能：基础技能被销毁后，继承者必须一起重建，
// 否则继承项里的归属指针会悬垂（施法转发时会用到）。
void skill_release_one(const char* base_name)
{
    if (!base_name || !base_name[0]) return;

    iterator_skill p = g_skill_list.find(base_name);
    if (p == g_skill_list.end()) return;

    std::vector<CSkill*> dying;
    dying.push_back(p->second);
    g_skill_list.erase(p);

    // 传递闭包：继承自待删技能的技能也一并摘除（下次 load_skill
    // 重建并重新继承）
    for (size_t i = 0; i < dying.size(); i++) {
        CSkill* cur       = dying[i];
        iterator_skill it = g_skill_list.begin();
        while (it != g_skill_list.end()) {
            if (it->second->depends_on(cur)) {
                dying.push_back(it->second);
                it = g_skill_list.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    for (CSkill* sk : dying) delete sk;
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
