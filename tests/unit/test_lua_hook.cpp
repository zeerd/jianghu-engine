// test_lua_hook.cpp - C++→Lua 钩子机制（LuaHook）单元测试
//
// 契约覆盖：
//   1. 缺席语义：引擎可用但 hooks.<name> 未定义 → call()=false、结果按 nil，
//      不崩不报错（调用方照常走自身默认逻辑）。
//   2. 实参/返回值往返：对象/整数/布尔/字符串压参与标量返回读取。
//   3. table 契约：{damage, text, handled} 字段读取（set_effect/resolve_parry
//   载体）。
//   4. 字符串数组契约：逐元素读取（hooks.do_channel 返回广播行的载体）。
//   5. pcall 保护：脚本 error → call()=false、结果按 nil、不崩。
//   6. 栈平衡：先构造 A 再构造并完成 B 的调用，随后 A
//   调用仍正确，析构后栈深还原。
//   7. boot：重置 hooks/commands 两表并按根回退加载文件；NULL 状态返回 false。
//
#include "pch.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "IniParser.h"
#include "lua/LuaEngine.h"
#include "lua/LuaHook.h"
#include "test_lua_env.h"

class LuaHookTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_L = ensure_lua_env();
        ASSERT_NE(m_L, nullptr);
        // 本文件的测试钩子名逐一注销，不整体复位（不影响其他 fixture
        // 的文件内容）
        ClearTestHooks();
    }

    void TearDown() override { ClearTestHooks(); }

    static void ClearTestHooks()
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        if (!L) return;
        luaL_dostring(L,
                      "hooks.t_absent = nil hooks.t_scalar = nil "
                      "hooks.t_nil = nil hooks.t_true = nil "
                      "hooks.t_table = nil hooks.t_lines = nil "
                      "hooks.t_error = nil hooks.t_first = nil "
                      "hooks.t_second = nil");
    }

    static ::testing::AssertionResult Exec(const char* chunk)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        if (luaL_dostring(L, chunk) != LUA_OK) {
            return ::testing::AssertionFailure()
                   << "dostring failed: " << lua_tostring(L, -1);
        }
        return ::testing::AssertionSuccess();
    }

    lua_State* m_L = nullptr;
};

// 1. 缺席：函数未定义时静默合法
TEST_F(LuaHookTest, AbsentHookIsSilentlyLegal)
{
    CLuaHookCall hook("t_absent");
    EXPECT_FALSE(hook.valid());
    EXPECT_FALSE(hook.call());
    EXPECT_TRUE(hook.result_is_nil());
    EXPECT_EQ(hook.result_int(), 0);
}

// 2. 实参/返回值往返（整数入，整数出）
TEST_F(LuaHookTest, ScalarArgsRoundTrip)
{
    ASSERT_TRUE(
        Exec("hooks.t_scalar = function(a, b, s) "
             "return a + b + #s end"));
    CLuaHookCall hook("t_scalar");
    ASSERT_TRUE(hook.valid());
    hook.add_int(2).add_int(3).add_str("xy");
    ASSERT_TRUE(hook.call());
    EXPECT_FALSE(hook.result_is_nil());
    EXPECT_EQ(hook.result_int(), 7);
}

// 3. nil 与 bool 返回
TEST_F(LuaHookTest, NilAndBoolResults)
{
    ASSERT_TRUE(Exec("hooks.t_nil = function() end"));
    CLuaHookCall nil_hook("t_nil");
    ASSERT_TRUE(nil_hook.valid());
    ASSERT_TRUE(nil_hook.call());
    EXPECT_TRUE(nil_hook.result_is_nil());

    ASSERT_TRUE(Exec("hooks.t_true = function() return true end"));
    CLuaHookCall bool_hook("t_true");
    ASSERT_TRUE(bool_hook.valid());
    ASSERT_TRUE(bool_hook.call());
    EXPECT_TRUE(bool_hook.result_bool());
    EXPECT_FALSE(bool_hook.result_is_nil());
}

