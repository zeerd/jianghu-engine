/**
 * @file Throwing.h
 * @brief 投掷技能基类，管理偷袭与投掷武器查询。
 */

#pragma once

#include "Skill.h"

#define THROWING_BEGIN(item_name)        \
    class item_name : public CThrowing { \
    public:                              \
        item_name() {};                  \
        virtual ~item_name() {};
#define THROWING_END \
    }                \
    ;

/**
 * @brief 投掷技能基类，提供偷袭与投掷武器查询。
 */
class CThrowing : public CSkill {
public:
    static int do_sneak(CChar* me, CContainer* target);

    virtual void create();
    virtual CMapping* query_action(CChar* me);

    static CContainer* get_first_throwing_weapon(CChar* me,
                                                 const char* name = NULL);

    CThrowing();
    virtual ~CThrowing();
};
