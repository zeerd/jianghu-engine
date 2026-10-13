/**
 * @file CharSkill.cpp
 * @brief implementation of CChar (CharSkill). */

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

// 设置技能
int CChar::set_skill(const char* skill, int level)
{
    CSkill* sk = load_skill(skill);

    if (!sk) {
        LOG_ERROR("F_SKILL: No such skill (%s)", skill);
        return 0;
    }
    if (sk->query("selfcreate")) sk->reset(this);

    skills.set(skill, level);
    return 1;
}

int CChar::delete_skill(const char* skill)
{
    if (!skill[0]) return 0;
    if (skills.undefinedp(skill)) return 0;

    skills.del(skill);
    learned.del(skill);

    return 1;
}

int CChar::update_condition()
{
    CMapping* conditions;
    CVector v;
    CCondition* cnd;
    int update_flag = 0, flag;
    char key[255];

    conditions = querymap("condition");
    if (!conditions) return 0;

    conditions->keys(v);

    for (int i = 0; i < v.count(); i++) {
        if (!strncmp(v[i], "damage_", 7)) continue;
        cnd = load_condition(v[i]);

        flag = cnd->update_condition(this, (*conditions)[v[i]]);

        if (!(flag & CND_CONTINUE))  // 删除该项目
        {
            conditions->del(v[i]);
            snprintf(key, 255, "damage_%s", v[i]);
            conditions->del(key);
        }

        update_flag |= flag;
    }

    // 检查Conditions是否已无
    if (!conditions->get_count()) {
        del("condition");
        delete_temp("condition");
    }

    return update_flag;
}

bool CChar::apply_condition(const char* cnd, int32_t duration, int damage)
{
    char key[80], key1[255];

    snprintf(key, 80, "condition/%s", cnd);
    snprintf(key1, 255, "condition/damage_%s", cnd);

    if (query_temp("apply/poi_shield")) return false;

    if (duration) {
        if (query(key1) > damage)
            return true;  // 如果新的条件伤害比原有的小，则后出现的条件忽略
        if (!query_condition(cnd)) {
            CCondition* condi = load_condition(cnd);
            if (!condi->init(this)) {
                // 脚本缺失/损坏：条件项不写入，避免出现一个心跳的假状态
                LOG_ERROR("apply_condition: script missing or broken (%s)",
                          cnd);
                return false;
            }
            condi->set_name(cnd);
        }

        set(key, duration);
        if (damage <= 0) damage = 0;
        if (damage > 1000) damage = 1000;
        set(key1, damage);
    }
    else {
        del(key);
        del(key1);
    }
    return true;
}

void CChar::map_skill(const char* skill, const char* mapped_to)
{
    if (!mapped_to) {
        skill_map.del(skill);
        return;
    }

    if (!load_skill(skill)) {
        return;
    }

    if (!skills.query(mapped_to)) return;

    skill_map.set(skill, mapped_to);
}

void CChar::prepare_skill(const char* skill, const char* mapped_to)
{
    if (!mapped_to) {
        skill_prepare.del(skill);
        return;
    }

    if (!load_skill(skill)) {
        return;
    }

    if (!skills.query(mapped_to)) return;

    skill_prepare.set(skill, mapped_to);
}

const char* CChar::query_skill_mapped(const char* skill)
{
    return skill_map.querystr(skill);
}

const char* CChar::query_skill_prepared(const char* skill)
{
    return skill_prepare.querystr(skill);
}

int CChar::query_skill(const char* skill, int raw)
{
    if (!raw) {
        int s;
        char key[80];

        snprintf(key, 80, "apply/%s", skill);

        s = query_temp(key);
        s += skills.query(skill) / 2;

        const char* key2 = skill_map.querystr(skill);
        if (key2[0]) s += skills.query(key2);

        return s;
    }

    return skills.query(skill);
}

CMapping* CChar::query_skill_map() { return &skill_map; }

CMapping* CChar::query_skill_prepare() { return &skill_prepare; }

void CChar::clear_skill_prepare() { skill_prepare.clear(); }

void CChar::clear_skill_map() { skill_map.clear(); }

void CChar::clear_skill()
{
    skills.clear();
    learned.clear();
}

