// test_log_levels.cpp — Unit tests for log_trace/debug/info/warn/error/fatal
// bindings
//
// Tests the six level-specific Lua logging functions to verify they support
// variadic arguments (string.format style) and handle edge cases.
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Container.h"
#include "LuaEngine.h"
#include "test_lua_env.h"

// ====================================================================
// Test fixture: initializes Lua engine
// ====================================================================
class LuaLogLevelTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    bool RunLua(const char* code)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        return luaL_dostring(L, code) == LUA_OK;
    }

    std::string RunLuaError(const char* code)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        int err      = luaL_dostring(L, code);
        if (err != LUA_OK) {
            const char* s = lua_tostring(L, -1);
            lua_pop(L, 1);
            return s ? s : "unknown error";
        }
        return "";
    }
};

// ====================================================================
// Test: all six functions exist as globals
// ====================================================================
TEST_F(LuaLogLevelTest, AllFunctionsExist)
{
    lua_State* L        = CLuaEngine::instance()->get_state();
    const char* names[] = {"log_trace", "log_debug", "log_info",
                           "log_warn",  "log_error", "log_fatal"};
    for (const char* name : names) {
        lua_getglobal(L, name);
        EXPECT_TRUE(lua_isfunction(L, -1)) << name << " is not a function";
        lua_pop(L, 1);
    }
}

// ====================================================================
// Test: single string argument (no formatting)
// ====================================================================
TEST_F(LuaLogLevelTest, SingleStringArg)
{
    EXPECT_TRUE(RunLua("log_error(\"test message\")"));
    EXPECT_TRUE(RunLua("log_info(\"info message\")"));
}

// ====================================================================
// Test: string.format style arguments
// ====================================================================
TEST_F(LuaLogLevelTest, FormatArgs)
{
    EXPECT_TRUE(RunLua("log_error(\"HP: %d, MP: %d\", 100, 50)"));
}

// ====================================================================
// Test: no arguments should fail (luaL_checkstring)
// ====================================================================
TEST_F(LuaLogLevelTest, NoArgsFails)
{
    std::string err = RunLuaError("log_error()");
    EXPECT_FALSE(err.empty()) << "log_error() with no args should fail";
}

// ====================================================================
// Test: string only (no extra args) should pass
// ====================================================================
TEST_F(LuaLogLevelTest, StringOnlyPasses)
{
    EXPECT_TRUE(RunLua("log_error(\"simple message\")"));
}

// ====================================================================
// Test: mixed format args
// ====================================================================
TEST_F(LuaLogLevelTest, MixedFormatArgs)
{
    EXPECT_TRUE(RunLua("log_error(\"name=%s, level=%d\", \"hero\", 10)"));
}

// ====================================================================
// Test: empty string
// ====================================================================
TEST_F(LuaLogLevelTest, EmptyString) { EXPECT_TRUE(RunLua("log_error(\"\")")); }
