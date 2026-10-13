/**
 * @file CharCombat.cpp
 * @brief implementation of CChar (CharCombat). */

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
#include "LuaHook.h"

#include "Sql.h"

// 属性增减生效后的钩子通知：hooks.receive(kind, me, who, attr, amount)。
// 返回值不消费；钩子缺席（引擎未初始化 / 未定义）时静默跳过。
static void notify_receive(CChar* me, const char* kind, CChar* who,
                           const char* attr, int64_t amount)
{
    CLuaHookCall hook("receive");
    hook.add_str(kind).add_object(me).add_object(who).add_str(attr).add_int(
        amount);
    hook.call();
}

int32_t CChar::receive_damage(const char* szType, int32_t damage, CChar* who)
{
    int val;

    if (damage <= 0) return 0;
    if (DIFFERSTR(szType, "hp") && DIFFERSTR(szType, "mp")) return 0;

    CChar* owner = NULL;
    if (who && who->querystr("master_id")) {
        owner = (CChar*)find_online(who->querystr("master_id"));
        if (owner) who = owner;
    }

    if (!owner && who && who->query("owner")) {
        owner = (CChar*)find_object(who->query("owner"), 1);
        if (owner) who = owner;
    }

    if (who && who != this) set_temp("last_damage_from", who->object_id());

    val = query(szType) - damage;

    if (val >= 0)
        set(szType, val);
    else {
        set(szType, -1);

        if (!who) {
            who = (CChar*)find_object(query_temp("last_damage_from"), 1);
        }

        if (who && who != this && living(this) && userp(who)
            && !querystr_temp("last_fainted_from")[0]) {
            set_temp("last_fainted_from", who->querystr("id"));
            if (userp(this)) call_out(remove_faint, 60);
        }
    }

    set_heart_beat(1);

    if (who && wizardp(who)
        && EQUALSTR(who->querystr("env/combat"), "verbose")) {
        LOG_DEBUG("receive_damage: %s, damage:%s fainted:%s", name(),
                  who->id(1), querystr_temp("last_fainted_from"));
    }

    notify_receive(this, "damage", who, szType, damage);
    return damage;
}

int32_t CChar::receive_wound(const char* type, int32_t damage, CChar* who)
{
    int32_t val;
    char eff_key[20];

    if (damage <= 0) return 0;
    if (DIFFERSTR(type, "hp") && DIFFERSTR(type, "mp")) return 0;

    snprintf(eff_key, 20, "eff_%s", type);

    CChar* owner = NULL;
    if (!owner && who && who->query("owner")) {
        CChar* owner = (CChar*)find_object(who->query("owner"), 1);
        if (owner) who = owner;
    }

    if (who && who != this) set_temp("last_damage_from", who->object_id());

    val = query(eff_key) - damage;

    if (val >= 0)
        set(eff_key, val);
    else {
        set(eff_key, -1);
        val = -1;

        if (!who) {
            who = (CChar*)find_object(query_temp("last_damage_from"), 1);
        }

        if (who && who != this && living(this) && userp(who)
            && !querystr_temp("last_fainted_from")[0]) {
            set_temp("last_fainted_from", who->querystr("id"));
            if (userp(this)) call_out(remove_faint, 60);
        }
    }

    if (query(type) > val) set(type, val);

    if (who && wizardp(who)
        && EQUALSTR(who->querystr("env/combat"), "verbose")) {
        LOG_DEBUG("receive_wound: %s damage:%s fainted:%s", name(), who->id(1),
                  querystr_temp("last_fainted_from"));
    }

    set_heart_beat(1);

    notify_receive(this, "wound", who, type, damage);
    return damage;
}

int32_t CChar::receive_curing(const char* szType, int32_t heal)
{
    int max, val;
    char max_key[20], eff_key[20];

    if (heal < 0) return 0;
    if (DIFFERSTR(szType, "hp") && DIFFERSTR(szType, "mp")) return 0;

    snprintf(max_key, 20, "max_%s", szType);
    snprintf(eff_key, 20, "eff_%s", szType);

    val = query(eff_key);
    max = query(max_key);

    int applied = val + heal > max ? max - val : heal;
    set(eff_key, val + applied);

    if (applied > 0) notify_receive(this, "curing", NULL, szType, applied);
    return applied;
}

