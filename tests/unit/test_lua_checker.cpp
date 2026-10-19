// test_lua_checker.cpp — --check-lua 的 require 闭包校验
//
// 构建与运行（唯一认可方式）: .\run-coverage.ps1（仓库根）
//
// 背景：语法校验只做 luaL_loadbuffer，字面量 require("a.b") 指向不存在的
// 模块要到运行期脚本加载才报错（历史事故：lua-default/skill/high/animal
// 引用不存在的 skill.skill_utils，语法全绿、加载即炸）。check_lua_files
// 在语法通过后把字面量 require 解析到 package.path 上的实体文件，
// 动态 require（require(name)）无法静态校验，只记 NOTE 不判失败。
#include "pch.h"

#include "lua/LuaSyntaxChecker.h"
#include "test_lua_env.h"

#include <string>
#include <vector>

namespace {
// 夹具位于 script_root（tests/unit/lua-tests）下，经 package.path 解析
const char* kGoodReq    = "tests/unit/lua-tests/check_fixture/good_req.lua";
const char* kMissingReq = "tests/unit/lua-tests/check_fixture/missing_req.lua";
const char* kDynamicReq = "tests/unit/lua-tests/check_fixture/dynamic_req.lua";
const char* kCommentReq = "tests/unit/lua-tests/check_fixture/comment_req.lua";
const char* kLongBracketReq =
    "tests/unit/lua-tests/check_fixture/long_bracket_req.lua";
const char* kSyntaxError = "tests/unit/lua-tests/check_fixture/syntax_error.lua";

class LuaCheckerRequireTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    // 校验单文件：返回失败文件数（0 = 通过）
    int check(const char* path)
    {
        std::vector<std::string> files{path};
        return check_lua_files(ensure_lua_env(), files);
    }
};
}  // namespace

TEST_F(LuaCheckerRequireTest, ResolvableRequirePasses)
{
    EXPECT_EQ(check(kGoodReq), 0) << "存在的依赖模块应通过闭包校验";
}

TEST_F(LuaCheckerRequireTest, MissingModuleFails)
{
    EXPECT_EQ(check(kMissingReq), 1) << "指向不存在模块的 require 应判失败";
}

TEST_F(LuaCheckerRequireTest, DynamicRequireSkipped)
{
    EXPECT_EQ(check(kDynamicReq), 0)
        << "非字面量 require 无法静态校验，不应误判失败";
}

TEST_F(LuaCheckerRequireTest, RequireInCommentOrStringIgnored)
{
    EXPECT_EQ(check(kCommentReq), 0)
        << "注释与字符串里的 require 示例不应被收集";
}

TEST_F(LuaCheckerRequireTest, LongBracketsIgnoredAndAccepted)
{
    EXPECT_EQ(check(kLongBracketReq), 0)
        << "长注释/长字符串内的 require 跳过，长括号字面量模块名正常解析";
}

TEST_F(LuaCheckerRequireTest, SyntaxErrorFails)
{
    EXPECT_EQ(check(kSyntaxError), 1) << "语法错误在语法阶段即判失败";
}

TEST_F(LuaCheckerRequireTest, UnreadableFileFails)
{
    EXPECT_EQ(check("tests/unit/lua-tests/check_fixture/not_there.lua"), 1)
        << "文件不可读应判失败";
}

TEST(LuaCheckerNullState, NullStateReturnsFailure)
{
    std::vector<std::string> none;
    EXPECT_EQ(check_lua_files(nullptr, none), 1);
}
