// test_lua_unit.cpp - Lua 脚本单测桥（LuaUnit 经引擎状态执行）
//
// 构建和运行: .\run-unit-tests.ps1
// 聚焦调试:   编译后从仓库根 x64\Release\tests.exe
// --gtest_filter=LuaUnitBridge.*
//
// 发现 tests/unit/lua-tests 下的 test_*.lua，经 lib/lua_unit_runner.lua
// 在引擎状态内逐文件装载执行（与生产同一 GBK require 口径），失败/装载
// 错误逐条 ADD_FAILURE。执行器的详细契约见该文件头注释。

#include "pch.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <lua.hpp>

#include "test_lua_env.h"

namespace {
const char* kRoot = "tests/unit/lua-tests";

// 发现 test_*.lua → 模块名（相对 kRoot：去 .lua、'/' → '.'）
std::vector<std::string> discover_test_modules()
{
    std::vector<std::string> mods;
    const std::filesystem::path root(kRoot);
    for (auto it = std::filesystem::recursive_directory_iterator(root);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        const std::string name = it->path().filename().string();
        if (name.rfind("test_", 0) != 0) continue;  // 前缀 test_
        if (it->path().extension() != ".lua") continue;
        std::string rel = it->path().lexically_relative(root).generic_string();
        rel.resize(rel.size() - 4);  // 去 .lua
        for (char& c : rel)
            if (c == '/') c = '.';
        mods.push_back(rel);
    }
    std::sort(mods.begin(), mods.end());
    return mods;
}

// 读字符串数组字段（table 在栈顶，读完弹掉），逐项回调
template <typename Fn>
void read_string_array(lua_State* L, const char* field, Fn&& on_item)
{
    lua_getfield(L, -1, field);
    const int n = static_cast<int>(lua_rawlen(L, -1));
    for (int i = 1; i <= n; ++i) {
        lua_geti(L, -1, i);
        const char* s = lua_tostring(L, -1);
        on_item(s ? s : "<非字符串结果>");
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}
}  // namespace

class LuaUnitBridge : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

TEST_F(LuaUnitBridge, AllScriptTestsPass)
{
    lua_State* L        = ensure_lua_env();
    const int stack_top = lua_gettop(L);

    const std::vector<std::string> mods = discover_test_modules();
    ASSERT_FALSE(mods.empty()) << "未发现 " << kRoot << "/test_*.lua";

    // require 执行器（其内部扩展 package.path 并装载 luaunit）
    lua_getglobal(L, "require");
    lua_pushstring(L, "lib.lua_unit_runner");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        ADD_FAILURE() << "装载 lua_unit_runner 失败: " << lua_tostring(L, -1);
        lua_settop(L, stack_top);
        return;
    }

    lua_getfield(L, -1, "run");
    if (!lua_isfunction(L, -1)) {
        ADD_FAILURE() << "lua_unit_runner 缺少 run()";
        lua_settop(L, stack_top);
        return;
    }

    // run(modules) 入参：模块名数组
    lua_createtable(L, static_cast<int>(mods.size()), 0);
    for (size_t i = 0; i < mods.size(); ++i) {
        lua_pushstring(L, mods[i].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        ADD_FAILURE() << "执行 Lua 单测失败: " << lua_tostring(L, -1);
        lua_settop(L, stack_top);
        return;
    }
    // 栈：执行器表 + 报告表

    int total = 0;
    lua_getfield(L, -1, "total");
    total = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    int failed = 0;
    read_string_array(L, "failures", [&failed](const std::string& msg) {
        ADD_FAILURE() << msg;
        ++failed;
    });
    read_string_array(L, "errors", [&failed](const std::string& msg) {
        ADD_FAILURE() << msg;
        ++failed;
    });

    lua_pop(L, 2);  // 报告表 + 执行器表
    EXPECT_EQ(stack_top, lua_gettop(L)) << "执行器改变了引擎栈";

    EXPECT_GT(total, 0) << "没有执行任何 Lua 用例";
    EXPECT_EQ(0, failed) << failed << " 个 Lua 用例/文件失败";
}
