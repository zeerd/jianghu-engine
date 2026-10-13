// test_lua_output_error.cpp — Unit tests for l_output_error binding
//
// Tests the output_error Lua function to verify it supports
// variadic arguments (string.format style) and handles edge cases.
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
class LuaOutputErrorTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    // Execute Lua code and return true on success
    bool RunLua(const char* code)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        return luaL_dostring(L, code) == LUA_OK;
    }

    // Execute Lua code and get error message
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
// Test: output_error exists as global function
// ====================================================================
TEST_F(LuaOutputErrorTest, FunctionExists)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    lua_getglobal(L, "output_error");
    EXPECT_TRUE(lua_isfunction(L, -1));
    lua_pop(L, 1);
}

// ====================================================================
// Test: output_error with single string argument (no formatting)
// ====================================================================
TEST_F(LuaOutputErrorTest, SingleStringArg)
{
    EXPECT_TRUE(RunLua("output_error(\"test message\")"));
}

// ====================================================================
// Test: output_error with string.format style arguments
// ====================================================================
TEST_F(LuaOutputErrorTest, FormatArgs)
{
    EXPECT_TRUE(RunLua("output_error(\"HP: %d, MP: %d\", 100, 50)"));
}

// ====================================================================
// Test: output_error with no arguments should fail
// ====================================================================
TEST_F(LuaOutputErrorTest, NoArgsFails)
{
    std::string err = RunLuaError("output_error()");
    EXPECT_FALSE(err.empty()) << "output_error() with no args should fail";
}

// ====================================================================
// Test: output_error with string only (no extra args) should pass
// ====================================================================
TEST_F(LuaOutputErrorTest, StringOnlyPasses)
{
    EXPECT_TRUE(RunLua("output_error(\"simple message\")"));
}

// ====================================================================
// Test: output_error with mixed format args
// ====================================================================
TEST_F(LuaOutputErrorTest, MixedFormatArgs)
{
    EXPECT_TRUE(RunLua("output_error(\"name=%s, level=%d\", \"hero\", 10)"));
}

// ====================================================================
// Test: output_error with empty string
// ====================================================================
TEST_F(LuaOutputErrorTest, EmptyString)
{
    EXPECT_TRUE(RunLua("output_error(\"\")"));
}
