/**
 * @file CharRules.h
 * @brief 角色规则询问点：C++ 只申报引擎时机（phase），规则内容与归属全归 Lua
 *        数据集（hooks.char_rule）。新增询问点或数据集规则无需改动本层。
 */

#pragma once

#include <cstdint>

class CChar;

// 数值覆盖型询问：规则返回 {limit=N} 即采用该值；
// 缺席 / nil / 缺字段 = 返回 default_limit（引擎默认基准）。
int64_t char_rule_limit(const char* phase, CChar* actor, int64_t default_limit);

// 命中型询问：规则返回 {yes=true} 即命中；缺席 / nil = false。
bool char_rule_yes(const char* phase, CChar* actor, CChar* target);
