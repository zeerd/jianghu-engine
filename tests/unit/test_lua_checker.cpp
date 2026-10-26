// test_lua_checker.cpp — --check-lua 的静态校验
//
// 构建与运行（唯一认可方式）: .\run-coverage.ps1（仓库根）
//
// 四道防线各自对应一次线上故障，这里逐道守护（详见 LuaSyntaxChecker.cpp
// 文件头）：
//   1. 语法         括号/引号不匹配
//   2. require 闭包 字面量 require 指向不存在的模块（语法全绿、加载即炸）
//   3. 未绑定全局   card_core 调 get_db_type()，登录期才抛 nil value
//   4. 未绑定方法   self:set_verbs()，对象元表里只有 set_beast_verbs
//
// 第 3 道最容易因"没装绑定表"而整片误报或漏报，故专门用裸 lua_State 用例钉住
// check_lua_files 自持调用 lua_bind_init 这一前提。
#include "pch.h"

#include "lua/LuaSyntaxChecker.h"
#include "test_lua_env.h"

#include <lua.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
// 夹具位于 script_root（tests/unit/lua-tests）下，经 package.path 解析
const char* kGoodReq    = "tests/unit/lua-tests/check_fixture/good_req.lua";
const char* kMissingReq = "tests/unit/lua-tests/check_fixture/missing_req.lua";
const char* kDynamicReq = "tests/unit/lua-tests/check_fixture/dynamic_req.lua";
const char* kCommentReq = "tests/unit/lua-tests/check_fixture/comment_req.lua";
const char* kLongBracketReq
    = "tests/unit/lua-tests/check_fixture/long_bracket_req.lua";
const char* kSyntaxError
    = "tests/unit/lua-tests/check_fixture/syntax_error.lua";

const char* kStrArith = "tests/unit/lua-tests/check_fixture/str_arith.lua";
const char* kConcatOk = "tests/unit/lua-tests/check_fixture/concat_ok.lua";
const char* kUnknownGlob
    = "tests/unit/lua-tests/check_fixture/unknown_global.lua";
const char* kBoundGlobOk
    = "tests/unit/lua-tests/check_fixture/bound_global_ok.lua";
const char* kGotoLabelOk
    = "tests/unit/lua-tests/check_fixture/goto_label_ok.lua";
const char* kHexLiteralOk
    = "tests/unit/lua-tests/check_fixture/hex_literal_ok.lua";
const char* kUnknownMeth
    = "tests/unit/lua-tests/check_fixture/unknown_method.lua";
const char* kMethodOk = "tests/unit/lua-tests/check_fixture/method_ok.lua";

// 作用域/定义收集的回归夹具
const char* kForinDef = "tests/unit/lua-tests/check_fixture/forin_def.lua";
const char* kForinAssign
    = "tests/unit/lua-tests/check_fixture/forin_assign.lua";
const char* kForinRead = "tests/unit/lua-tests/check_fixture/forin_read.lua";
const char* kForinUnknown
    = "tests/unit/lua-tests/check_fixture/forin_unknown.lua";
const char* kMultiAssign
    = "tests/unit/lua-tests/check_fixture/multi_assign.lua";
const char* kFileMethodOk
    = "tests/unit/lua-tests/check_fixture/file_method_ok.lua";
const char* kGlobalViaG = "tests/unit/lua-tests/check_fixture/global_via_g.lua";
const char* kGlobalViaGRead
    = "tests/unit/lua-tests/check_fixture/global_via_g_read.lua";
const char* kTableFnScope
    = "tests/unit/lua-tests/check_fixture/table_fn_scope.lua";
const char* kTableFnScopeRead
    = "tests/unit/lua-tests/check_fixture/table_fn_scope_read.lua";
const char* kStrArithParen
    = "tests/unit/lua-tests/check_fixture/str_arith_paren.lua";
const char* kStrArithCallOk
    = "tests/unit/lua-tests/check_fixture/str_arith_call_ok.lua";
const char* kNoparenReq
    = "tests/unit/lua-tests/check_fixture/noparen_req_bad.lua";
const char* kConcatReq = "tests/unit/lua-tests/check_fixture/concat_req_ok.lua";

// 基线文件由用例自己在仓库外的临时目录生成、析构即删。
// 它只是测试的 arrange（生产接口按路径读基线，注释行/空行/CRLF/重复条目
// 计数都得真跑一遍），不是需要跟代码同步演进的数据 —— 提交进仓库只会变成
// 没有别的消费者的孤儿文件。
class ScopedBaseline {
public:
    ScopedBaseline(const char* name, const std::vector<std::string>& lines = {})
    {
        m_path = (std::filesystem::temp_directory_path() / name).string();
        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        // 用 CRLF：load_baseline 对 \r\n 与 \n 都要吃得下
        for (const std::string& line : lines) out << line << "\r\n";
    }

    ~ScopedBaseline()
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    ScopedBaseline(const ScopedBaseline&)            = delete;
    ScopedBaseline& operator=(const ScopedBaseline&) = delete;