void CChar::improve_skill(const char* skill, int amount, int weak_mode)
{
    CSkill* sk;
    char msg[255];

    if (!skill[0]) return;

    sk = load_skill(skill);
    if (sk->query("fail_ob")) return;

    if (!weak_mode || !userp(this)) {
        if (skills.undefinedp(skill)) skills.set(skill, 0l);
    }

    learned.add(skill, amount);

    if ((!weak_mode || !userp(this))
        && learned.query(skill)
               > (skills.query(skill) + 1) * (skills.query(skill) + 1)) {
        skills.add(skill, 1);
        learned.set(skill, 0l);
        tell_object(this,
                    snprintf(msg, 255, "$YEL你的「%s」进步了！", sk->name(1)));

        sk->skill_improved(this);

        // 通知
        flush_my_skill(this);

        if (userp(this)) {
            LOG_DEBUG("improve_skill(%s(%s) %s(%d))", name(1), id(1), skill,
                      query_skill(skill, 1));
        }
    }
}

CMapping* CChar::query_skills() { return &skills; }

CMapping* CChar::query_learned() { return &learned; }

// 查询该技能升级所需要的点数
int32_t CChar::skill_need_points(const char* skill)
{
    int32_t need_pt = (skills.query(skill) + 1) * (skills.query(skill) + 1)
                      - learned.query(skill);

    // 再附加上技能数量的因素
    // 20技能以上，每多一项目技能要花比平时多10%的经验
    if (skills.get_count() > 20)
        need_pt = need_pt * (10 + skills.get_count() - 20) / 10;

    if (need_pt < 1) return 1;

    return need_pt;
}

// 返回最低技能的等级
int CChar::mini_skill()
{
    CVector v;
    int max_skill = 9999;

    if (!skills.get_count()) return 0;

    skills.keys(v);

    for (int i = 0; i < v.count(); i++) {
        int lv = skills[v[i]];
        if (max_skill > lv) max_skill = lv;
    }

    return max_skill;
}

int CChar::can_learn(const char* skillname)
{
    if (!skillname[0]) return 0;
    CMapping* m = _M.querymap("forget");
    if (!m) return 1;
    return m->undefinedp(skillname);
}

bool CChar::apply_condition(CChar* who, const char* cnd, int32_t duration,
                            int damage)
{
    if (query_temp("apply/poi_shield")) return false;

    if (!apply_condition(cnd, duration, damage)) return false;

    if (who != NULL) {
        char key2[255];
        snprintf(key2, 255, "condition/who_%s", cnd);
        if (duration) {
            set_temp(key2, who->object_id());
        }
        else {
            delete_temp(key2);
        }
    }
    return true;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:query_skill(skill[, raw]) -> number
// raw=1 时返回原始技能等级（skills.query），等价 C++ 的 query_skill(skill, 1)
int l_query_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    const char* skill = luaL_checkstring(L, 2);
    int raw           = (int)luaL_optinteger(L, 3, 0);
    lua_pushinteger(L, ch->query_skill(skill, raw));
    return 1;
}

// self:mini_skill() -> number - 最低武功等级
int l_mini_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->mini_skill());
    return 1;
}

// self:set_skill(skill, level)
int l_set_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    const char* skill = luaL_checkstring(L, 2);
    int level         = (int)luaL_checkinteger(L, 3);
    ch->set_skill(skill, level);
    return 0;
}

// self:delete_skill(skill) - 删除技能
int l_delete_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    const char* skill = luaL_checkstring(L, 2);
    ch->delete_skill(skill);
    return 0;
}

// self:apply_condition(cnd, duration[, who])
int l_apply_condition(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    const char* cnd  = luaL_checkstring(L, 2);
    int32_t duration = (int32_t)luaL_checkinteger(L, 3);
    int32_t damage   = 0;
    if (lua_isnumber(L, 4)) damage = (int32_t)luaL_checkinteger(L, 4);
    CChar* who = NULL;
    if (lua_isuserdata(L, 5)) who = dynamic_cast<CChar*>(get_object(L, 5));
    if (who)
        ch->apply_condition(who, cnd, duration, damage);
    else
        ch->apply_condition(cnd, duration, damage);
    return 0;
}

// self:query_skill_mapped(skill) -> string
int l_query_skill_mapped(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    const char* val   = ch->query_skill_mapped(skill);
    lua_pushstring(L, val ? val : "");
    return 1;
}

