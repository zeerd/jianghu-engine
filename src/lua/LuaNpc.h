/**
 * @file LuaNpc.h
 * @brief Lua 脚本定义的 NPC 模板类，用模板支持不同基类（CNpc 等）。
 */

//---------------------------------------------------------

#pragma once

#include "Npc.h"
#include "BankOwner.h"
#include "Vendor.h"

#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#include "LuaInstanceRegistry.h"

// 检测 T 是否含有 4 参数 handle_action(char*, CChar*, char*, char*)
// （CVendor/CBankOwner 只重写 3 参版本，会用名字隐藏掉 CChar 的 4 参版本，
//   用 4 参调用表达式做 SFINAE，避免成员指针 cast 的硬错误）
template <typename T, typename = void>
struct npc_has_handle_action4 : std::false_type {};

template <typename T>
struct npc_has_handle_action4<
    T, decltype(std::declval<T&>().handle_action(
                    std::declval<char*>(), std::declval<CChar*>(),
                    std::declval<char*>(), std::declval<char*>()),
                void())> : std::true_type {};

// 模板基类：支持不同 C++ 基类（CNpc 等）
// 通过脚本的 base 字段选择具体基类：
//   base 缺省/="npc"   -> CLuaNpc   （T=CNpc，普通 NPC）
template <typename T>
class CLuaNpcT : public T {
public:
    CLuaNpcT(const char* scriptName);
    virtual ~CLuaNpcT();

    // 重写 create()：调用 Lua 脚本的 create 函数
    virtual void create();

    // 重写 setup()：C++ 框架在 create() 之后调用，转发到 Lua 脚本的 setup 函数
    virtual void setup();

    // 刷新脚本引用并调用 Lua reload(self) 回调（热重载用）
    void reload_script();

    // 供 Lua 脚本调用基类的 create()
    // 由于本模板的 create() 只调脚本 create，不调用
    // T::create()， 需要基类通用初始化逻辑的脚本可在其 create 中主动调用
    // self:base_create()
    void base_create() { T::create(); }
    // 供 Lua 脚本显式调用基类 setup()
    // self:base_setup()
    void base_setup() { T::setup(); }
    // 供 Lua 脚本显式调用基类 init()（自动战斗/仇恨等逻辑）
    // self:base_init(me)
    void base_init(CChar* me) { T::init(me); }
    // 供 Lua 脚本显式调用基类 3 参 handle_action
    // self:base_handle_action(comm, me, arg)
    int base_handle_action(char* comm, CChar* me, char* arg)
    {
        return T::handle_action(comm, me, arg);
    }
    // 供 Lua 脚本显式调用基类 4 参 handle_action
    // self:base_handle_action4(comm, me, arg, tar)
    int base_handle_action4(char* comm, CChar* me, char* arg, char* tar)
    {
        return call_base_handle_action4(npc_has_handle_action4<T>(), comm, me,
                                        arg, tar);
    }
    // 供 Lua 脚本显式调用基类 do_talk（含 通用对话钩子 / Vendor/BankOwner
    // 回退 + DoLuaInquiry 兜底，与脚本未定义 do_talk 时的行为一致）
    // 通用对话钩子（hooks.pre_talk）决定脚本侧是否接管本次对话，
    // C++ 不硬编码任何具体任务 key。
    // self:base_do_talk(me, ask)
    int base_do_talk(CChar* me, char* ask);

    // 重写 create_family 桥接：脚本通过 set_inquiry 注册询问
    // 预置回答版：set_inquiry(ask, answer, topic)
    void set_inquiry(const char* ask, const char* answer,
                     const char* topic = NULL);
    // 回调版：set_inquiry(ask, funcName, topic)，funcName 为脚本 table
    // 中的函数名
    void set_inquiry_func(const char* ask, const char* funcName,
                          const char* topic = NULL);

    // 重写 do_talk：调用 Lua 脚本的 do_talk 函数（若存在）
    virtual int do_talk(CChar* me, char* ask = NULL);

