

/**
 * @file AutoFight.h
 * @brief NPC 自动战斗 AI 与仇恨触发器。
 */
#pragma once

#include <cstdint>
#include <string>

class CChar;
class CContainer;

/**
 * @brief NPC 自动战斗控制器，提供狂暴/仇恨/敌意/主动攻击等触发模式。
 */
class CAutoFight {
public:
    // Start auto-fight with specified aggression type
    static void auto_fight(CChar* me, CChar* obj, std::string type);

    // Individual aggression callbacks (CALLOUTFUNC signature)
    static void start_berserk(CContainer* ob, int32_t target, int32_t param2);
    static void start_hatred(CContainer* ob, int32_t target, int32_t param2);
    static void start_vendetta(CContainer* ob, int32_t target, int32_t param2);
    static void start_aggressive(CContainer* ob, int32_t target,
                                 int32_t param2);
};

extern CAutoFight g_auto_fight;