int32_t CChar::receive_healing(const char* szType, int32_t heal)
{
    int val;
    int eff;
    const char* eff_key;

    if (heal < 0) return 0;
    if (DIFFERSTR(szType, "hp") && DIFFERSTR(szType, "mp")) return 0;

    if (EQUALSTR(szType, "hp"))
        eff_key = "eff_hp";
    else
        eff_key = "max_mp";

    eff = query(eff_key);
    val = query(szType);

    // 如果val > nEff，不补。
    if (val >= eff) return 0;

    int applied = val + heal > eff ? eff - val : heal;
    set(szType, val + applied);

    if (applied > 0) notify_receive(this, "healing", NULL, szType, applied);
    return applied;
}

// 查询一些属性，进行修正后的值
void CChar::start_busy(BUSYFUNCTION* new_busy, INTERRUPTFUNCTION* new_interrupt)
{
    if (!new_busy) return;

    if (m_interrupt) (*m_interrupt)(this, this, "next_busy");

    if (m_busy_count) m_busy_count = 0;
    m_busy_func = new_busy;
    m_interrupt = new_interrupt;
    set_heart_beat(1);
}

void CChar::start_busy(int start, INTERRUPTFUNCTION* new_interrupt, CChar* src)
{
    if (!start || !this) return;

    if (m_interrupt) (*m_interrupt)(this, this, "next_busy");

    if (m_busy_func) m_busy_func = 0;  // 互斥的

    m_busy_count = start;

    m_interrupt = new_interrupt;
    set_heart_beat(1);
}

void CChar::continue_action()
{
    if (m_busy_count > 0) {
        m_busy_count--;
        return;
    }

    if (m_busy_func) {
        if (!(*m_busy_func)(this)) {
            m_busy_func = 0;
            m_interrupt = 0;
        }
    }
    else {
        m_busy_func = 0;
        m_interrupt = 0;
    }
}

// 打断Busy
void CChar::interrupt_me(CChar* who, char* how)
{
    if (!m_busy_func && !m_busy_count) return;
    if (!m_interrupt) return;

    if ((*m_interrupt)(this, who, how)) {
        m_busy_func  = 0;
        m_busy_count = 0;
        m_interrupt  = 0;
    }
}

// 保存到服务器
extern char g_dbip[80], g_user[80], g_pwd[80];

void CChar::kill_ob(CChar* ob)
{
    set_killer(ob);
    fight_ob(ob);
}

void CChar::fight_ob(CChar* ob) { cmd_hit_ob(ob, ""); }

void CChar::set_killer(CChar* ob)
{
    char msg[80];

    if (m_killers.find(ob->querystr("id")) == m_killers.end())
        m_killers.append(ob->querystr("id"));

    tell_object(ob, snprintf(msg, 80, "$RED看起来%s想杀死你！", name()));
}

void CChar::remove_killer(CChar* ob) { m_killers.remove(ob->querystr("id")); }

void CChar::remove_all_killer()
{
    delete_temp("last_damage_from");
    m_killers.clear();
}

void CChar::attack(CChar* opponent) { g_combatd.fight(this, opponent); }