    // 重写 handle_action：调用 Lua 脚本的 handle_action 函数（若存在）
    virtual int handle_action(char* comm, CChar* me, char* arg);
    virtual int handle_action(char* comm, CChar* me, char* arg, char* tar);

    // 4 参 handle_action 的基类回退：T 有 4 参版本则调用之，否则退到 3 参版本
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

    // 重写 accept_object：调用 Lua 脚本的 accept_object 函数（若存在）
    virtual int accept_object(CChar* me, CContainer* item);

    // 重写 init：调用 Lua 脚本的 init 函数（若存在）
    virtual void init(CChar* me);

    // 重写 die：调用 Lua 脚本的 die 函数（若存在）
    virtual void die();

    // 重写 finish_kill：调用 Lua 脚本的 finish_kill
    // 函数（若存在，如巡捕击杀玩家）
    virtual void finish_kill(CChar* me);

    // 重写 is_defeated：调用 Lua 脚本的 is_defeated
    // 函数（若存在，如比武大会登记得胜者）
    virtual void is_defeated(CChar* winner, CChar* failer);

    // 重写 do_look：调用 Lua 脚本的 do_look 函数（若存在）
    virtual int do_look(CChar* me);

    // 重写 hit_ob：调用 Lua 脚本的 hit_ob 函数（若存在）
    virtual const char* hit_ob(CChar* me, CChar* victim, int32_t damage_bonus);

    // 重写 chat_msg_combat：调用 Lua 脚本的 chat_msg_combat 函数（若存在）
    virtual char* chat_msg_combat();

    // 重写 chat_msg：调用 Lua 脚本的 chat_msg 函数（若存在）
    virtual char* chat_msg();

    // 重写 greeting：调用 Lua 脚本的 greeting 函数（若存在）
    virtual char* greeting(CChar* me);

    // 重写 attempt_apprentice：调用 Lua 脚本的 attempt_apprentice
    // 函数（若存在）
    virtual void attempt_apprentice(CChar* me);

    // 重写 prevent_learn：调用 Lua 脚本的 prevent_learn 函数（若存在）
    virtual int prevent_learn(CChar* me, const char* skill);

    // 重写 recognize_apprentice：调用 Lua 脚本的 recognize_apprentice
    // 函数（若存在）
    virtual int recognize_apprentice(CChar* me);

    // 重写 accept_fight：调用 Lua 脚本的 accept_fight 函数（若存在）
    virtual int accept_fight(CChar* me);

    // 重写 attack：调用 Lua 脚本的 attack 函数（若存在）
    virtual void attack(CChar* opponent);

    // 说话/表情转发回调
    virtual void relay_say(CChar* me, char* msg);
    virtual void relay_emote(CChar* me, std::string verb);

    // 脚本名
    const char* get_script_name() { return m_script_name.c_str(); }

    // 获取脚本引用
    int get_script_ref() { return m_script_ref; }

private:
    struct LuaInquiry_t {
        std::string topic;
        std::string answer;    // 预置回答
        std::string funcName;  // 回调函数名（脚本 table 中的函数）
        bool isFunc = false;   // true=回调, false=预置回答
    };
    std::map<std::string, LuaInquiry_t> m_list_lua_inquiry;

    // 脚本无 do_talk 时，处理 set_inquiry 注册的询问逻辑
    int do_lua_inquiry(CChar* me, char* ask);

    std::string m_script_name;
    int m_script_ref;  // 脚本 table 的 registry 引用
};

// 类型别名：根据脚本 base 字段选择
typedef CLuaNpcT<CNpc> CLuaNpc;  // base 缺省/="npc"：普通 NPC
typedef CLuaNpcT<CVendor>
    CLuaVendor;  // type="vendor"：商家NPC（CVendor 基类，支持买卖菜单）
typedef CLuaNpcT<CBankOwner>
    CLuaBankOwner;  // type="bankowner"：钱庄老板NPC（CBankOwner
                    // 基类，支持存取款）

#include "Encoding.h"
#include "LuaNpc.inl"
