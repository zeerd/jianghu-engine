/**
 * @file CmdStandard.h
 * @brief 标准玩家命令处理（物品/战斗/师徒/社交等）。
 */
#pragma once

#include "CObject.h"
#include "CmdHandle.h"

// 自由函数声明
void wakeup1(CContainer* ob, int32_t param1, int32_t param2);

// --- test fixture friends ---
class CmdStandardItemTest;
class CmdStandardCombatTest;
class CmdStandardLifeTest;
class CmdStandardMasterTest;

class CCmdStandard {
public:
    CCmdStandard();
    ~CCmdStandard();

    int handlecommand(char* cmd, CChar* me, int32_t target, char* arg);

    friend class CmdStandardItemTest;
    friend class CmdStandardCombatTest;
    friend class CmdStandardLifeTest;
    friend class CmdStandardMasterTest;

private:
    typedef int (*func)(CChar* me, int32_t target, char* arg);
    typedef map<string, func, less<string> > CMDMAP;
    CMDMAP m_standard_cmd;

    // ---- 物品操作 ----
    static int get(CChar* me, int32_t target, char* arg);
    static int use(CChar* me, int32_t target, char* arg);
    static int wield(CChar* me, int32_t target, char* arg);
    static int un_wield(CChar* me, int32_t target, char* arg);
    static int drop(CChar* me, int32_t target, char* arg);
    static int give(CChar* me, int32_t target, char* arg);
    static int do_drop(CChar* me, CContainer* obj, CContainer* env, int32_t x,
                       int32_t y);
    static int do_give(CChar* me, CContainer* obj, CChar* who);

    // ---- 战斗系统 ----
    static int kill(CChar* me, int32_t target, char* arg);
    static int cast(CChar* me, int32_t target, char* arg);
    static int join_battle(CChar* me, int32_t target, char* arg);
    static int guard(CChar* me, int32_t target, char* arg);
    static int fight(CChar* me, int32_t target, char* arg);
    static int follow(CChar* me, int32_t target, char* arg);
    static int halt(CChar* me, int32_t target, char* arg);

    // ---- 师徒/技能 ----
    static int apprentice(CChar* me, int32_t target, char* arg);
    static int recruit(CChar* me, int32_t target, char* arg);
    static int expell(CChar* me, int32_t target, char* arg);
    static int teach(CChar* me, int32_t target, char* arg);
    static int learn(CChar* me, int32_t target, char* arg);
    static int abandon(CChar* me, int32_t target, char* arg);
    static int enforce(CChar* me, int32_t target, char* arg);
    static int enable(CChar* me, int32_t target, char* arg);
    static int prepare(CChar* me, int32_t target, char* arg);
    static int study(CChar* me, int32_t target, char* arg);
    static int practice(CChar* me, int32_t target, char* arg);

    // ---- 生活/社交 ----
    static int Sleep(CChar* me, int32_t target, char* arg);
    static int exercise(CChar* me, int32_t target, char* arg);
    static int say(CChar* me, int32_t target, char* arg);
    static int look(CChar* me, int32_t target, char* arg);
    static int go_door(CChar* me, int32_t target, char* arg);
    static int tell(CChar* me, int32_t target, char* arg);
    static int valid_move(CChar* me);
    static void wakeup2(CContainer* ob, int32_t param1, int32_t param2);
    static int exercising(CChar* me);
    static int halt_exercise(CChar* me, CChar* who, char* how);
};