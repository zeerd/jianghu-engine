// test_lua_env.h — 测试共用：Lua 环境初始化（配置 + 引擎 + 绑定 + 钩子表）
//
// 用例需要真实 Lua 环境时，在 fixture 的 SetUp 里调用 ensure_lua_env()：
// 读配置、起引擎、注册 C++ 绑定、按根装载 hooks/commands 钩子表。
// 每次进入用例都重跑一遍，幂等：
//   - ini_load 重复执行会清空并重读配置，复位被上一个用例改过的 g_ini_data；
//   - 引擎只在没有 state 时初始化，绑定随 state 创建注册一次；
//   - 钩子表按根重新装载，把改写过 hooks/commands 的用例状态复位。
// 配置与脚本路径相对仓库根（测试必须从仓库根运行，见 run-coverage.ps1）。
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

// 读配置 + 起引擎（幂等）+ 注册绑定 + 装载钩子表
inline lua_State* ensure_lua_env() { return ensure_lua_env_impl(); }

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
