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

        string lua_func;          // Lua 法术函数名（法术一律由 Lua 脚本实现）
        CSkill* owner = nullptr;  // 法术归属技能：本技能注册时为自身；
                                  // 继承自基础技能时指向基础技能

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
    // 施法内功前置：已准备内功匹配 / 脚本扩展 / 引擎配置 / 数据集通用规则。
    // 返回非 0 表示满足；0 表示不满足（调用方据此拒绝施法）。
    int check_cast_force(CChar* me);
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
    // 本技能是否提供 hit_ob 实现（Lua 技能看脚本里有没有同名函数）
    virtual bool has_hit_ob() { return false; }
    // 本技能是否提供 query_action 实现（Lua 技能看脚本里有没有同名函数）
    virtual bool has_query_action() { return false; }
    virtual CMapping* query_action(CChar* me);
    void download(CChar* me);
    virtual int32_t query_attr(int attr);
    virtual int cast(string magic, CChar* me, CContainer* target = NULL);
    virtual int valid_learn(CChar* me);

    // 查询已注册法术的属性；未注册法术：hostile 保守按敌意返回 1，target 返回
    // CAST_TO_RANDOM。
    int query_magic_hostile(const char* magic);
    int query_magic_target(const char* magic);

    // 本技能（含继承自基础技能的）是否提供该法术
    bool has_magic(const char* magic);
    // 法术归属技能：本地注册返回自身；继承自基础技能时返回基础技能；
    // 未注册返回 NULL。
    CSkill* magic_owner(const char* magic);
    // 本技能是否继承了 basic 的法术（继承项的归属指向 basic）
    bool depends_on(const CSkill* basic) const;

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
    // 查找已注册法术（含继承自基础技能的）；未注册返回 NULL
    const Magic_t* find_magic(const char* magic) const;

    // 继承基础技能的法术：同名不覆盖（本技能自己注册的优先）。
    // 继承项的 owner 指向基础技能，施法时转发给归属技能执行。
    void inherit_magics(const CSkill* basic);

    typedef map<string, std::unique_ptr<Magic_t>, less<string> > CastList_t;
    CastList_t m_magic_list;  // 法术列表
};
