// test_lua_env.h — 测试共用：Lua 环境初始化（配置 + 引擎 + 绑定 + 钩子表）
//
// 用例需要真实 Lua 环境时，在 fixture 的 SetUp 里调用 ensure_lua_env()：
// 读配置、起引擎、注册 C++ 绑定、按根装载 hooks/commands 钩子表。
// 每次进入用例都重跑一遍，幂等：
//   - ini_load 重复执行会清空并重读配置，复位被上一个用例改过的 g_ini_data；
//   - 引擎只在没有 state 时初始化，绑定随 state 创建注册一次；
//   - 钩子表按根重新装载，把改写过 hooks/commands 的用例状态复位。
// 配置与脚本路径相对仓库根（测试必须从仓库根运行，见 run-unit-tests.ps1）。
#pragma once

#include "gtest/gtest.h"

#include "IniParser.h"
#include "lua/LuaBind.h"
#include "lua/LuaEngine.h"
#include "lua/LuaHook.h"

#include <filesystem>
#include <fstream>
#include <string>

inline lua_State* ensure_lua_env_impl()
{
    ini_load("tests/unit/test_config.ini");

    CLuaEngine* engine = CLuaEngine::instance();
    if (!engine->get_state()) {
        const char* root
            = ini_get("lua", "script_root", "tests/unit/lua-tests");
        const char* def = ini_get("lua", "default_script_root", "lua-default");
        EXPECT_TRUE(engine->init(root, def))
            << "Lua 引擎初始化失败 script_root=" << root
            << " default_script_root=" << def;
        if (!engine->get_state()) return nullptr;
        lua_bind_init(engine->get_state());
    }
    EXPECT_TRUE(lua_hook_boot(engine->get_state()));
    return engine->get_state();
}

// 测试自身的模块目录（lib/lua_unit_runner、lib/lua_coverage）必须始终可
// require。script_root 随配置组变化——sample 组的根是 lua-sample，里面没有
// lib/——所以不能靠引擎按 script_root 捎带，这里显式补进 package.path。
inline void ensure_test_module_path(lua_State* L)
{
    static const char* kEntry = "tests/unit/lua-tests/?.lua";
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char* cur  = lua_tostring(L, -1);
    std::string path = cur ? cur : "";
    lua_pop(L, 1);
    if (path.find(kEntry) == std::string::npos) {
        lua_pushstring(L, (path + ";" + kEntry).c_str());
        lua_setfield(L, -2, "path");
    }
    lua_pop(L, 1);
}

// 读配置 + 起引擎（幂等）+ 注册绑定 + 补测试模块路径 + 装载钩子表
inline lua_State* ensure_lua_env()
{
    lua_State* L = ensure_lua_env_impl();
    if (L) ensure_test_module_path(L);
    return L;
}

// 调用全局 Lua 函数（前导组合出的 API）：入参（不含函数）已压栈，
// 返回值个数（-1 = 调用失败，错误信息已弹出）
inline int call_global(lua_State* L, const char* name, int nargs)
{
    int base = lua_gettop(L) - nargs;
    lua_getglobal(L, name);
    lua_insert(L, base + 1);
    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK) {
        lua_pop(L, 1);
        return -1;
    }
    return lua_gettop(L) - base;
}

// 求值 Lua 表达式并取回整数结果（失败记 ADD_FAILURE 并回 0）。
// 用于内容层前导定义的全局函数与引擎侧同式函数的对拍。
inline int64_t lua_int_expr(lua_State* L, const char* expr)
{
    const std::string code = std::string("return ") + expr;
    if (luaL_dostring(L, code.c_str()) != LUA_OK) {
        ADD_FAILURE() << expr << ": " << lua_tostring(L, -1);
        lua_pop(L, 1);
        return 0;
    }
    int64_t v = (int64_t)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}

// 求值 Lua 表达式并取回字符串结果（非字符串回空串，失败记 ADD_FAILURE）
inline std::string lua_str_expr(lua_State* L, const char* expr)
{
    const std::string code = std::string("return ") + expr;
    if (luaL_dostring(L, code.c_str()) != LUA_OK) {
        ADD_FAILURE() << expr << ": " << lua_tostring(L, -1);
        lua_pop(L, 1);
        return std::string();
    }
    std::string s;
    if (lua_isstring(L, -1)) s = lua_tostring(L, -1);
    lua_pop(L, 1);
    return s;
}

// 调用对象方法（经 __index 解析，含前导定义的方法）：self + 入参已压栈，
// 返回值个数（-1 = 调用失败，错误信息已弹出）
inline int call_method(lua_State* L, const char* name, int nargs)
{
    int base = lua_gettop(L) - nargs;  // self 所在槽（1-based base+1）
    lua_getfield(L, base + 1, name);
    lua_insert(L, base + 1);
    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK) {
        lua_pop(L, 1);
        return -1;
    }
    return lua_gettop(L) - base;
}

// 换装 login.lua：写入 → check_player 经 load_script 按 hash 自动重载读到
// 新文件 → 收尾还原原文件（或删除），不污染脚本目录
class ScopedLoginScript {
public:
    explicit ScopedLoginScript(const char* content)
    {
        const std::string root = ini_get("lua", "script_root", "");
        m_file = std::filesystem::path(root) / "data" / "login.lua";
        std::filesystem::create_directories(m_file.parent_path());
        m_had_orig = std::filesystem::exists(m_file);
        if (m_had_orig)
            std::filesystem::copy_file(
                m_file, std::filesystem::path(m_file.string() + ".bak"),
                std::filesystem::copy_options::overwrite_existing);
        std::ofstream out(m_file);
        out << content;
    }

    ~ScopedLoginScript()
    {
        std::error_code ec;
        if (m_had_orig) {
            std::filesystem::rename(
                std::filesystem::path(m_file.string() + ".bak"), m_file, ec);
        }
        else {
            std::filesystem::remove(m_file, ec);
        }
    }

    ScopedLoginScript(const ScopedLoginScript&)            = delete;
    ScopedLoginScript& operator=(const ScopedLoginScript&) = delete;

private:
    std::filesystem::path m_file;
    bool m_had_orig = false;
};
