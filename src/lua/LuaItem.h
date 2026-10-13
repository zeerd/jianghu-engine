/**
 * @file LuaItem.h
 * @brief Lua 脚本定义的 Item 模板类，用模板支持不同基类（CItem/CEquip）。
 */

//---------------------------------------------------------

#pragma once

#include "Item.h"
#include "Equip.h"

#include <type_traits>
#include <vector>
#include "LuaInstanceRegistry.h"

// 编译期特性检测：T 是否含有各可选基类方法
// （这些钩子并非对所有基类都存在，需要用标签分派避免在
//  不含该方法的基类 T 上实例化非法调用）

// 检测 T 是否含有 4 参数 handle_action(char*, CChar*, char*, char*)
// （用 4 参调用表达式做 SFINAE，避免成员指针 cast 的硬错误）
template <typename T, typename = void>
struct has_handle_action4 : std::false_type {};

template <typename T>
struct has_handle_action4<
    T, decltype(std::declval<T&>().handle_action(
                    std::declval<char*>(), std::declval<CChar*>(),
                    std::declval<char*>(), std::declval<char*>()),
                void())> : std::true_type {};

// 检测 T 是否含有 owner_is_killed(CChar*)
// CContainer 定义了 virtual owner_is_killed，所有子类都继承
// 用 tag dispatch 处理：CLuaItemT 总是定义 owner_is_killed，
// 其中 fallback 调用 T::owner_is_killed（对 CContainer 为空操作）
template <typename T>
struct has_owner_is_killed : std::true_type {};

// 检测 T 是否含有 Do_Wield()/Do_UnWield()（CEquip 的 protected 方法，
// 仅 CEquip 及其派生类拥有，用继承关系判断）
template <typename T>
struct has_do_wield : std::is_base_of<CEquip, T> {};

template <typename T>
class CLuaItemT : public T {
public:
    CLuaItemT(const char* scriptName);
    virtual ~CLuaItemT();

    // 重写 create()：调用 Lua 脚本的 create 函数
    virtual void create(int flag = 0);

    // 刷新脚本引用并调用 Lua reload(self) 回调（热重载用）
    void reload_script();

    // 重写 do_use：调用 Lua 脚本的 do_use 函数（若存在）
    virtual int do_use(CChar* me, CContainer* target);

    // 重写 do_look：调用 Lua 脚本的 do_look 函数（若存在）
    virtual int do_look(CChar* me);

    // 重写 Can_Use：调用 Lua 脚本的 Can_Use 函数（若存在）
    virtual int can_use();

    // 重写 Can_Equip：调用 Lua 脚本的 Can_Equip 函数（若存在）
    virtual int can_equip(CChar* me);

    // 重写 hit_ob：调用 Lua 脚本的 hit_ob 函数（若存在）
    virtual const char* hit_ob(CChar* me, CChar* victim, int32_t damage_bonus);

    // 重写 query_autoload：调用 Lua 脚本的 query_autoload 函数（若存在）
    virtual int query_autoload();

    // 重写 handle_action：调用 Lua 脚本的 handle_action 函数（若存在）
    virtual int handle_action(char* comm, CChar* me, char* arg);
    virtual int handle_action(char* comm, CChar* me, char* arg, char* tar);

    // 脚本名
    const char* get_script_name() { return m_script_name.c_str(); }

    // 获取脚本引用
    int get_script_ref() { return m_script_ref; }

    // 重写 owner_is_killed：调用 Lua 脚本的 owner_is_killed 函数（若存在）
    virtual void owner_is_killed(CChar* killer);

    // 重写 Do_Action：调用 Lua 脚本的 do_action 函数（若存在）
    virtual int do_action(int act, CChar* me);

    // 重写 OnMoved：调用 Lua 脚本的 on_moved 函数（若存在）
    virtual void on_moved(CContainer* me);

    // 重写 Query：从 Lua 脚本属性读取 CAN_COMBINE、IS_FOOD 等标志
    virtual int32_t query_attr(int attr);

    // 重写 CanCombineInBox：从 Lua 脚本属性读取
    virtual int can_combine_in_box();

    // 重写 init：回退语义——脚本定义 init 时完全接管（需要基类行为时脚本
    // 显式调用 self:base_init()），否则调用基类
    // T::init()（反复制检查/重量同步）
    virtual void init();

    // self:base_init() - 供脚本显式调用 C++ 基类 init（非虚限定调用防递归）
    void base_init() { T::init(); }

    // 供 Lua 调用的公共包装：调用 CEquip 的 protected Do_Wield()/Do_UnWield()
    int lua_do_wield();
    int lua_do_un_wield();

    // 供 Lua 调用的默认动作：调用 CItem::Do_Action（绕过 Lua 重写）
    int lua_do_action_base(int act, CChar* me);

private:
    std::string m_script_name;
    int m_script_ref;  // 脚本 table 的 registry 引用

    // 标签分派：仅当基类 T 确实含有对应方法时才回退到 T::，
    // 否则返回安全默认值，避免在 CItem/CEquip 等
    // 不含该方法的基类上实例化非法调用。
    int call_base_handle_action4(std::true_type, char* comm, CChar* me,
                                 char* arg, char* tar)
    {
        return T::handle_action(comm, me, arg, tar);
    }
    int call_base_handle_action4(std::false_type, char* comm, CChar* me,
                                 char* arg, char* tar)
    {
        return T::handle_action(comm, me, arg);
    }

    void call_base_owner_is_killed(std::true_type, CChar* killer)
    {
        T::owner_is_killed(killer);
    }
    void call_base_owner_is_killed(std::false_type, CChar* killer)
    { /* 基类无此方法，忽略 */ }

    // 标签分派：仅当 T 派生自 CEquip 时才调用 Do_Wield/Do_UnWield，
    // 否则返回安全默认值。
    int call_base_do_wield(std::true_type) { return do_wield(); }
    int call_base_do_wield(std::false_type) { return 0; }
    int call_base_do_unwield(std::true_type) { return do_un_wield(); }
    int call_base_do_unwield(std::false_type) { return 0; }

    // 基类 Do_Action 回退：调用 T 的直接基类实现（对 CEquip 系为
    // CItem::Do_Action）
    int call_base_do_action(int act, CChar* me)
    {
        return T::do_action(act, me);
    }
};

// 类型别名
typedef CLuaItemT<CItem> CLuaItem;
typedef CLuaItemT<CEquip> CLuaEquip;

#include "LuaItem.inl"
