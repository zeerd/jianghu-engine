/**
 * @file Skill.cpp
 * @brief implementation of the CSkill class. */

#include <windows.h>

#include "Skill.h"
#include "Global.h"
#include "Sql.h"
#include "LuaBindEnums.h"
#include "ServerConfig.h"
#include "CharRules.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CSkill::CSkill() {}

CSkill::~CSkill() { m_magic_list.clear(); }

int CSkill::valid_learn(CChar* me) { return 1; }

int CSkill::query_magic_hostile(const char* magic)
{
    if (!magic) return 1;
    CastList_t::iterator p = m_magic_list.find(magic);
    if (p == m_magic_list.end()) return 1;  // 未注册：保守按敌意
    return p->second->hostile;
}

int CSkill::query_magic_target(const char* magic)
{
    if (!magic) return CAST_TO_RANDOM;
    CastList_t::iterator p = m_magic_list.find(magic);
    if (p == m_magic_list.end()) return CAST_TO_RANDOM;
    return p->second->target;
}

const CSkill::Magic_t* CSkill::find_magic(const char* magic) const
{
    if (!magic) return NULL;
    CastList_t::const_iterator p = m_magic_list.find(magic);
    if (p == m_magic_list.end()) return NULL;
    return p->second.get();
}

bool CSkill::has_magic(const char* magic) { return find_magic(magic) != NULL; }

CSkill* CSkill::magic_owner(const char* magic)
{
    const Magic_t* entry = find_magic(magic);
    if (!entry) return NULL;
    return entry->owner ? entry->owner : this;
}

// 本技能是否继承了 basic 的法术（继承项的归属指向 basic）。
// 基础技能被销毁前据此找出依赖者：归属指针一旦悬垂，施法转发会踩到已释放对象。
bool CSkill::depends_on(const CSkill* basic) const
{
    if (!basic) return false;
    CastList_t::const_iterator it = m_magic_list.begin();
    for (; it != m_magic_list.end(); ++it) {
        if (it->second->owner == basic) return true;
    }
    return false;
}

// 继承基础技能的法术：同名不覆盖（本技能自己注册的优先）。
// 继承项 owner 指向基础技能，施法时转发给归属技能执行，
// 保证法术逻辑仍由基础技能的脚本实现。
void CSkill::inherit_magics(const CSkill* basic)
{
    if (!basic || basic == this) return;

    for (const auto& kv : basic->m_magic_list) {
        if (m_magic_list.count(kv.first)) continue;
        std::unique_ptr<Magic_t> copy(new Magic_t(*kv.second));
        copy->owner            = const_cast<CSkill*>(basic);
        m_magic_list[kv.first] = std::move(copy);
    }
}

CMapping* CSkill::query_action(CChar* me) { return 0; }

int32_t CSkill::query_attr(int attr)
{
    switch (attr) {
        case IS_SKILL:
            return 1;
        default:
            return CMainObject::query_attr(attr);
    }

    return 1;
}

// 下载 C=DOWNSKILL S=skill N=skill_name P=picture E=valid_enable T=target
void CSkill::download(CChar* me)
{
    char str[255];

    snprintf(str, 255, "&C=%d&S=%s&N=%s&P=%d&E=%s&", DOWNSKILL,
             querystr("base_name"), name(1), query("icon"),
             querystr("base_skill"));

    if (query("special")) strcat(str, "s=1&");

    me->send_command(str, SEND_WAIT_BUFFERED);

    // 传送该技能支持的法术 C=DOWNMAGIC S=skill M=magic N=name R=mp A=ap
    // a=animate, V=valid_cast
    CastList_t::iterator p;

    for (p = m_magic_list.begin(); p != m_magic_list.end(); p++) {
        Magic_t* magic = p->second.get();
        snprintf(str, 255, "&C=%d&S=%s&M=%s&N=%s&R=%d&A=%d&T=%d&V=%d&",
                 DOWNMAGIC, querystr("base_name"), p->first.c_str(),
                 magic->name.c_str(), (int)magic->mp, (int)magic->ap,
                 magic->target, magic->valid_cast);

        me->send_command(str, SEND_WAIT_BUFFERED);
    }
}

void CSkill::create() {}

void CSkill::reset(CChar* me) {}

void CSkill::skill_improved(CChar* me) {}

int CSkill::cast(string magic, CChar* me, CContainer* target)
{
    // 继承自基础技能的法术：转发给归属技能执行（法术逻辑在基础技能脚本里）
    const Magic_t* entry = find_magic(magic.c_str());
    if (entry && entry->owner && entry->owner != this) {
        // 转发前仍按本技能的前置条件校验（内功搭配限制等）
        if (!check_cast_force(me)) return 0;
        return entry->owner->cast(magic, me, target);
    }

    // C++ 基类不实现法术：法术一律由 Lua 脚本注册（见 CLuaSkill::cast）
    return 0;
}

int CSkill::valid_effect(CChar* me, CContainer* weapon, char* action_name,
                         int skill)
{
    return 1;
}

const char* CSkill::type() { return "martial"; }

const char* CSkill::query_dodge_msg(CChar* me) { return 0; }

const char* CSkill::query_parry_msg(CContainer* weapon) { return 0; }

int CSkill::valid_enable(string usage) { return 0; }

int CSkill::practice_skill(CChar* me) { return 1; }

