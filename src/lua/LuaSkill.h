/**
 * @file LuaSkill.h
 * @brief Lua 脚本定义的 Skill 模板类，支持 CSkill/CForce/CThrowing 基类。
 */

////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include "Force.h"
#include "Throwing.h"
#include "Skill.h"

// 模板基类：支持不同 C++ 基类（CSkill/CForce/CThrowing）
template <typename T>
class CLuaSkillT : public T {
public:
    CLuaSkillT(const char* scriptName);
    virtual ~CLuaSkillT();

    // 重写 create()：回退语义——脚本定义 create 时完全接管（需要基类副作用
    // 时脚本显式调用 self:base_create()，如 force 注册复元术、throwing 注册
    // 偷袭），否则调用基类 T::create()
    virtual void create();

    // self:base_create() - 供脚本显式调用 C++ 基类 create（非虚限定调用防递归）
    void base_create() { T::create(); }

    // 重写 query_action：调用 Lua 脚本的 query_action 函数（若存在），
    // 否则回退到基类 T::query_action
    virtual CMapping* query_action(CChar* me);

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

    // 重写 hit_ob
    virtual char* hit_ob(CChar* me, CChar* victim, int32_t& damage);

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

    // 获取已注册的 Lua 法术数量
    int get_magic_count() { return (int)m_lua_magic_list.size(); }

    // 获取已注册的 Lua 法术名（用于验证）
    const char* get_magic_name(int idx);

    // 获取法术的显示名（用于验证编码）
    const char* get_magic_display_name(int idx);

private:
    std::string m_script_name;
    int m_script_ref;               // 脚本 table 的 registry 引用
    std::string m_type_cache;       // type() 返回值缓存（保证返回指针生命周期）
    std::string m_parry_msg_cache;  // query_parry_msg()
                                    // 返回值缓存（保证返回指针生命周期）
    std::string
        m_hit_msg_cache;  // hit_ob() 返回消息缓存（保证返回指针生命周期）

    // Lua 法术表：法术名 -> Lua 函数名
    std::map<std::string, std::string> m_lua_magic_list;
};

// 类型别名
typedef CLuaSkillT<CSkill> CLuaSkill;        // base = "skill"（普通技能，默认）
typedef CLuaSkillT<CForce> CLuaForce;        // base = "force"（内功）
typedef CLuaSkillT<CThrowing> CLuaThrowing;  // base = "throwing"（投掷）

#include "Encoding.h"
#include "LuaSkill.inl"