// self:query_skill_map_list() -> table|nil
// 返回技能映射表（usage -> 映射的高级技能名）的条目数组 [{usage, mapped}]
int l_query_skill_map_list(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    CMapping* map = ch->query_skill_map();
    if (!map) {
        lua_pushnil(L);
        return 1;
    }

    CVector v;
    map->keys(v);
    if (v.count() <= 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, (int)v.count(), 0);
    int idx = 1;
    for (long i = 0; i < v.count(); i++) {
        const char* usage = v[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, usage ? usage : "");
        lua_setfield(L, -2, "usage");
        const char* mapped = map->querystr(usage);
        lua_pushstring(L, mapped ? mapped : "");
        lua_setfield(L, -2, "mapped");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// self:query_hardcore_skills() -> table|nil -
// 返回所有“核心”已学技能的技能名数组
int l_query_hardcore_skills(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    CMapping* skills = ch->query_skills();
    if (!skills) {
        lua_pushnil(L);
        return 1;
    }

    CVector v;
    skills->keys(v);
    int cnt = (int)v.count();
    int idx = 1;
    lua_createtable(L, 0, 0);
    for (int i = 0; i < cnt; i++) {
        const char* sn = v[i];
        CSkill* sk     = load_skill(sn);
        if (sk && sk->query("hardcore") != 0) {
            lua_pushstring(L, sn ? sn : "");
            lua_rawseti(L, -2, idx++);
        }
    }
    if (idx == 1) {
        lua_pushnil(L);
        return 1;
    }
    return 1;
}

// self:inc_skill(skill, delta) - 增加指定技能等级
int l_inc_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* sn = luaL_checkstring(L, 2);
    int delta      = (int)luaL_checkinteger(L, 3);

    CMapping* skills = ch->query_skills();
    if (!skills) return 0;
    (*skills)[sn] = (*skills)[sn] + delta;
    return 0;
}

// self:query_skill_prepared(skill) -> string
int l_query_skill_prepared(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    const char* val   = ch->query_skill_prepared(skill);
    lua_pushstring(L, val ? val : "");
    return 1;
}

// self:query_skill_prepare_list() -> table|nil
// 返回技能备战映射表（usage -> 备战技能名）的条目数组 [{usage, prepared}]
int l_query_skill_prepare_list(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    CMapping* map = ch->query_skill_prepare();
    if (!map) {
        lua_pushnil(L);
        return 1;
    }

    CVector v;
    map->keys(v);
    if (v.count() <= 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, (int)v.count(), 0);
    int idx = 1;
    for (long i = 0; i < v.count(); i++) {
        const char* usage = v[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, usage ? usage : "");
        lua_setfield(L, -2, "usage");
        const char* prepared = map->querystr(usage);
        lua_pushstring(L, prepared ? prepared : "");
        lua_setfield(L, -2, "prepared");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// self:improve_skill(skill, amount[, weak_mode])
// weak_mode 对应 C++ CChar::improve_skill 的第三参数（默认 0）
int l_improve_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    int amount        = (int)luaL_checkinteger(L, 3);
    int weak_mode     = (int)luaL_optinteger(L, 4, 0);
    ch->improve_skill(skill, amount, weak_mode);
    return 0;
}

// self:map_skill(skill, mapped_to)
int l_map_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    const char* mto   = luaL_optstring(L, 3, NULL);
    ch->map_skill(skill, mto);
    return 0;
}

// self:prepare_skill(skill, mapped_to)
int l_prepare_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    const char* mto   = luaL_optstring(L, 3, NULL);
    ch->prepare_skill(skill, mto);
    return 0;
}

// self:clear_skill() - 清空已学技能
int l_clear_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->clear_skill();
    return 0;
}

// self:clear_skill_map() - 清空技能映射
int l_clear_skill_map(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->clear_skill_map();
    return 0;
}

// self:clear_skill_prepare() - 清空技能备战
int l_clear_skill_prepare(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->clear_skill_prepare();
    return 0;
}

// self:skill_need_points(skill_name) -> int32_t（CChar 计算技能所需点数）
int l_skill_need_points(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* skill = luaL_checkstring(L, 2);
    lua_pushinteger(L, ch->skill_need_points(skill));
    return 1;
}

// self:query_skills() -> table|nil（技能映射：技能名 -> 等级）
int l_query_skills(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CMapping* m = c->query_skills();
    if (!m) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    for (std::unordered_map<std::string, CMapping::svalue_t>::iterator it
         = m->m_map.begin();
         it != m->m_map.end(); ++it) {
        if (it->second.type == T_NUMBER)
            lua_pushinteger(L, it->second.u.number);
        else if (it->second.type == T_STRING)
            lua_pushstring(L, it->second.u.string.get());
        else
            continue;
        lua_setfield(L, -2, it->first.c_str());
    }
    return 1;
}
