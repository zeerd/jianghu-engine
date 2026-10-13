/**
 * @file Skill.cpp
 * @brief implementation of the CSkill class. */

#include <windows.h>

#include "Skill.h"
#include "Global.h"
#include "Sql.h"
#include "LuaBindEnums.h"
#include "ServerConfig.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
int check_ap(int ap, CChar* me);
void resume_ap(int ap, CChar* me);

// 加载技能
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

static void remove_no_cast(CContainer* ob, int32_t param1, int32_t param2)
{
    ob->delete_temp("no_cast");
}

int CSkill::cast(string magic, CChar* me, CContainer* target)
{
    CastList_t::iterator p;
    int32_t object_id = 0;

    bool gift   = false;
    CMapping* m = me->querymap("newlife/forces");
    if (m != NULL) {
        CVector key;
        m->keys(key);
        for (int i = 0; i < key.count(); i++) {
            if (valid_force(key[i])) {
                gift = true;
                break;
            }
        }
    }

    if (CServerConfig::force_no_need_prepare() && !gift) {
        CMapping* skills;
        skills = me->query_skills();
        CVector v;
        skills->keys(v);
        for (int i = 0; i < v.count(); i++) {
            if (valid_force(v[i])) {
                gift = true;
                break;
            }
        }
    }

    // 增加内功对法术的限制
    if (!gift && !valid_force(me->query_skill_mapped("force"))
        && !valid_force_pro(me))
        return 0;

    if (target) object_id = target->object_id();

    for (p = m_magic_list.begin(); p != m_magic_list.end(); p++)
        if (p->first == magic) {
            Magic_t* magic_name = p->second.get();

            // 先判断AP是否足够！
            int32_t ap
                = magic_name->ap
                  * (120 - me->query_skill(_M.querystr("base_name"), 1) / 4
                     - me->query("level") / 2)
                  / 100;

            if (ap < 0) ap = 1;

            if (!check_ap(ap, me)) return 0;

            int sub_ap = 0;
            if (me->is_fighting()) sub_ap = 1;

            int ret;
            if ((ret = (*magic_name->func)(me, target))) {
                me->set_temp("no_cast", 1);
                me->call_out(remove_no_cast, ret);

                if (object_id == g_last_destruct_object) target = NULL;

                if (sub_ap) resume_ap(ap, me);

                // 传输状态
                if (target && magic_name->target == CAST_TO_OTHER)
                    static_cast<CChar*>(target)->send_state();
                me->send_state();
                return 1;
            }
        }

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

CSkill::Magic_t* CSkill::add_magic(string magic, MAGICFUNCTIONP* func)
{
    auto magic_name = std::make_unique<Magic_t>();

    magic_name->func    = func;
    Magic_t* ret        = magic_name.get();
    m_magic_list[magic] = std::move(magic_name);

    return ret;
}

int CSkill::valid_force(const char* arg)
{
    const char* valid_force = querystr("valid_force");
    if (valid_force[0] && DIFFERSTR(valid_force, arg))
        return notify_fail("%s必须与%s配合使用。", name(1),
                           load_skill(valid_force)->name(1));

    return 1;
}

int CSkill::valid_force_pro(CChar* me) { return 0; }

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
    // 技能（各基类模板实例）均支持 add_magic
    CLuaSkill* skill = dynamic_cast<CLuaSkill*>(obj);
    if (skill)
        ret = skill->lua_add_magic(name, displayName, mp, ap, target, validCast,
                                   luaFuncName, hostile);
    else {
        CLuaForce* forceSkill = dynamic_cast<CLuaForce*>(obj);
        if (forceSkill)
            ret = forceSkill->lua_add_magic(name, displayName, mp, ap, target,
                                            validCast, luaFuncName, hostile);
        else {
            CLuaThrowing* throwingSkill = dynamic_cast<CLuaThrowing*>(obj);
            if (throwingSkill)
                ret = throwingSkill->lua_add_magic(name, displayName, mp, ap,
                                                   target, validCast,
                                                   luaFuncName, hostile);
        }
    }
    lua_pushinteger(L, ret);
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
