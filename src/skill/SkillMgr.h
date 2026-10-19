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

// 从技能缓存摘除一个技能，并连带摘除继承自它的技能。
// 继承项的归属指针指向基础技能：基础技能被销毁后，继承者若仍留在缓存里，
// 其继承项的归属指针就会悬垂（施法转发时会用到）。连带摘除后，下次
// load_skill 会重建它们并重新继承。
void skill_release_one(const char* base_name);
void condition_release();