// 4. table 字段读取（damage/text/handled 契约）
TEST_F(LuaHookTest, TableFieldContract)
{
    ASSERT_TRUE(
        Exec("hooks.t_table = function() "
             "return {damage = -1, text = '被挡开', handled = true} end"));
    CLuaHookCall hook("t_table");
    ASSERT_TRUE(hook.valid());
    ASSERT_TRUE(hook.call());

    int64_t damage = 0;
    EXPECT_TRUE(hook.table_field_int("damage", damage));
    EXPECT_EQ(damage, -1);

    std::string text;
    EXPECT_TRUE(hook.table_field_str("text", text));
    EXPECT_EQ(text, "被挡开");

    bool handled = false;
    EXPECT_TRUE(hook.table_field_bool("handled", handled));
    EXPECT_TRUE(handled);

    int64_t missing = 0;
    EXPECT_FALSE(hook.table_field_int("absent_key", missing));
}

// 5. 字符串数组读取（广播行契约）
TEST_F(LuaHookTest, StringArrayContract)
{
    ASSERT_TRUE(
        Exec("hooks.t_lines = function() "
             "return {'第一行', '第二行'} end"));
    CLuaHookCall hook("t_lines");
    ASSERT_TRUE(hook.valid());
    ASSERT_TRUE(hook.call());

    EXPECT_EQ(hook.table_str_count(), 2);
    std::string line;
    EXPECT_TRUE(hook.table_str_at(0, line));
    EXPECT_EQ(line, "第一行");
    EXPECT_TRUE(hook.table_str_at(1, line));
    EXPECT_EQ(line, "第二行");
    EXPECT_FALSE(hook.table_str_at(2, line));
}

// 6. 脚本错误：不崩、call()=false、结果按缺席
TEST_F(LuaHookTest, ScriptErrorDoesNotCrash)
{
    ASSERT_TRUE(Exec("hooks.t_error = function() error('boom') end"));
    CLuaHookCall hook("t_error");
    ASSERT_TRUE(hook.valid());
    EXPECT_FALSE(hook.call());
    EXPECT_TRUE(hook.result_is_nil());
    EXPECT_EQ(hook.result_int(), 0);
}

// 7. 栈平衡：A 构造后先完成 B 的调用，A 结果仍正确；析构后栈深还原
TEST_F(LuaHookTest, InterleavedCallsKeepStackBalanced)
{
    ASSERT_TRUE(
        Exec("hooks.t_first = function() return 'first' end "
             "hooks.t_second = function() return 'second' end"));
    int top_before = lua_gettop(m_L);
    {
        CLuaHookCall first("t_first");
        CLuaHookCall second("t_second");
        ASSERT_TRUE(first.valid());
        ASSERT_TRUE(second.valid());

        ASSERT_TRUE(second.call());
        EXPECT_STREQ(second.result_str(), "second");

        ASSERT_TRUE(first.call());
        EXPECT_STREQ(first.result_str(), "first");
    }
    EXPECT_EQ(lua_gettop(m_L), top_before);
}

// 8. boot：重置两表并从默认根加载文件；NULL 状态返回 false
TEST_F(LuaHookTest, BootLoadsHookTables)
{
    EXPECT_FALSE(lua_hook_boot(nullptr));

    ASSERT_TRUE(lua_hook_boot(m_L));

    lua_getglobal(m_L, "hooks");
    EXPECT_TRUE(lua_istable(m_L, -1));
    lua_pop(m_L, 1);
    lua_getglobal(m_L, "commands");
    EXPECT_TRUE(lua_istable(m_L, -1));
    lua_pop(m_L, 1);
}

// 9. script_root 钩子脚本损坏：记错后回退 default 参考脚本，不阻断开机
TEST_F(LuaHookTest, BootFallsBackWhenScriptRootScriptBroken)
{
    // 运行时在系统临时目录构造损坏脚本（不入库，避免污染工程脚本检查）
    namespace fs = std::filesystem;
    const fs::path root
        = fs::temp_directory_path() / "jianghu_broken_hook_root";
    fs::remove_all(root);  // 清理上次残留
    fs::create_directories(root / "data");
    {
        std::ofstream hooks_file(root / "data/hooks.lua");
        hooks_file << "local broken = (\n";
    }

    const std::string saved_root     = ini_get("lua", "script_root", "");
    g_ini_data["lua"]["script_root"] = root.string();

    EXPECT_TRUE(lua_hook_boot(m_L));
    lua_getglobal(m_L, "hooks");
    EXPECT_TRUE(lua_istable(m_L, -1));
    lua_pop(m_L, 1);
    lua_getglobal(m_L, "commands");
    EXPECT_TRUE(lua_istable(m_L, -1));
    lua_pop(m_L, 1);

    g_ini_data["lua"]["script_root"] = saved_root;
    lua_hook_boot(m_L);  // 按恢复后的 script_root 重建钩子表
    fs::remove_all(root);
}