int CSkill::valid_combine(string combo) { return 0; }

int CSkill::valid_force(const char* arg)
{
    const char* valid_force = querystr("valid_force");
    if (valid_force[0] && DIFFERSTR(valid_force, arg))
        return notify_fail("%s必须与%s配合使用。", name(1),
                           load_skill(valid_force)->name(1));

    return 1;
}

int CSkill::valid_force_pro(CChar* me) { return 0; }

// 施法内功前置闸门：
//   1) 已准备（映射）的内功被本技能接受；
//   2) 技能脚本扩展 valid_force_pro；
//   3) 引擎配置 force_no_need_prepare：学会即算；
//   4) 数据集通用规则 hooks.char_rule("cast_force", me,
//   this)（如前世修习过的内功）。
// 返回非 0 表示满足；0 表示不满足（由调用方拒绝施法）。
int CSkill::check_cast_force(CChar* me)
{
    bool gift = false;

    if (CServerConfig::force_no_need_prepare()) {
        CMapping* skills = me->query_skills();
        CVector v;
        skills->keys(v);
        for (int i = 0; i < v.count(); i++) {
            if (valid_force(v[i])) {
                gift = true;
                break;
            }
        }
    }

    if (!gift) gift = char_rule_yes("cast_force", me, this);

    if (!gift && !valid_force(me->query_skill_mapped("force"))
        && !valid_force_pro(me))
        return 0;

    return 1;
}

// 开始激励
void CSkill::start_powerup(CChar* me, int duration, int attack, int dodge,
                           const char* msg)
{
    me->add_temp("apply/attack", attack);
    me->add_temp("apply/dodge", dodge);
    me->set_temp("powerup/attack", attack);
    me->set_temp("powerup/dodge", dodge);
    me->set_temp("powerup/msg", msg);

    me->call_out(remove_powerup, duration, 0);
}

// 消除激励
void CSkill::remove_powerup(CContainer* ob, int32_t silence, int32_t param2)
{
    CChar* me = static_cast<CChar*>(ob);

    if (!me->querymap_temp("powerup")) return;

    me->add_temp("apply/attack", -me->query_temp("powerup/attack"));
    me->add_temp("apply/dodge", -me->query_temp("powerup/dodge"));

    me->send_state(false);

    if (!silence) tell_object(me, me->querystr_temp("powerup/msg"));

    me->delete_temp("powerup");
}

char* CSkill::hit_ob(CChar* me, CChar* victim, int32_t& damage) { return 0; }

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:add_magic(name, displayName, mp, ap, target, validCast, luaFuncName
//                [, hostile])
// 注册一个 Lua 法术；hostile 缺省为 1（敌意行为）
int l_add_magic(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    // displayName 从 Lua 传入是 UTF-8，需转成 GBK（服务器内部编码）
    const char* name        = luaL_checkstring(L, 2);
    const char* displayName = luaL_checkstring(L, 3);
    int32_t mp              = (int32_t)luaL_optinteger(L, 4, 10);
    int32_t ap              = (int32_t)luaL_optinteger(L, 5, 100);
    int target              = (int)luaL_optinteger(L, 6, 3);
    int validCast           = (int)luaL_optinteger(L, 7, 0);
    const char* luaFuncName = luaL_checkstring(L, 8);
    int hostile             = (int)luaL_optinteger(L, 9, 1);

    int ret = 0;
    // 技能均支持 add_magic
    CLuaSkill* skill = dynamic_cast<CLuaSkill*>(obj);
    if (skill)
        ret = skill->lua_add_magic(name, displayName, mp, ap, target, validCast,
                                   luaFuncName, hostile);
    lua_pushinteger(L, ret);
    return 1;
}

// self:valid_force(name) -> 1|0
// 查询某内功名是否被本技能接受；复用技能自身的 valid_force（含 Lua 脚本覆写）。
// 供数据集 hook（如 hooks.char_rule 的 cast_force）复用技能的 force 判定。
int l_valid_force(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    CSkill* skill    = dynamic_cast<CSkill*>(obj);
    if (!skill) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const char* name = luaL_checkstring(L, 2);
    lua_pushinteger(L, skill->valid_force(name));
    return 1;
}

// Skill.h 施法时机/目标枚举 → Lua 全局常量
static const LuaEnumEntry kSkillCastEnums[] = {
    {"CAST_IN_NONE", CSkill::CAST_IN_NONE},
    {"CAST_IN_COMMON", CSkill::CAST_IN_COMMON},
    {"CAST_IN_COMBAT", CSkill::CAST_IN_COMBAT},
    {"CAST_IN_BOTH", CSkill::CAST_IN_BOTH},
    {"CAST_TO_NONE", CSkill::CAST_TO_NONE},
    {"CAST_TO_ME", CSkill::CAST_TO_ME},
    {"CAST_TO_OTHER", CSkill::CAST_TO_OTHER},
    {"CAST_TO_RANDOM", CSkill::CAST_TO_RANDOM},
    {"CAST_TO_FRIEND", CSkill::CAST_TO_FRIEND},
    {"CAST_TO_TEAM", CSkill::CAST_TO_TEAM},
    {"CAST_TO_OTHER_TEAM", CSkill::CAST_TO_OTHER_TEAM},
    {nullptr, 0},
};

void lua_bind_reg_skill_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kSkillCastEnums);
}
