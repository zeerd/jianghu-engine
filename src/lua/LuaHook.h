/**
 * @file LuaHook.h
 * @brief C++ 主动调取 Lua 的功能增强/补充机制（全局表 hooks / commands）。
 *
 * 语义：钩子缺席（引擎未初始化 / 表或函数未定义）= 静默合法，
 * 调用方照常执行自身默认逻辑；在场 = 按各挂点契约消费返回值。
 * 脚本由 lua_hook_boot 开机加载（script_root 优先，回退 default_script_root）。
 */

#pragma once

#include <cstdint>
#include <string>

struct lua_State;
class CMainObject;
class CChar;

// 幂等创建全局 hooks/commands 空表（已有内容不清空），并为 commands 表
// 安装按命令名懒加载的 __index 分发器。在 lua_bind_init 末尾调用。
void lua_hook_init_tables(lua_State* L);

// 为全局 commands 表安装命令模块分发器：表内未命中命令名时，
// require("command.<命令名>") 取模块中与文件同名的入口函数，
// 结果缓存于分发器私有表（不写回 commands 本体，脚本直接定义的命令不受影响）。
// 模块不存在/加载失败/入口缺失记日志并返回 nil（分发链照常回落），
// 同一命令名的失败只尝试与记录一次。
// 每次重装都会丢弃既有缓存与失败记录；lua_hook_boot 重建 commands 表后调用。
void lua_commands_install(lua_State* L);

// 清空命令模块的懒加载缓存与失败记录（脚本热重载后调用，
// 使 command/*.lua 的修改立即生效）。不动 commands 表本体。
void lua_commands_flush(lua_State* L);

// 重置 hooks/commands 两表，装 data/constants.lua（数据集图片号常量，缺席
// 静默跳过），再加载 data/hooks.lua（script_root → default_script_root 回退，
// UTF-8 转 GBK 执行）。commands 表与命令分发器由 C++ 构造，不加载脚本文件——
// 命令一律 command/<命令名>.lua 按需 require（见 lua_commands_install）。
// hooks 缺失/失败记 LOG_ERROR（开机校验点），返回 hooks 是否加载成功。
bool lua_hook_boot(lua_State* L);

// Lua 命令分发：调用 commands[cmd](me, target, arg)。
// 返回 -1 = 未定义或执行出错（调用方继续回落链）；
// 否则透传脚本返回值（无返回/非数值按 1 = 已处理）。
int lua_call_command(const char* cmd, CChar* me, int32_t target, char* arg);

// 单次钩子调用的栈守卫（每个实例独立平衡栈，可重入）。
//
// 用法：CLuaHookCall hook("do_look");
//       hook.add_object(target).add_object(observer);
//       if (hook.call()) { ... hook.result_int() ... }
//
// 实参即 Lua 函数直接形参（无隐式 self）。
class CLuaHookCall {
public:
    explicit CLuaHookCall(const char* name);
    ~CLuaHookCall();

    CLuaHookCall(const CLuaHookCall&)            = delete;
    CLuaHookCall& operator=(const CLuaHookCall&) = delete;

    // 钩子函数已就绪（引擎可用且 hooks.<name> 为函数）
    bool valid() const { return m_valid; }

    CLuaHookCall& add_object(CMainObject* obj);
    CLuaHookCall& add_int(int64_t value);
    CLuaHookCall& add_bool(bool value);
    CLuaHookCall& add_str(const char* value);  // NULL 压 nil

    // 构造一个 table 实参：把结构化数据（如商品快照）整体交给脚本。
    // 钩子签名固定为 (…, table, …)，日后新增字段只在此处 add，脚本按需取用，
    // 不必再改 C++ 调用点与脚本形参。
    // 用法：hook.begin_table().table_set_str("name", n).table_set_int("code",
    // c)
    //            .end_table();
    CLuaHookCall& begin_table();
    CLuaHookCall& table_set_str(const char* key, const char* value);
    CLuaHookCall& table_set_int(const char* key, int64_t value);
    CLuaHookCall& end_table();  // table 留在栈上作为实参

    // 执行 pcall；仅当函数存在且正常返回时为 true。
    // 脚本错误记 LOG_ERROR 且不崩，之后结果按缺席（nil）处理。
    bool call();

    // call() 成功后的返回值读取；缺席/未调用时返回零值
    bool result_is_nil() const;
    bool result_bool() const;
    bool result_is_number() const;
    int64_t result_int() const;
    const char* result_str() const;  // 指向 Lua 栈，调用方使用期间有效

    // 返回值为 table 时的字段读取（读取后自动弹出临时值）
    bool table_field_bool(const char* key, bool& out) const;
    bool table_field_int(const char* key, int64_t& out) const;
    bool table_field_str(const char* key, std::string& out) const;

    // 返回值为字符串数组时的读取（index 从 0 起）
    int table_str_count() const;
    bool table_str_at(int index, std::string& out) const;

private:
    lua_State* m_L;
    int m_top0;     // 构造前栈深（析构还原基线）
    bool m_valid;   // 钩子函数已就绪
    bool m_called;  // 已执行 call()
};
