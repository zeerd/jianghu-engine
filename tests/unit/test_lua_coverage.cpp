// test_lua_coverage.cpp - Lua 语料覆盖率的采集窗口（全局测试环境）
//
// 构建和运行: .\run-unit-tests.ps1
//
// 窗口横跨整个测试进程：语料的首次执行常落在 Lua 单测之外（C++ 测试经
// load_script / require 加载同一批 lua-default 脚本），且共享 lua_State 的
// package.loaded 与脚本缓存跨用例存活——只在 Lua 单测期间开窗，记录到的
// 只是「已无人执行」的空档。窗口语义与明细格式见
// tests/unit/lua-tests/lib/lua_coverage.lua。
//
// 收窗后把行级明细写到 JIANGHU_TEST_TMP/lua-coverage.csv；该目录在分片里
// 各自私有，由 run-unit-tests.ps1 在分片工作目录删除前跨片合并。

#include "pch.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <lua.hpp>

#include "test_lua_env.h"
#include "test_tmp_dir.h"

namespace {
// 语料清单：lua-default 下全部 .lua，按 CWD 相对路径给出。
// 路径口径必须与 luacov stats 的 chunkname 一致（引擎以脚本根相对路径作
// chunkname），两侧才能按键对上——CWD 即分片根，此处枚举到的正是分片
// 私有树里的那份拷贝。
std::vector<std::string> discover_corpus_files()
{
    std::vector<std::string> files;
    const std::filesystem::path root("lua-default");
    if (!std::filesystem::is_directory(root)) return files;
    for (auto it = std::filesystem::recursive_directory_iterator(root);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        if (it->path().extension() != ".lua") continue;
        files.push_back(it->path().generic_string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// 压入字符串数组 table（压在栈顶）
void push_string_array(lua_State* L, const std::vector<std::string>& items)
{
    lua_createtable(L, static_cast<int>(items.size()), 0);
    for (size_t i = 0; i < items.size(); ++i) {
        lua_pushstring(L, items[i].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

// require("lib.lua_coverage")，成功时模块表留在栈顶
bool push_coverage_module(lua_State* L)
{
    lua_getglobal(L, "require");
    lua_pushstring(L, "lib.lua_coverage");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        std::fprintf(stderr, "[lua-coverage] require failed: %s\n",
                     lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    if (!lua_istable(L, -1)) {
        std::fprintf(stderr,
                     "[lua-coverage] lib.lua_coverage is not a table\n");
        lua_pop(L, 1);
        return false;
    }
    return true;
}
}  // namespace

// 采集窗口挂在全局测试环境上而非某个用例上：窗口必须先于任何测试开、
// 后于任何测试关，否则中间那些执行永远进不了统计。
class LuaCoverageEnv : public ::testing::Environment {
public:
    void SetUp() override
    {
        // 引擎状态是采集的前提：脚本根与 luacov 的解析路径都由它铺好。
        // 起在这里之后装载的前导/钩子表因此都落在窗口内。
        if (ensure_lua_env() == nullptr) return;
        lua_State* L = CLuaEngine::instance()->get_state();
        if (L == nullptr) return;
        const int top = lua_gettop(L);

        const std::string statsfile
            = std::filesystem::absolute(test_tmp_dir() / "luacov.stats.out")
                  .string();
        const std::string reportfile
            = std::filesystem::absolute(test_tmp_dir() / "lua-coverage.csv")
                  .string();
        // 残留会让 save_stats 累加进本轮，必须先清
        std::error_code ec;
        std::filesystem::remove(statsfile, ec);
        std::filesystem::remove(reportfile, ec);

        if (push_coverage_module(L)) {
            lua_getfield(L, -1, "start");
            if (lua_isfunction(L, -1)) {
                lua_pushstring(L, statsfile.c_str());
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    std::fprintf(stderr, "[lua-coverage] start failed: %s\n",
                                 lua_tostring(L, -1));
                }
            }
            else {
                std::fprintf(stderr,
                             "[lua-coverage] lib.lua_coverage lacks start()\n");
            }
        }
        lua_settop(L, top);
        m_reportfile = reportfile;  // 有落点才有收窗的理由
    }

    void TearDown() override
    {
        if (m_reportfile.empty()) return;
        lua_State* L = CLuaEngine::instance()->get_state();
        if (L == nullptr) return;
        const int top = lua_gettop(L);

        if (push_coverage_module(L)) {
            lua_getfield(L, -1, "finish");
            if (lua_isfunction(L, -1)) {
                push_string_array(L, discover_corpus_files());
                lua_pushstring(L, m_reportfile.c_str());
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    std::fprintf(stderr, "[lua-coverage] finish failed: %s\n",
                                 lua_tostring(L, -1));
                }
            }
            else {
                std::fprintf(
                    stderr, "[lua-coverage] lib.lua_coverage lacks finish()\n");
            }
        }
        lua_settop(L, top);
    }

private:
    std::string m_reportfile;
};

// 静态初始化期注册：早于 main→RUN_ALL_TESTS，故所有用例都在窗口内。
// 本文件由 tests.vcxproj 直接编进可执行文件，不会被链接器当未引用目标丢弃。
static ::testing::Environment* const g_lua_coverage_env
    = ::testing::AddGlobalTestEnvironment(new LuaCoverageEnv);