// 10. 结果读取边界：nil 实参、非数值返回、非表拒绝字段/数组读取、表内缺失 bool
// 字段
TEST_F(LuaHookTest, ResultReaderEdges)
{
    ASSERT_TRUE(Exec("hooks.t_scalar = function(a) return a end"));

    CLuaHookCall nil_arg("t_scalar");
    nil_arg.add_str(nullptr);  // 压 nil 实参
    ASSERT_TRUE(nil_arg.call());
    EXPECT_TRUE(nil_arg.result_is_nil());
    EXPECT_FALSE(nil_arg.result_is_number());

    CLuaHookCall str_arg("t_scalar");
    str_arg.add_str("abc");
    ASSERT_TRUE(str_arg.call());
    EXPECT_FALSE(str_arg.result_is_number());
    int64_t v = 0;
    EXPECT_FALSE(str_arg.table_field_int("x", v));
    EXPECT_EQ(str_arg.table_str_count(), 0);
    std::string s;
    EXPECT_FALSE(str_arg.table_str_at(0, s));
    EXPECT_FALSE(str_arg.table_str_at(-1, s));

    // 表内缺失 bool 字段 → table_field_bool 返回 false
    ASSERT_TRUE(Exec("hooks.t_table = function() return {damage = -1} end"));
    CLuaHookCall tbl("t_table");
    ASSERT_TRUE(tbl.call());
    bool b = true;
    EXPECT_FALSE(tbl.table_field_bool("handled", b));
}

// 11. 命令模块加载失败（文件存在但执行出错）：分发返回 -1 回落，不崩。
//     损坏夹具在系统临时目录运行时构造，不入库（避免污染工程脚本检查）。
TEST_F(LuaHookTest, CommandModuleLoadErrorFallsBack)
{
    namespace fs        = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "jianghu_broken_cmd_root";
    fs::remove_all(root);
    fs::create_directories(root / "command");
    {
        std::ofstream f(root / "command" / "t_broken.lua");
        f << "local M = {}\nerror('fixture broken module')\nreturn M\n";
    }

    // 临时把夹具根追加进 package.path（结束时还原）
    std::string saved;
    lua_getglobal(m_L, "package");
    lua_getfield(m_L, -1, "path");
    saved = lua_tostring(m_L, -1);
    lua_pop(m_L, 1);
    lua_pushstring(m_L, (saved + ";" + root.string() + "/?.lua").c_str());
    lua_setfield(m_L, -2, "path");
    lua_pop(m_L, 1);

    // 首次解析报错并缓存失败；二次命中缓存 —— 两次都必须安全回落
    EXPECT_EQ(lua_call_command("t_broken", nullptr, 0, nullptr), -1);
    EXPECT_EQ(lua_call_command("t_broken", nullptr, 0, nullptr), -1);

    lua_getglobal(m_L, "package");
    lua_pushstring(m_L, saved.c_str());
    lua_setfield(m_L, -2, "path");
    lua_pop(m_L, 1);

    fs::remove_all(root);
    lua_commands_install(m_L);  // 丢弃失败缓存，隔离后续用例
}

// 12. 命令名含 '.' 或 '/'：拒绝拼越界模块路径（防 require 目录穿越）。
TEST_F(LuaHookTest, CommandNameRejectsPathSeparators)
{
    EXPECT_EQ(lua_call_command("t_lazy.a", nullptr, 0, nullptr), -1);
    EXPECT_EQ(lua_call_command("../evil", nullptr, 0, nullptr), -1);
    EXPECT_EQ(lua_call_command("a/b", nullptr, 0, nullptr), -1);
}

// 13. 命令缺席（文件不存在）：静默回落，不记失败以外的副作用。
//     表情与拼错的命令名都走这条路，必须零成本命中缓存。
TEST_F(LuaHookTest, CommandAbsentFallsBackQuietly)
{
    EXPECT_EQ(lua_call_command("no_such_command_abc", nullptr, 0, nullptr), -1);
    EXPECT_EQ(lua_call_command("no_such_command_abc", nullptr, 0, nullptr), -1);
}
