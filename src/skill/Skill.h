/**
 * @file Skill.h
 * @brief 技能基类，管理法术/动作/练习/启用。
 */

#pragma once

#include <memory>

#include "Char.h"
#include "Combatd.h"

#define SKILL_USAGE_ATTACK  1
#define SKILL_USAGE_DEFENSE 2

#define LANSKILLMAXNUM 10

/**
 * @brief 技能基类，定义法术注册、动作查询与练习接口。
 */
class CSkill : public CMainObject {
public:
    // 法术函式定义
    typedef int MAGICFUNCTIONP(CChar* me, CContainer* ob);
    // 法术结构
    typedef struct tagMagic {
        string name;     // 中文名
        int mp;          // 需要mp的值
        int ap;          // 需要的ap值
        int animate;     // 动画号
        int valid_cast;  // 施法时机
        int target;      // 施法目标 CAST_TO_OTHER, 对其他人 CAST_TO_ME 对自己
                         // CAST_TO_NONE 无目标 CAST_TO_RANDOM 任意目标
        int hostile;     // 是否属于敌意行为（用于 PK 敌意判定），缺省为敌意

        MAGICFUNCTIONP* func = nullptr;  // 魔法函式

        tagMagic()
        {
            mp         = 10;
            ap         = 100;
            animate    = 0;
            valid_cast = CAST_IN_NONE;
            target     = CAST_TO_RANDOM;
            hostile    = 1;
        }
    } Magic_t;

public:
    static void start_powerup(CChar* me, int duration, int attack, int dodge,
                              const char* msg);
    static void remove_powerup(CContainer* ob, int32_t param1, int32_t param2);
    virtual int valid_force(const char* arg);
    virtual int valid_force_pro(CChar* me);
    virtual int valid_combine(string combo);
    virtual int practice_skill(CChar* me);
    virtual int valid_enable(string usage);
    virtual const char* query_parry_msg(CContainer* weapon);
    virtual const char* query_dodge_msg(CChar* me);
    virtual const char* type();
    virtual int valid_effect(CChar* me, CContainer* weapon, char* action_name,
                             int skill);
    virtual void skill_improved(CChar* me);
    virtual void create();
    virtual void reset(CChar* me);
    virtual char* hit_ob(CChar* me, CChar* victim, int32_t& damage);
    virtual CMapping* query_action(CChar* me);
    void download(CChar* me);
    virtual int32_t query_attr(int attr);
    virtual int cast(string magic, CChar* me, CContainer* target = NULL);
    virtual int valid_learn(CChar* me);

    // 查询已注册法术的属性；未注册法术：hostile 保守按敌意返回 1，target 返回
    // CAST_TO_RANDOM。
    int query_magic_hostile(const char* magic);
    int query_magic_target(const char* magic);

    CSkill();
    virtual ~CSkill();

    enum {
        CAST_IN_NONE = 0,
        CAST_IN_COMMON,
        CAST_IN_COMBAT,
        CAST_IN_BOTH,
    };

    enum {
        CAST_TO_NONE = 0,
        CAST_TO_ME,      // 自己
        CAST_TO_OTHER,   // 对方
        CAST_TO_RANDOM,  // 对任意人

        CAST_TO_FRIEND,      // 对本队队员使用
        CAST_TO_TEAM,        // 对本队全体
        CAST_TO_OTHER_TEAM,  // 对敌方全体
    };

protected:
    Magic_t* add_magic(string magic, MAGICFUNCTIONP* func);

    typedef map<string, std::unique_ptr<Magic_t>, less<string> > CastList_t;
    CastList_t m_magic_list;  // 法术列表
};
