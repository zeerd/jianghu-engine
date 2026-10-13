/**
 * @file Force.h
 * @brief 内功基类，管理复元术与内功反震。
 */

#pragma once

#include "Skill.h"

#define FORCE_BEGIN(item_name)        \
    class item_name : public CForce { \
    public:                           \
        item_name() {};               \
        virtual ~item_name() {};
#define FORCE_END \
    }             \
    ;

/**
 * @brief 内功基类，提供复元术与内功反震逻辑。
 */
class CForce : public CSkill {
public:
    static int do_recover(CChar* me, CContainer* target);

    virtual void create();
    virtual char* hit_ob(CChar* me, CChar* victim, int32_t& damage_bonus);

    CForce();
    virtual ~CForce();
};
