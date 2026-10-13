// test_cmd_lua_command.cpp - Lua 命令分发（CCmdHandle → commands 表）单元测试
//
// 契约覆盖：
//   1. 未定义命令回落：C++ 三表与 commands 表均未命中 → 0（交
//   emote/未知命令链）。
//   2. Lua 命令命中：执行并透传返回值，实参序为 (me, target, arg)。
//   3. C++ 表优先：同名命令由 C++ 先声明，commands 不能遮蔽 standard 表。
//   4. 脚本错误不崩：执行出错 → -1 → 分发链回落 0。
//   5. 无返回视为已处理（nil → 1）。
//   6. 权限由脚本自检：command/showmjexp.lua 对非巫师拒绝(0)、巫师执行(1)。
#include "pch.h"

#include <string>

#include "Char.h"
#include "CmdHandle.h"
#include "Container.h"
#include "IDatabase.h"
#include "Sql.h"
#include "lua/LuaEngine.h"
#include "lua/LuaHook.h"
#include "test_lua_env.h"
#include "mock/mock_database.h"

extern char g_notify_fail_msg[512];

namespace {

class CmdLuaHarness : public CCmdHandle {
public:
    int Dispatch(const char* cmd, CChar* me, int32_t target, const char* arg)
    {
        return handlecommand(const_cast<char*>(cmd), me, target,
                             const_cast<char*>(arg));
    }
};

class CmdLuaCommandTest : public ::testing::Test {
protected:
    CContainer room;
    CChar me;
    MockDatabase db;

    static ::testing::AssertionResult Exec(const char* chunk)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        if (luaL_dostring(L, chunk) != LUA_OK) {
            return ::testing::AssertionFailure()
                   << "dostring failed: " << lua_tostring(L, -1);
        }
        return ::testing::AssertionSuccess();
    }

    static void ClearDefs()
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        if (!L) return;
        luaL_dostring(L,
                      "commands.t_probe = nil commands.t_err = nil "
                      "commands.t_noret = nil commands.t_null = nil "
                      "commands.t_co = nil commands.t_lazy = nil "
                      "commands.look = nil _G.__cmd_cap = nil "
                      "_G.__lazy_cap = nil _G.__co_id = nil "
                      "_G.__saved_commands = nil");
    }

    void SetUp() override
    {
        ensure_lua_env();
        g_db                 = &db;  // showmjexp 的 db_query 边界
        g_notify_fail_msg[0] = '\0';
        ClearDefs();
        room.set_name("room", "cmd_lua_room");
        me.move(&room);
    }

    void TearDown() override
    {
        ClearDefs();
        // 脚本内 call_out 会把对象登记进全局心跳链表，容器析构不注销；
        // 栈对象离开作用域前必须撤登记，否则残留悬垂指针
        me.set_heart_beat(0);
        me.move(nullptr);
        room.move(nullptr);
        g_db = nullptr;
    }
};

}  // namespace

// 1. 未定义命令 → 回落 0
TEST_F(CmdLuaCommandTest, UndefinedCommandFallsThroughToZero)
{
    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("no_such_cmd_xyz", &me, 0, ""), 0);
}

// 2. Lua 命令执行、实参序与返回值透传
TEST_F(CmdLuaCommandTest, LuaCommandExecutesAndReceivesArgs)
{
    ASSERT_TRUE(
        Exec("commands.t_probe = function(me, target, arg) "
             "  _G.__cmd_cap = {target = target, arg = arg} "
             "  return 1 "
             "end"));

    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("t_probe", &me, 77, "abc"), 1);

    lua_State* L = ensure_lua_env();
    lua_getglobal(L, "__cmd_cap");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "target");
    EXPECT_EQ(lua_tointeger(L, -1), 77);
    lua_pop(L, 1);
    lua_getfield(L, -1, "arg");
    EXPECT_STREQ(lua_tostring(L, -1), "abc");
    lua_pop(L, 2);
}

// 3. C++ 表优先：同名 standard 命令由 C++ 声明（返回 0 而非 Lua 的 42）
TEST_F(CmdLuaCommandTest, CppTableClaimsBeforeLua)
{
    ASSERT_TRUE(Exec("commands.look = function() return 42 end"));

    CmdLuaHarness harness;
    // look 指定不存在的目标 → C++ 分支 notify_fail 返回 0；
    // 若 commands 遮蔽了 standard 表则会得到 42
    EXPECT_EQ(harness.Dispatch("look", &me, 99999, ""), 0);
}

// 4. 脚本错误不崩，回落 0
TEST_F(CmdLuaCommandTest, ScriptErrorFallsBackToZero)
{
    ASSERT_TRUE(Exec("commands.t_err = function() error('boom') end"));

    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("t_err", &me, 0, ""), 0);
}

// 5. 无返回视为已处理
TEST_F(CmdLuaCommandTest, NoReturnTreatedAsHandled)
{
    ASSERT_TRUE(Exec("commands.t_noret = function() end"));

    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("t_noret", &me, 0, ""), 1);
}

