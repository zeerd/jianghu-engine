/**
 * @file CharAttr.cpp
 * @brief implementation of CChar (CharAttr). */

#include <windows.h>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "CharRules.h"
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

int32_t CChar::query_str()
{
    return query("str") + query_temp("apply/str") + query_unarmed() / 10;
}

int32_t CChar::query_con()
{
    return query("con") + query_temp("apply/con")
           + query_skill("force", 1) / 10;
}

int32_t CChar::query_int()
{
    return query("int") + query_temp("apply/int")
           + query_skill("literate", 1) / 10;
}

int32_t CChar::query_dex()
{
    return query("dex") + query_temp("apply/dex")
           + query_skill("dodge", 1) / 10;
}

// 返回可用经验点
int32_t CChar::query_valid_exp()
{
    int32_t exp      = CChar::level2_exp(query("level"));
    int32_t need_exp = query_exp() - exp - query("sk_exp");

    return need_exp;
}

unsigned long CChar::query_magic_exp() { return query_exp(); }

int CChar::query_magic_skill(const char* skill, int raw)
{
    return query_skill(skill, raw);
}

int32_t CChar::query_combat_exp() { return query("combat_exp"); }

int32_t CChar::query_exp()
{
    int32_t comexp = query("combat_exp");
    return comexp;
}

int CChar::query_per() { return query("per") + query_temp("apply/per"); }

int CChar::query_grant() { return query("grant"); }

int CChar::is_loading() { return query_temp("client/loading"); }

int32_t CChar::query_apply(const char* attr)
{
    char key[80];

    snprintf(key, 80, "apply/%s", attr);
    return query(attr) + query_temp(key);
}

// 检查用户是否拒绝收听该频道信息
int CChar::refuse_channel(const char* channel)
{
    char msg[80];
    return query_temp(snprintf(msg, 80, "refuse/%s", channel));
}

// 学习系数。用于learn的计算
int CChar::learn_bonus()
{
    int32_t pt = query("level") * query_int() / 50;
    if (pt <= 0) return 1;
    if (pt <= 30) return pt;
    if (pt <= 50) return 30 + (pt - 30) / 2;
    if (pt <= 80) return 45 + (pt - 50) / 3;
    return 55 + (pt - 80) / 5;
}

int CChar::is_invalid()
{
    int64_t max_mp = char_rule_limit("max_mp_limit", this, 60000);
    if (query("mp") > max_mp) {
        LOG_ERROR("%s(%s) MP invalid: %ld / %ld。", name(1), id(1), query("mp"),
                  query("max_mp"));
        tell_object(this,
                    "\n$HIY您的内力超过了合理范围，请您及时联络巫师查处原因。");
        return true;
    }

    return false;
}

void CChar::add_balance(int32_t val)
{
    if (val < 0) {
        add("balance", val);
    }
    else {
        if (query("balance") < LONGMAX - val) {
            add("balance", val);
        }
        else {
            CMoney::pay_player(this, val);
        }
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:add_balance(val) - 增加/减少角色的存款
int l_add_balance(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    int32_t val = (int32_t)luaL_checkinteger(L, 2);
    ch->add_balance(val);
    return 0;
}

// self:query_dex() -> number
int l_query_dex(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_dex());
    return 1;
}

// self:query_combat_exp() -> number
int l_query_combat_exp(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_combat_exp());
    return 1;
}

// self:query_apply(attr) -> number
int l_query_apply(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* attr = luaL_checkstring(L, 2);
    lua_pushinteger(L, ch->query_apply(attr));
    return 1;
}

// self:query_str() -> number
int l_query_str(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_str());
    return 1;
}

// self:query_con() -> number
int l_query_con(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_con());
    return 1;
}

// self:query_int() -> number
int l_query_int(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_int());
    return 1;
}

// self:query_per() -> number
int l_query_per(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_per());
    return 1;
}

// self:is_invalid() -> number - 是否为异常人物（CChar）
int l_is_invalid(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    lua_pushinteger(L, ch->is_invalid());
    return 1;
}

// self:learn_bonus() -> int（CChar 学习加成倍率）
int l_learn_bonus(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->learn_bonus());
    return 1;
}

// query_respect(ob) -> string
int l_query_respect(lua_State* L)
{
    CChar* ch = NULL;
    if (lua_isuserdata(L, 1)) ch = dynamic_cast<CChar*>(get_object(L, 1));
    const char* s = (ch ? query_respect(ch) : "");
    lua_pushstring(L, s ? s : "");
    return 1;
}

// query_rude(ob) -> string
int l_query_rude(lua_State* L)
{
    CChar* ch = NULL;
    if (lua_isuserdata(L, 1)) ch = dynamic_cast<CChar*>(get_object(L, 1));
    const char* s = (ch ? query_rude(ch) : "");
    lua_pushstring(L, s ? s : "");
    return 1;
}

// query_self(ob) -> string - 自称（如 在下/小女子）
int l_query_self(lua_State* L)
{
    CChar* ch = NULL;
    if (lua_isuserdata(L, 1)) ch = dynamic_cast<CChar*>(get_object(L, 1));
    const char* s = (ch ? query_self(ch) : "");
    lua_pushstring(L, s ? s : "");
    return 1;
}

// query_self_rude(ob) -> string - 自称（如 老衲/贫道）
int l_query_self_rude(lua_State* L)
{
    CChar* ch = NULL;
    if (lua_isuserdata(L, 1)) ch = dynamic_cast<CChar*>(get_object(L, 1));
    const char* s = (ch ? query_self_rude(ch) : "");
    lua_pushstring(L, s ? s : "");
    return 1;
}

// self:query_exp() -> number
int l_query_exp(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    lua_pushinteger(L, c->query_exp());
    return 1;
}

// self:query_valid_exp() -> number（校验等级是否有效）
int l_query_valid_exp(lua_State* L)
{
    CNpc* n = dynamic_cast<CNpc*>(get_object(L, 1));
    if (!n) return 0;
    lua_pushinteger(L, n->query_valid_exp());
    return 1;
}
