/**
 * @file CombatRules.h
 * @brief 战斗规则询问点：C++ 只申报引擎时机（phase），规则内容与归属全归 Lua
 *        数据集（hooks.combat_rule）。新增询问点或数据集规则无需改动本层。
 */

#pragma once

#include <string>

class CChar;

// 否决型询问：任一规则返回 {reason=文案} 即拒绝，取首个理由；
// 空串 = 无规则反对。
// detail 为该时机的补充上下文（如被拦截的命令名），无则传 NULL（脚本收到
// nil）。
std::string combat_rule_reason(const char* phase, CChar* actor, CChar* target,
                               const char* detail = nullptr);

// 命中型询问：任一规则返回 {yes=true} 即命中；缺席/nil = false。
bool combat_rule_yes(const char* phase, CChar* actor, CChar* target);