// 7. 空命令名：直接回落，不入脚本
TEST_F(CmdLuaCommandTest, EmptyCommandFallsThrough)
{
    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("", &me, 0, ""), 0);
    EXPECT_EQ(lua_call_command(nullptr, &me, 0, nullptr), -1);
}

// 8. commands 表缺失：回落 0（脚本层故障不影响分发链）
TEST_F(CmdLuaCommandTest, MissingCommandsTableFallsThrough)
{
    lua_State* L = ensure_lua_env();
    ASSERT_EQ(luaL_dostring(L, "__saved_commands = commands"), LUA_OK);
    ASSERT_EQ(luaL_dostring(L, "commands = nil"), LUA_OK);

    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("anything", &me, 0, ""), 0);

    ASSERT_EQ(luaL_dostring(L, "commands = __saved_commands"), LUA_OK);
}

// 9. me/arg 为空时压 nil 传入脚本
TEST_F(CmdLuaCommandTest, NullMeAndArgPassedAsNil)
{
    ASSERT_TRUE(
        Exec("commands.t_null = function(me, target, arg) "
             "return (me == nil and arg == nil) and 1 or 0 end"));
    EXPECT_EQ(lua_call_command("t_null", nullptr, 7, nullptr), 1);
}

// 10. 命令模块懒加载：command/<命令名>.lua 即命令，无需任何注册
//     （夹具 lua-tests/command/t_lazy.lua，入口键与文件名同名）
TEST_F(CmdLuaCommandTest, CommandModuleLazyLoadedByName)
{
    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("t_lazy", &me, 77, "abc"), 1);

    lua_State* L = ensure_lua_env();
    lua_getglobal(L, "__lazy_cap");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "name");
    EXPECT_EQ(lua_tointeger(L, -1), 77);
    lua_pop(L, 1);
    lua_getfield(L, -1, "arg");
    EXPECT_STREQ(lua_tostring(L, -1), "abc");
    lua_pop(L, 2);
}

// 11. commands 表中脚本直定义的命令优先于同名模块文件（__index 只在未命中触发）
TEST_F(CmdLuaCommandTest, DirectTableDefShadowesModuleFile)
{
    ASSERT_TRUE(Exec("commands.t_lazy = function() return 5 end"));
    EXPECT_EQ(lua_call_command("t_lazy", &me, 0, ""), 5);
}

// 12. 命令分发期间，脚本可对无脚本表的对象（玩家/普通角色）挂 call_out：
//     l_call_out 反查不到 self 的脚本表时，回退取 commands 表引用。
//     迁移前 lua_call_command 不设 active script ref，此用例必然失败。
TEST_F(CmdLuaCommandTest, CommandDispatchEnablesCalloutOnPlainObject)
{
    ASSERT_TRUE(
        Exec("commands.t_co = function(me, target, arg) "
             "_G.__co_id = me:call_out(function(self, p1, p2) "
             "  _G.__cmd_cap = {p1 = p1, p2 = p2} "
             "  return 1 "
             "end, 1, 11, 22) "
             "return 1 end"));

    CmdLuaHarness harness;
    EXPECT_EQ(harness.Dispatch("t_co", &me, 0, ""), 1);

    lua_State* L = ensure_lua_env();
    lua_getglobal(L, "__co_id");
    const lua_Integer id = lua_tointeger(L, -1);
    lua_pop(L, 1);
    ASSERT_GT(id, 0) << "call_out 注册失败：命令分发期未提供脚本引用回退";

    me.heart_beat();

    lua_getglobal(L, "__cmd_cap");
    ASSERT_TRUE(lua_istable(L, -1)) << "回调未执行";
    lua_getfield(L, -1, "p1");
    EXPECT_EQ(lua_tointeger(L, -1), 11);
    lua_pop(L, 1);
    lua_getfield(L, -1, "p2");
    EXPECT_EQ(lua_tointeger(L, -1), 22);
    lua_pop(L, 2);
}

// ====================================================================
// LuaEngine.h 的两个 RAII guard：作用域析构路径（332/347）
// ====================================================================
TEST(LuaEngineGuardsTest, ActiveRefGuardRestoresOnDestruct)
{
    CLuaEngine* eng = g_LuaEngine;  // 单例宏 → instance() 指针
    int before      = eng->get_active_script_ref();

    {
        CLuaActiveRefGuard guard(123);
        EXPECT_EQ(123, eng->get_active_script_ref()) << "构造时接管 active";
    }  // 析构恢复(332)

    EXPECT_EQ(before, eng->get_active_script_ref())
        << "析构恢复旧 active 值(332)";
}

TEST(LuaEngineGuardsTest, CalloutExecGuardRestoresOnDestruct)
{
    int32_t target = 7;
    {
        CLuaCalloutExecGuard guard(target, 42);
        EXPECT_EQ(42, target) << "构造时接管 callout id";
    }  // 析构恢复(347)

    EXPECT_EQ(7, target) << "析构恢复旧值(347)";
}
