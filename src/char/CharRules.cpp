/**
 * @file CharRules.cpp
 * @brief 角色规则询问点的 Lua 派发实现（hooks.char_rule）。
 */

#include "CharRules.h"

#include "Char.h"
#include "LuaHook.h"

int64_t char_rule_limit(const char* phase, CChar* actor, int64_t default_limit)
{
    CLuaHookCall hook("char_rule");
    hook.add_str(phase).add_object(actor);
    if (!hook.call() || hook.result_is_nil()) return default_limit;

    int64_t limit = default_limit;
    hook.table_field_int("limit", limit);
    return limit;
}

bool char_rule_yes(const char* phase, CChar* actor, CChar* target)
{
    CLuaHookCall hook("char_rule");
    hook.add_str(phase).add_object(actor).add_object(target);
    if (!hook.call() || hook.result_is_nil()) return false;

    bool yes = false;
    hook.table_field_bool("yes", yes);
    return yes;
}
