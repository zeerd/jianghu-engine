/**
 * @file CombatRules.cpp
 * @brief 战斗规则询问点的 Lua 派发实现（hooks.combat_rule）。
 */

#include "CombatRules.h"

#include "Char.h"
#include "LuaHook.h"

std::string combat_rule_reason(const char* phase, CChar* actor, CChar* target,
                               const char* detail)
{
    CLuaHookCall hook("combat_rule");
    hook.add_str(phase).add_object(actor).add_object(target).add_str(detail);
    if (!hook.call() || hook.result_is_nil()) return std::string();

    std::string reason;
    hook.table_field_str("reason", reason);
    return reason;
}

bool combat_rule_yes(const char* phase, CChar* actor, CChar* target)
{
    CLuaHookCall hook("combat_rule");
    hook.add_str(phase).add_object(actor).add_object(target).add_str(nullptr);
    if (!hook.call() || hook.result_is_nil()) return false;

    bool yes = false;
    hook.table_field_bool("yes", yes);
    return yes;
}
