

/**
 * @file CombatCalc.h
 * @brief 战斗力计算（技能攻击力/防御力）。
 */
#pragma once

#include <cstdint>

class CChar;

/**
 * @brief 战斗力计算器，提供基于技能的攻击/防御力计算。
 */
class CCombatCalc {
public:
    // Calculate skill-based attack or defense power
    static double skill_power(CChar* ob, const char* skill, int usage);
};
