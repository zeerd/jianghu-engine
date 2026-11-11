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

// 数值属性统查，name 取 str/int/con/per/dex/combat_exp：前五项叠加 dbase
// 值、apply/ 装备修正与对应技能加成，combat_exp 为原始历练值。可取键只此
// 一处。未知名说明调用方写错了键：传入 ok 时交回调用方处置，否则记错误
// 日志（C++ 调用点只写常量键，走到这里即缺陷）。
int32_t CChar::attr(const char* name, bool* ok)
{
    if (ok) *ok = true;

    if (strcmp(name, "str") == 0)
        return query("str") + query_temp("apply/str") + query_unarmed() / 10;
    if (strcmp(name, "con") == 0)
        return query("con") + query_temp("apply/con")
               + query_skill("force", 1) / 10;
    if (strcmp(name, "int") == 0)
        return query("int") + query_temp("apply/int")
               + query_skill("literate", 1) / 10;
    if (strcmp(name, "dex") == 0)
        return query("dex") + query_temp("apply/dex")
               + query_skill("dodge", 1) / 10;
    if (strcmp(name, "per") == 0) return query("per") + query_temp("apply/per");
    if (strcmp(name, "combat_exp") == 0) return query("combat_exp");

    if (ok) {
        *ok = false;
        return 0;
    }
    LOG_ERROR("unknown attr: %s", name);
    return 0;
}

// 返回可用经验点
int32_t CChar::query_valid_exp()
{
    int32_t exp      = CChar::level2_exp(query("level"));
    int32_t need_exp = attr("combat_exp") - exp - query("sk_exp");

    return need_exp;
}

unsigned long CChar::query_magic_exp() { return attr("combat_exp"); }

int CChar::query_magic_skill(const char* skill, int raw)
{
    return query_skill(skill, raw);
}

int CChar::query_grant() { return query("grant"); }

int CChar::is_loading() { return query_temp("client/loading"); }

// 检查用户是否拒绝收听该频道信息
int CChar::refuse_channel(const char* channel)
{
    char msg[80];
    return query_temp(snprintf(msg, 80, "refuse/%s", channel));
}

// 学习系数。用于learn的计算
int CChar::learn_bonus()
{
    int32_t pt = query("level") * attr("int") / 50;
    if (pt <= 0) return 1;
    if (pt <= 30) return pt;
    if (pt <= 50) return 30 + (pt - 30) / 2;
    if (pt <= 80) return 45 + (pt - 50) / 3;
    return 55 + (pt - 80) / 5;
}

bool CChar::is_invalid()
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
        if (query("balance") < MAPPING_VALUE_MAX - val) {
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

// self:attr(name) -> number - 数值属性统查（键与口径见 CChar::attr）。
// 属性名写错是脚本缺陷，抛 Lua 错误；非角色对象无属性，返回空。
int l_attr(lua_State* L)
{
    const char* name = luaL_checkstring(L, 2);

    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    bool ok;
    int32_t v = ch->attr(name, &ok);
    if (!ok) return luaL_error(L, "unknown attr: %s", name);

    lua_pushinteger(L, v);
    return 1;
}

// self:is_invalid() -> boolean - 是否为异常人物（CChar）；非角色对象回 false
int l_is_invalid(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    CChar* ch        = obj ? dynamic_cast<CChar*>(obj) : NULL;

    lua_pushboolean(L, ch ? ch->is_invalid() : false);
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

// respect(kind, ob) -> string - 称谓族合一：kind 取 respect/rude/self/
// self_rude（对应 Rank.cpp 的四种称谓）；非角色入参返回空串
int l_respect(lua_State* L)
{
    const char* kind = luaL_checkstring(L, 1);
    if (strcmp(kind, "respect") != 0 && strcmp(kind, "rude") != 0
        && strcmp(kind, "self") != 0 && strcmp(kind, "self_rude") != 0)
        return luaL_error(L, "unknown respect kind: %s", kind);

    CChar* ch = NULL;
    if (lua_isuserdata(L, 2)) ch = dynamic_cast<CChar*>(get_object(L, 2));
    if (!ch) {
        lua_pushstring(L, "");
        return 1;
    }

    const char* s = query_respect(ch);
    if (strcmp(kind, "rude") == 0)
        s = query_rude(ch);
    else if (strcmp(kind, "self") == 0)
        s = query_self(ch);
    else if (strcmp(kind, "self_rude") == 0)
        s = query_self_rude(ch);
    lua_pushstring(L, s ? s : "");
    return 1;
}