    const char* c_str() const { return m_path.c_str(); }

private:
    std::string m_path;
};

class LuaCheckerRequireTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    // 校验单文件：返回失败文件数（0 = 通过）
    int check(const char* path)
    {
        std::vector<std::string> files{path};
        return check_lua_files(ensure_lua_env(), files);
    }

    // 校验一组文件（可带基线选项）
    int check_all(const std::vector<std::string>& files,
                  const CheckLuaOptions* opts = nullptr)
    {
        return check_lua_files(ensure_lua_env(), files, opts);
    }
};
}  // namespace

// ---------------- 防线 1/2：语法与 require 闭包 ----------------

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

// ---------------- 防线 3：未绑定全局 ----------------

TEST_F(LuaCheckerRequireTest, StringArithmeticFails)
{
    EXPECT_EQ(check(kStrArith), 1)
        << "字符串字面量参与 + 必然运行期抛错，应判失败";
}

TEST_F(LuaCheckerRequireTest, ConcatOperatorPasses)
{
    EXPECT_EQ(check(kConcatOk), 0)
        << "'..' 是合法字符串拼接，不得被算术检测误伤";
}

TEST_F(LuaCheckerRequireTest, UnknownGlobalFails)
{
    EXPECT_EQ(check(kUnknownGlob), 1) << "引擎未绑定且无脚本定义的全局应判失败";
}

TEST_F(LuaCheckerRequireTest, BoundGlobalPasses)
{
    EXPECT_EQ(check(kBoundGlobOk), 0) << "已绑定的全局不得被误报";
}

// goto <label> 与 ::label:: 是跳转目标，不是全局读取。
// 回归：曾把数据集里 78 处 goto 的标签名当成未绑定全局。
TEST_F(LuaCheckerRequireTest, GotoLabelNotFlagged)
{
    EXPECT_EQ(check(kGotoLabelOk), 0) << "goto 标签不应被当成全局读取";
}

// 0x41 是数字字面量。词法器若只消费 '0'，剩下的 "x41" 会被当成全局读取，
// 数据集里 4 个文件的字节范围判断因此被误报（回归）。
TEST_F(LuaCheckerRequireTest, HexLiteralNotFlagged)
{
    EXPECT_EQ(check(kHexLiteralOk), 0) << "十六进制字面量不得被拆出伪全局名";
}

// ---------------- 防线 4：未绑定方法 ----------------

TEST_F(LuaCheckerRequireTest, UnknownMethodFails)
{
    EXPECT_EQ(check(kUnknownMeth), 1)
        << "对象元表里没有、脚本也没定义的 ':' 方法应判失败";
}

TEST_F(LuaCheckerRequireTest, DefinedAndBoundMethodPass)
{
    EXPECT_EQ(check(kMethodOk), 0)
        << "脚本自定义方法与引擎对象方法都不得被误报";
}

// ---------------- 基线豁免（存量只清不增）----------------

TEST_F(LuaCheckerRequireTest, BaselineSuppressesExisting)
{
    const ScopedBaseline base("jh_baseline_hit.txt",
                              {"# both known violations exempted here.", "",
                               "A:definitely_not_bound_xyz",
                               "B:tests/unit/lua-tests/check_fixture/"
                               "str_arith.lua:alpha"});
    CheckLuaOptions opts;
    opts.baseline_path = base.c_str();
    const std::vector<std::string> files{kUnknownGlob, kStrArith};
    EXPECT_EQ(check_all(files, &opts), 0) << "基线条目命中的存量违规不判失败";
}

TEST_F(LuaCheckerRequireTest, BaselineDoesNotSuppressNew)
{
    const ScopedBaseline base("jh_baseline_miss.txt",
                              {"# covers an unrelated key only, so every",
                               "A:some_other_unrelated_name"});
    CheckLuaOptions opts;
    opts.baseline_path = base.c_str();  // 只豁免了别的名字
    const std::vector<std::string> files{kStrArith};
    EXPECT_EQ(check_all(files, &opts), 1) << "基线未覆盖的新违规必须拦下";
}

// check_lua_files 必须自持 lua_bind_init：否则换一个未绑定的裸状态就整片
// 把 notify_fail 之类报成"未知全局"（历史上 --check-lua 正是漏了这一步）。
TEST(LuaCheckerSelfBinding, BareStateStillSeesBoundGlobals)
{
    lua_State* L = luaL_newstate();
    ASSERT_NE(L, nullptr);
    luaL_openlibs(L);

    const std::vector<std::string> files{kBoundGlobOk};
    EXPECT_EQ(check_lua_files(L, files), 0)
        << "check_lua_files 应自行完成绑定注册，不依赖调用方";

    lua_close(L);
}

// ---------------- 作用域 / 定义收集的回归 ----------------

