// test_lua_smoke.cpp — 钩子冒烟（src/lua/LuaSmokeCheck.cpp）
//
// 覆盖目标：
//   - 干净脚本：create / do_use 各驱动一次，零违规
//   - 钩子内抛错：引擎侧 lua_pcall 会吞掉它，冒烟必须记成违规
//   - 被脚本自己的 pcall 吞掉的错误：静态防线与引擎日志都看不见
//   - 被 require 的 item 目录模块不参与冒烟（它是库，不是数据键）
//   - 用完把全局 pcall 还原，不污染同状态里的其它测试
//
//
#include "pch.h"

#include <initializer_list>
#include <string>
#include <vector>

#include <lua.hpp>

#include "lua/LuaSmokeCheck.h"
#include "test_lua_env.h"

namespace {
const char* kRoot = "tests/unit/lua-tests/";

class LuaSmokeTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    SmokeReport smoke(std::initializer_list<const char*> rels)
    {
        std::vector<std::string> files;
        for (const char* r : rels) files.push_back(std::string(kRoot) + r);
        return smoke_check_items(ensure_lua_env(), files);
    }
};
}  // namespace

// 干净脚本：两个钩子都被驱动，且不产生任何违规
TEST_F(LuaSmokeTest, CleanScriptDrivesHooksWithoutViolation)
{
    lua_State* L        = ensure_lua_env();
    const int stack_top = lua_gettop(L);  // 冒烟借引擎状态用，不得改变它

    const SmokeReport rep = smoke({"item/fixture_lua_item.lua"});
    EXPECT_EQ(1, rep.scripts);
    EXPECT_EQ(2, rep.hooks) << "create 与 do_use 各驱动一次";
    EXPECT_TRUE(rep.violations.empty());
    EXPECT_GE(rep.elapsed_ms, 0);
    EXPECT_GE(rep.slowest_ms, 0);
    EXPECT_EQ(stack_top, lua_gettop(L)) << "冒烟改变了借用的引擎栈";
}

// 钩子抛错：引擎侧 pcall 吞掉只写日志，冒烟必须把它翻成违规
TEST_F(LuaSmokeTest, HookErrorEscapingToEngineIsReported)
{
    const SmokeReport rep = smoke({"item/fixture_smoke_boom.lua"});
    ASSERT_FALSE(rep.violations.empty()) << "钩子抛错必须成为违规";
    EXPECT_EQ("create", rep.violations[0].hook);
    EXPECT_NE(std::string::npos, rep.violations[0].message.find("smoke_boom"));
}

// 被脚本自己的 pcall 吞掉的错误：只有记录版 pcall 看得见
TEST_F(LuaSmokeTest, ErrorSwallowedByScriptPcallIsReported)
{
    const SmokeReport rep = smoke({"item/fixture_smoke_swallow.lua"});
    ASSERT_FALSE(rep.violations.empty())
        << "被脚本 pcall 吞掉的 nil 调用必须可见";
    EXPECT_EQ("pcall", rep.violations[0].hook);
}

// 被 require 的 item 目录文件是模块：不驱动它自己的 create
TEST_F(LuaSmokeTest, RequiredModuleUnderItemIsSkipped)
{
    const SmokeReport rep = smoke(
        {"item/fixture_smoke_lib.lua", "item/fixture_smoke_lib_user.lua"});
    EXPECT_EQ(1, rep.skipped) << "被 require 的是模块，不冒烟";
    for (const SmokeViolation& v : rep.violations) {
        EXPECT_EQ(std::string::npos, v.file.find("fixture_smoke_lib.lua"))
            << "模块自身不该被冒烟: " << v.message;
    }
}

// 冒烟把全局 pcall 换成记录版，收工必须还原成原件（无 upvalue 的那个）
TEST_F(LuaSmokeTest, GlobalPcallRestoredAfterSmoke)
{
    lua_State* L = ensure_lua_env();
    smoke({"item/fixture_smoke_boom.lua"});

    lua_getglobal(L, "pcall");
    ASSERT_TRUE(lua_isfunction(L, -1)) << "pcall 必须还在";
    const char* up = lua_getupvalue(L, -1, 1);
    EXPECT_TRUE(up == NULL) << "冒烟的 pcall 包装没有还原";
    if (up) lua_pop(L, 1);
    lua_pop(L, 1);
}

// 模块体执行期抛错：语法是好的（防线1 报不出），load_script 返回 NOREF
TEST_F(LuaSmokeTest, ModuleBodyErrorIsReportedAsLoad)
{
    const SmokeReport rep = smoke({"item/fixture_smoke_bodyboom.lua"});
    ASSERT_FALSE(rep.violations.empty()) << "模块体出错必须记成 load 违规";
    EXPECT_EQ("load", rep.violations[0].hook);
    EXPECT_NE(std::string::npos,
              rep.violations[0].message.find("smoke_body_boom"));
}

// pcall 参数缺失：原 pcall 自己抛错，包装层"原 pcall 就炸"这条上抛路径
// 要走到并记进日志。该夹具只有这一条出错路径，故 hook=pcall 即为证据。
TEST_F(LuaSmokeTest, PcallMisuseInsideHookIsReported)
{
    const SmokeReport rep = smoke({"item/fixture_smoke_pcall_args.lua"});
    bool seen             = false;
    std::string all;
    for (const SmokeViolation& v : rep.violations) {
        if (v.hook == "pcall") seen = true;
        all += "[" + v.hook + "] " + v.message + "; ";
    }
    EXPECT_TRUE(seen) << "原 pcall 自身抛错也要进日志，实际: " << all;
}

// 非 item 目录的文件不参与冒烟：点名解析为空即跳过
TEST_F(LuaSmokeTest, NonItemFileIsIgnored)
{
    const SmokeReport rep
        = smoke({"check_fixture/good_req.lua", "item/fixture_lua_item.lua"});
    EXPECT_EQ(1, rep.scripts) << "只有 item 数据键参与冒烟";
    EXPECT_TRUE(rep.violations.empty());
}