int32_t CChar::receive_poison(const char* type, int32_t damage,
                              const char* condi)
{
    char key2[255];
    snprintf(key2, 255, "condition/who_%s", condi);
    CChar* who = (CChar*)find_object(query_temp(key2), 1);
    if (who != NULL) {
        int poi = who->query_skill("poison", 1);
        if (poi > 0) {
            receive_damage("hp", damage * (poi / 100 + 1), who);
        }
    }
    return receive_wound(type, damage, who);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:wield_weapon(weapon) - 装备指定武器（在角色身上找到对应装备并装备）
int l_wield_weapon(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* w    = luaL_checkstring(L, 2);
    CContainer* item = ch->present(w, 0);
    if (item) {
        CLuaEquip* e = dynamic_cast<CLuaEquip*>(item);
        if (e) {
            lua_pushinteger(L, e->lua_do_wield());
            return 1;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}

// self:unwield_weapon(weapon) - 卸下指定武器
int l_unwield_weapon(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* w    = luaL_checkstring(L, 2);
    CContainer* item = ch->present(w, 0);
    if (item) {
        CLuaEquip* e = dynamic_cast<CLuaEquip*>(item);
        if (e) {
            lua_pushinteger(L, e->lua_do_un_wield());
            return 1;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}

// self:receive_curing(type, heal) -> number
int l_receive_curing(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* type = luaL_checkstring(L, 2);
    int32_t heal     = (int32_t)luaL_checkinteger(L, 3);
    lua_pushinteger(L, ch->receive_curing(type, heal));
    return 1;
}

// self:receive_healing(type, heal) -> number
int l_receive_healing(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* type = luaL_checkstring(L, 2);
    int32_t heal     = (int32_t)luaL_checkinteger(L, 3);
    lua_pushinteger(L, ch->receive_healing(type, heal));
    return 1;
}

// self:base_attack(opponent) - 调用基类 CChar::attack 开始/继续攻击
int l_base_attack(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar* op = NULL;
    if (lua_isuserdata(L, 2)) op = dynamic_cast<CChar*>(get_object(L, 2));
    if (op) ch->CChar::attack(op);
    return 0;
}

// self:receive_damage(type, damage[, who]) -> number
int l_receive_damage(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* type = luaL_checkstring(L, 2);
    int32_t damage   = (int32_t)luaL_checkinteger(L, 3);
    CChar* who       = NULL;
    if (lua_isuserdata(L, 4)) who = dynamic_cast<CChar*>(get_object(L, 4));
    lua_pushinteger(L, ch->receive_damage(type, damage, who));
    return 1;
}

// self:receive_wound(type, damage[, who]) -> number
int l_receive_wound(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* type = luaL_checkstring(L, 2);
    int32_t damage   = (int32_t)luaL_checkinteger(L, 3);
    CChar* who       = NULL;
    if (lua_isuserdata(L, 4)) who = dynamic_cast<CChar*>(get_object(L, 4));
    lua_pushinteger(L, ch->receive_wound(type, damage, who));
    return 1;
}

// self:receive_poison(type, damage, condi) -> number
int l_receive_poison(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* type  = luaL_checkstring(L, 2);
    int32_t damage    = (int32_t)luaL_checkinteger(L, 3);
    const char* condi = luaL_optstring(L, 4, "");
    lua_pushinteger(L, ch->receive_poison(type, damage, condi));
    return 1;
}

// self:remove_powerup(silence[, param2]) -> 消除激励（CSkill::remove_powerup）
int l_remove_powerup(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;
    int32_t silence = (int32_t)luaL_optinteger(L, 2, 0);
    int32_t param2  = (int32_t)luaL_optinteger(L, 3, 0);
    CSkill::remove_powerup(c, silence, param2);
    return 0;
}

// self:start_powerup(me, duration, attack, dodge, msg)
// 调用 CSkill::start_powerup（开始激励：临时提升 attack/dodge，duration
// 后自动到期移除）
int l_start_powerup(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CSkill* skill = dynamic_cast<CSkill*>(obj);
    if (!skill) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (!me) return 0;
    int duration    = (int)luaL_optinteger(L, 3, 0);
    int attack      = (int)luaL_optinteger(L, 4, 0);
    int dodge       = (int)luaL_optinteger(L, 5, 0);
    const char* msg = lua_isstring(L, 6) ? luaL_checkstring(L, 6) : "";
    skill->CSkill::start_powerup(me, duration, attack, dodge, msg);
    return 0;
}

// self:start_busy(n)
int l_start_busy(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    int n = (int)luaL_checkinteger(L, 2);
    ch->start_busy(n);
    return 0;
}

// self:kill_ob(target)
int l_kill_ob(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar* target = NULL;
    if (lua_isuserdata(L, 2)) target = dynamic_cast<CChar*>(get_object(L, 2));
    if (target) ch->kill_ob(target);
    return 0;
}

// self:set_killer(target)
int l_set_killer(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar* target = NULL;
    if (lua_isuserdata(L, 2)) target = dynamic_cast<CChar*>(get_object(L, 2));
    if (target) ch->set_killer(target);
    return 0;
}

// self:fight_ob(target)
int l_fight_ob(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar* target = NULL;
    if (lua_isuserdata(L, 2)) target = dynamic_cast<CChar*>(get_object(L, 2));
    if (target) ch->fight_ob(target);
    return 0;
}

// self:remove_all_killer() - 清除所有杀手列表（CChar）
int l_remove_all_killer(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    ch->remove_all_killer();
    return 0;
}
