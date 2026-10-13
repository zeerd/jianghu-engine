/**
 * @file SkillMgr.h
 * @brief 技能/状态加载与管理。
 */
#pragma once

#include <string>

class CSkill;
class CCondition;

using std::string;

// --- 加载 ---
CSkill* load_skill(const char* base_name);
inline CSkill* load_skill(const std::string& base_name)
{
    return load_skill(base_name.c_str());
}
CCondition* load_condition(const char* base_name);

// --- 释放（内部） ---
void skill_release();
void condition_release();