// 回归（P0）：for 头部扫描只认 LT_OP 的 `=`，而 `in` 是关键字，循环头收不了
// 尾，一路把后面的 token 全吞进 locals —— 定义随之丢失、跨文件读者被误报。
TEST_F(LuaCheckerRequireTest, ForInLoopKeepsFollowingDefinitions)
{
    const std::vector<std::string> files{kForinDef, kForinAssign, kForinRead};
    EXPECT_EQ(check_all(files), 0)
        << "for..in 之后与循环体内的全局定义必须登记，跨文件读取不得误报";
}

// 同一个缺陷的反面：窗口里的未知全局读不能因此被放过
TEST_F(LuaCheckerRequireTest, ForInLoopStillFlagsUnknownGlobal)
{
    EXPECT_EQ(check(kForinUnknown), 1)
        << "紧跟 for..in 的未知全局必须照报，不能被吞进 locals";
}

// 回归：`a, b = ...` 只有紧邻 '=' 的目标被当成赋值，前面的目标既没登记成
// 定义、又被当成全局读取报出来（一条赋值报两次）。
TEST_F(LuaCheckerRequireTest, MultiTargetAssignDefinesEveryTarget)
{
    EXPECT_EQ(check(kMultiAssign), 0)
        << "多目标赋值的每个目标都算定义，第一个目标不得当成全局读取";
}

// 回归：io 文件句柄的方法挂在 FILE 元表上，只收 io 表的键会把 f:seek()
// 误报成未绑定方法。
TEST_F(LuaCheckerRequireTest, FileHandleMethodsNotFlagged)
{
    EXPECT_EQ(check(kFileMethodOk), 0)
        << "f:seek()/f:setvbuf() 来自 FILE 元表，不得误报";
}

// 回归：_G.name = v / rawset(_G, "name", v) 是间接定义全局，读者必须认得。
TEST_F(LuaCheckerRequireTest, GlobalDefinedViaGResolvesForReaders)
{
    const std::vector<std::string> files{kGlobalViaG, kGlobalViaGRead};
    EXPECT_EQ(check_all(files), 0)
        << "经 _G 定义的全局必须登记，跨文件读取不得误报";
}

// 回归：只按花括号深度判字段，会把表构造器里嵌的函数体
// （{ init = function() x = 1 end }）的赋值当表字段，定义随之丢失。
TEST_F(LuaCheckerRequireTest, GlobalInFunctionNestedInTableDefines)
{
    const std::vector<std::string> files{kTableFnScope, kTableFnScopeRead};
    EXPECT_EQ(check_all(files), 0)
        << "表构造器内函数体里的赋值是语句级定义，不是表字段";
}

// 回归：字符串算术只看紧邻 token，("a") + 1 这种带括号的形态漏检。
TEST_F(LuaCheckerRequireTest, ParenthesizedStringArithmeticFails)
{
    EXPECT_EQ(check(kStrArithParen), 1)
        << "(\"a\") + 1 与 \"a\" + 1 同错，括号不改变操作数类型";
}

// 上一条的反面：f("a") + 1 里的字符串是实参不是操作数，那是合法 Lua
TEST_F(LuaCheckerRequireTest, StringArgumentIsNotStringArithmetic)
{
    EXPECT_EQ(check(kStrArithCallOk), 0)
        << "字符串作为实参传给函数后再参与算术，不得误报";
}

// 回归：require "mod"（无括号）是字面量调用，此前既不校验也不记 NOTE，
// 指向不存在的模块会静默通过。
TEST_F(LuaCheckerRequireTest, RequireWithoutParensIsChecked)
{
    EXPECT_EQ(check(kNoparenReq), 1) << "require \"mod\" 必须参与闭包校验";
}

// 回归：require("a" .. b) 没有字面量模块名，此前把前缀 "a" 当模块名去解析，
// 必然失败 → 误报。应按动态 require 跳过。
TEST_F(LuaCheckerRequireTest, ConcatRequireIsTreatedAsDynamic)
{
    EXPECT_EQ(check(kConcatReq), 0)
        << "拼接模块名没有字面量，按动态 require 只记 NOTE 不判失败";
}

// 回归：--write-baseline 只顾落盘、直接返回 0，语法写坏的树也能"全绿"。
// 写基线时违规不判失败，但语法错误/读不到文件照常失败，基线也必须落盘。
TEST_F(LuaCheckerRequireTest, WriteBaselineDoesNotHideSyntaxErrors)
{
    const ScopedBaseline base("jh_baseline_write.txt");  // 空文件，待用例覆盖
    CheckLuaOptions opts;
    opts.write_baseline = true;
    opts.baseline_path  = base.c_str();
    const std::vector<std::string> files{kUnknownGlob, kSyntaxError};

    EXPECT_GT(check_all(files, &opts), 0) << "写基线不能把语法错误洗成全绿";

    std::ifstream in(base.c_str());
    std::string line;
    std::string content;
    while (std::getline(in, line)) content += line + "\n";
    in.close();

    EXPECT_NE(content.find("A:definitely_not_bound_xyz"), std::string::npos)
        << "违规仍要写进基线（基线只豁免违规，管不着语法错误）";
}
