/**
 * @file LuaSkill.h
 * @brief Lua 脚本定义的 Skill 实现类。
 *
 * 技能行为全部由 Lua 脚本提供（法术用 add_magic 注册，回调用同名函数实现）；
 * 本类只做「脚本 → 引擎」的回调分派，以及脚本没实现时的回落。
 */

//////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include "Skill.h"

// Lua 脚本技能：所有技能都是这一种实现（脚本的 base 字段不再选 C++ 基类）
class CLuaSkill : public CSkill {
public:
    CLuaSkill(const char* scriptName);
    virtual ~CLuaSkill();

    // 重写 create()：回退语义——脚本定义 create 时完全接管，否则调用基类
    // CSkill::create()；create 收尾时继承"可 enable
    // 的技能种类"对应基础技能的法术。
    virtual void create();

    // 重写 query_action：调用 Lua 脚本的 query_action 函数（若存在），
    // 否则回落到基础技能（同族默认实现，如基本投掷的出招姿势）
    virtual CMapping* query_action(CChar* me);

    // 本技能是否提供 query_action 实现（脚本里有同名函数）
    virtual bool has_query_action();

    // 重写 valid_learn：调用 Lua 脚本的 valid_learn 函数（若存在）
    virtual int valid_learn(CChar* me);

    // 重写 Cast：支持 Lua 脚本注册的法术
    virtual int cast(string magic, CChar* me, CContainer* target = NULL);

    // 重写 valid_enable
    virtual int valid_enable(string usage);

    // 重写 practice_skill
    virtual int practice_skill(CChar* me);

    // 重写 valid_force
    virtual int valid_force(const char* arg);

    // 重写 valid_force_pro
    virtual int valid_force_pro(CChar* me);

    // 重写 valid_combine
    virtual int valid_combine(string combo);

    // 重写 query_dodge_msg
    virtual const char* query_dodge_msg(CChar* me);

    // 重写 query_parry_msg
    virtual const char* query_parry_msg(CContainer* weapon);

    // 重写 hit_ob：调用 Lua 脚本的 hit_ob 函数（若存在），
    // 否则回落到基础技能（同族默认实现，如基本内功的内功反震）
    virtual char* hit_ob(CChar* me, CChar* victim, int32_t& damage);

    // 本技能是否提供 hit_ob 实现（脚本里有同名函数）
    virtual bool has_hit_ob();

    // 重写 type：调用 Lua 脚本的 type 函数（返回 "knowledge" 等，若存在）
    virtual const char* type();

    // 重写 skill_improved：调用 Lua 脚本的 skill_improved 函数（若存在）
    virtual void skill_improved(CChar* me);

    // 注册 Lua 法术（供 Lua 脚本调用）
    // hostile：是否属于敌意行为，缺省传 1（攻击/震慑类招式）
    // 返回 1 成功，0 失败
    int lua_add_magic(const char* magicName, const char* displayName,
                      int32_t mp, int32_t ap, int target, int validCast,
                      const char* luaFuncName, int hostile);

    // 脚本名
    const char* get_script_name() { return m_script_name.c_str(); }

    // 获取脚本引用
    int get_script_ref() { return m_script_ref; }

    // 设置 base_skill（可 enable 的技能类型）
    void setup_base_skill();

    // 继承"可 enable 的技能种类"对应基础技能的法术
    // （如高级投掷继承基本投掷的"偷袭"）
    void inherit_base_magics();

    // 获取已注册法术数量（含继承自基础技能的）
    int get_magic_count() { return (int)m_magic_list.size(); }

    // 获取已注册法术名（用于验证）
    const char* get_magic_name(int idx);

    // 获取法术的显示名（用于验证编码）
    const char* get_magic_display_name(int idx);

private:
    // 脚本未实现 hit_ob 时回落到基础技能的同名实现
    char* base_hit_ob(CChar* me, CChar* victim, int32_t& damage);

    // 脚本未实现 query_action 时回落到基础技能的同名实现
    CMapping* base_query_action(CChar* me);

    std::string m_script_name;
    int m_script_ref;               // 脚本 table 的 registry 引用
    std::string m_type_cache;       // type() 返回值缓存（保证返回指针生命周期）
    std::string m_parry_msg_cache;  // query_parry_msg()
                                    // 返回值缓存（保证返回指针生命周期）
    std::string
        m_hit_msg_cache;  // hit_ob() 返回消息缓存（保证返回指针生命周期）

    // 本技能可 enable 的技能种类（如 "throwing"）：对应同名的基础技能
    std::vector<std::string> m_base_usages;
};
