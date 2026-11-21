// test_lua_checker.cpp — --check-lua 的静态校验
//
// 构建与运行（唯一认可方式）: .\run-unit-tests.ps1（仓库根）
//
// 各道防线各自对应一次线上故障，这里逐道守护（详见 LuaSyntaxChecker.cpp
// 文件头）：
//   1. 语法         括号/引号不匹配
//   2. require 闭包 字面量 require 指向不存在的模块（语法全绿、加载即炸）
//   3. 未绑定全局   card_core 调 get_db_type()，登录期才抛 nil value
//   4. 未绑定方法   self:set_verbs()，对象元表里只有 set_beast_verbs
//   5. call_out 目标 延迟函数不在发起脚本的返回表里（xy_tielao 的 do_tanhua
//                   写成顶层裸 function）——注册期按名查不到、回调挂不上
//   9. 模块表字段   mingjiao_db 把 on_mj_special 写成全局函数，35 处
//                   mj_db.on_mj_special() 取到 nil（cast即抛 field 为 nil）
//
// 第 3 道最容易因"没装绑定表"而整片误报或漏报，故专门用裸 lua_State 用例钉住
// check_lua_files 自持调用 lua_bind_init 这一前提。
#include "pch.h"

#include "lua/LuaSyntaxChecker.h"
#include "test_lua_env.h"
#include "test_tmp_dir.h"

#include <lua.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
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
// self 接收者的严格口径：脚本表字段 / 标准库方法名都到不了 self 上
const char* kSelfScriptMeth
    = "tests/unit/lua-tests/check_fixture/self_script_method_bad.lua";
const char* kSelfStdlibMeth
    = "tests/unit/lua-tests/check_fixture/self_stdlib_method_bad.lua";

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

// 防线5：call_out 目标可见性（docs/guide/call_out.md 规则1）
const char* kCalloutGlobalBad
    = "tests/unit/lua-tests/check_fixture/callout_global_bad.lua";
const char* kCalloutLocalBad
    = "tests/unit/lua-tests/check_fixture/callout_local_bad.lua";
const char* kCalloutFieldOk
    = "tests/unit/lua-tests/check_fixture/callout_field_ok.lua";
const char* kCalloutAliasOk
    = "tests/unit/lua-tests/check_fixture/callout_alias_ok.lua";
const char* kCalloutGhostBad
    = "tests/unit/lua-tests/check_fixture/callout_ghost_bad.lua";
const char* kCalloutCrossOk
    = "tests/unit/lua-tests/check_fixture/callout_cross_ok.lua";
const char* kCalloutCrossProvider
    = "tests/unit/lua-tests/check_fixture/callout_cross_provider.lua";
const char* kCalloutDynamicOk
    = "tests/unit/lua-tests/check_fixture/callout_dynamic_ok.lua";

// 防线7：random 语义（引擎全局 random 是 0 基、实参必须整型；
// math.random 是 1 基、单参 0 会回绕）
const char* kRandomIndexBad
    = "tests/unit/lua-tests/check_fixture/random_index_bad.lua";
const char* kRandomIndexOk
    = "tests/unit/lua-tests/check_fixture/random_index_ok.lua";
const char* kRandomLocalBad
    = "tests/unit/lua-tests/check_fixture/random_local_index_bad.lua";
const char* kRandomLocalOk
    = "tests/unit/lua-tests/check_fixture/random_local_index_ok.lua";
const char* kRandomLocalCrossFnOk
    = "tests/unit/lua-tests/check_fixture/random_local_cross_function_ok.lua";
const char* kRandomZeroBasedOk
    = "tests/unit/lua-tests/check_fixture/random_zero_based_table_ok.lua";
const char* kRandomFloatBad
    = "tests/unit/lua-tests/check_fixture/random_float_arg_bad.lua";
const char* kRandomFloatOk
    = "tests/unit/lua-tests/check_fixture/random_float_arg_ok.lua";
const char* kMathRandomZeroBad
    = "tests/unit/lua-tests/check_fixture/math_random_zero_bad.lua";
const char* kMathRandomNeverZeroBad
    = "tests/unit/lua-tests/check_fixture/math_random_never_zero_bad.lua";

// 防线9：字面量 require 绑定的局部与其模块导出面的对账
const char* kModuleFieldMissing
    = "tests/unit/lua-tests/check_fixture/module_field_missing_bad.lua";
const char* kModuleFieldOk
    = "tests/unit/lua-tests/check_fixture/module_field_ok.lua";
const char* kModuleFieldGuardOk
    = "tests/unit/lua-tests/check_fixture/module_field_guard_ok.lua";
const char* kModuleFieldTableBad
    = "tests/unit/lua-tests/check_fixture/module_field_table_bad.lua";
const char* kModuleFieldDynamicOk
    = "tests/unit/lua-tests/check_fixture/module_field_dynamic_ok.lua";
const char* kModuleFieldMetatableOk
    = "tests/unit/lua-tests/check_fixture/module_field_metatable_ok.lua";
const char* kModuleFieldNilOk
    = "tests/unit/lua-tests/check_fixture/module_field_nil_ok.lua";
const char* kModuleFieldDirectOk
    = "tests/unit/lua-tests/check_fixture/module_field_direct_ok.lua";
const char* kModuleFieldDirectBad
    = "tests/unit/lua-tests/check_fixture/module_field_direct_bad.lua";
const char* kModuleFieldReassignOk
    = "tests/unit/lua-tests/check_fixture/module_field_reassign_ok.lua";

// 基线文件由用例自己在仓库内 test_tmp\ 生成、析构即删（.gitignore 已排除）。
// 它只是测试的 arrange（生产接口按路径读基线，注释行/空行/CRLF/重复条目
// 计数都得真跑一遍），不是需要跟代码同步演进的数据 —— 提交进仓库只会变成
// 没有别的消费者的孤儿文件。
class ScopedBaseline {
public:
    ScopedBaseline(const char* name, const std::vector<std::string>& lines = {})
    {
        m_path = (test_tmp_dir() / name).string();
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

// 局部变量作用域：`local X = {...}` 构造器内部的 X 还没进作用域，读到的是
// 全局（nil）。数据集曾因 `return {` → `local X = {` 的改写静默引入这一类
// 运行期错误——静态必须报，否则同一个坑能再溜进来。
TEST_F(LuaCheckerRequireTest, LocalUsedInsideOwnInitializerFails)
{
    EXPECT_EQ(1, check("tests/unit/lua-tests/check_fixture/"
                       "local_init_scope_bad.lua"))
        << "构造器内的局部引用必须判违规";
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

// self 恒为引擎压入的对象 userdata（lua_bind_push_object → jianghu_object
// 元表），脚本表字段经 self:foo() 不可达：调用必然抛 nil value。
// 「某个脚本里定义过」就放行的旧口径，会把这类必炸调用整片放走。
TEST_F(LuaCheckerRequireTest, SelfScriptMethodFails)
{
    EXPECT_EQ(check(kSelfScriptMeth), 1)
        << "self 接收者只认对象元表，脚本表里定义过的函数不得放行";
}

// 标准库冒号白名单按名字给（math.random / string.format），不看接收者；
// self 是对象 userdata，上面同样没有这些字段。
TEST_F(LuaCheckerRequireTest, SelfStdlibMethodFails)
{
    EXPECT_EQ(check(kSelfStdlibMeth), 1)
        << "标准库方法名对 self 接收者不成立，不得被白名单放走";
}

// 上一条的反面：非 self 接收者可能是脚本表，仍按「任一脚本已定义」放行；
// 引擎对象方法对 self 照常通过。
TEST_F(LuaCheckerRequireTest, DefinedAndBoundMethodPass)
{
    EXPECT_EQ(check(kMethodOk), 0)
        << "脚本表接收者的方法与 self 上的引擎对象方法都不得被误报";
}

// ---------------- 防线 6：钩子冒烟（默认开） ----------------

// 默认必须是开的：检查的目的就是不让运行期错误出门，靠调用方记得加旗标
// 等于没有。
TEST(LuaCheckerOptions, SmokeOnByDefault)
{
    const CheckLuaOptions opts;
    EXPECT_TRUE(opts.smoke) << "防线6 必须默认开";
}

// 且 opts 为空（不传选项）时也照跑：传 nullptr 不能成为绕过冒烟的后门。
// 夹具 create 里直接抛错，静态三道防线都查不出，只有冒烟能把它翻成违规。
TEST_F(LuaCheckerRequireTest, SmokeRunsWithoutOptions)
{
    EXPECT_EQ(1, check("tests/unit/lua-tests/item/fixture_smoke_boom.lua"))
        << "默认开启的冒烟应把钩子抛错记成违规";
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

// ---------------- 防线 5：call_out 目标可见性（docs/guide/call_out.md 规则1）
// ----------------

// 顶层裸 function = 全局定义，不在返回表里 → 注册期两处表都查不到、必拒
TEST_F(LuaCheckerRequireTest, CalloutGlobalTargetFails)
{
    EXPECT_EQ(check(kCalloutGlobalBad), 1)
        << "裸全局定义的延迟函数不在返回表里，注册必拒 → 应判失败";
}

// local function 未导出 → 表里查不到（locals 不是表字段）
TEST_F(LuaCheckerRequireTest, CalloutLocalNotExportedFails)
{
    EXPECT_EQ(check(kCalloutLocalBad), 1)
        << "local 延迟函数未导出到返回表 → 应判失败";
}

// 目标函数何处都没有表字段定义
TEST_F(LuaCheckerRequireTest, CalloutGhostTargetFails)
{
    EXPECT_EQ(check(kCalloutGhostBad), 1) << "幽灵目标 → 应判失败";
}

// 正例：function 模块名.名字（表字段）
TEST_F(LuaCheckerRequireTest, CalloutTableFieldPasses)
{
    EXPECT_EQ(check(kCalloutFieldOk), 0) << "返回表字段形态应通过";
}

// 正例：local 函数 + 表内别名导出（数据集 chunjie_yh 形态）
TEST_F(LuaCheckerRequireTest, CalloutAliasExportPasses)
{
    EXPECT_EQ(check(kCalloutAliasOk), 0) << "别名导出后字段在表里，应通过";
}

// NOTE 形态：目标在另一脚本的表里（owner 契约）→ 不判失败
TEST_F(LuaCheckerRequireTest, CalloutCrossFileIsNoteNotFailure)
{
    const std::vector<std::string> files{kCalloutCrossOk,
                                         kCalloutCrossProvider};
    EXPECT_EQ(check_all(files), 0)
        << "跨文件 owner 契约形态只记 NOTE，不应判失败";
}

// 盲区形态：目标名是变量（非字面量）→ 静态不可判，跳过
TEST_F(LuaCheckerRequireTest, CalloutDynamicArgSkipped)
{
    EXPECT_EQ(check(kCalloutDynamicOk), 0)
        << "非字面量目标名静态不可判，不应误报失败";
}

// ---------------- 防线 7：random 语义 ----------------
//
// 引擎全局 random 走 C++ 绑定（src/util/Random.cpp 的 l_random，已废弃）：
// 返回 [0, n-1]、n<=0 返回 0、实参经 luaL_checkinteger 拒收非整型 float；
// Lua 标准库 math.random 返回 [1, n]。两套语义在语料里并存，混用即线上故障。

// 直接下标：random() 可返回 0 → t[0] 为 nil，索引即抛
TEST_F(LuaCheckerRequireTest, RandomIndexWithoutOffsetFails)
{
    EXPECT_EQ(check(kRandomIndexBad), 1)
        << "引擎 random() 是 0 基，t[random(n)] 会取到 t[0] = nil";
}

// 上一条的反面：+1 与 math.random 都落在 1..n，不得误报
TEST_F(LuaCheckerRequireTest, RandomIndexWithOffsetPasses)
{
    EXPECT_EQ(check(kRandomIndexOk), 0)
        << "random(n)+1 与 math.random(n) 都是合法下标写法";
}

// 先存局部再下标与直接下标同错（xiaoyao_dao query_action 的线上形态）
TEST_F(LuaCheckerRequireTest, RandomLocalIndexedFails)
{
    EXPECT_EQ(check(kRandomLocalBad), 1)
        << "裸 random() 结果存进 local 后下标，同样是 0 基取值";
}

TEST_F(LuaCheckerRequireTest, RandomLocalWithOffsetPasses)
{
    EXPECT_EQ(check(kRandomLocalOk), 0) << "赋值时就带上 +1 的局部下标不应误报";
}

// 跨函数的同名局部是两个变量（数据集 mfnpc_1：for 循环变量 i 撞了另一个函数里
// 的 random 局部）—— 不得把随机赋值串到别人头上，宁可漏、不可误报。
TEST_F(LuaCheckerRequireTest, RandomLocalCrossFunctionNotFlagged)
{
    EXPECT_EQ(check(kRandomLocalCrossFnOk), 0)
        << "使用点在赋值之前 / 中间隔着 function 的同名局部不得误报";
}

// 0 基表有构造证据（`for i = 0, n - 1` 的循环变量被当下标）时，0 基下标正是
// 它要的写法 —— 这类容器对 0 基 random 取值是正确的，报了就是假红。
TEST_F(LuaCheckerRequireTest, ZeroBasedTableIndexNotFlagged)
{
    EXPECT_EQ(check(kRandomZeroBasedOk), 0)
        << "按 0 基建表的容器用 0 基 random 下标不得误报";
}

// 实参：luaL_checkinteger 拒收无整数表示的 float，mp / 2 在奇数时必抛
TEST_F(LuaCheckerRequireTest, RandomFloatArgFails)
{
    EXPECT_EQ(check(kRandomFloatBad), 1)
        << "random(mp / 2) 传非整型 float 会抛 number has no integer "
           "representation";
}

// 上一条的反面：//、math.floor、裸整数都给出整型实参
TEST_F(LuaCheckerRequireTest, RandomIntegerArgPasses)
{
    EXPECT_EQ(check(kRandomFloatOk), 0)
        << "整除、math.floor 与整型实参都是合法写法，不得误报";
}

// math.random 单参 0 在 Lua 5.5 不报错，但空区间回绕成全量域大整数
TEST_F(LuaCheckerRequireTest, MathRandomZeroArgFails)
{
    EXPECT_EQ(check(kMathRandomZeroBad), 1)
        << "单参 math.random(0) 不是 0，是回绕出来的无意义大整数";
}

// math.random(n) >= 1，与 0 比较是恒假常量（0 基思维套在 1 基 API 上）
TEST_F(LuaCheckerRequireTest, MathRandomComparedToZeroFails)
{
    EXPECT_EQ(check(kMathRandomNeverZeroBad), 1)
        << "math.random 结果 >= 1，== 0 分支永不成立";
}

namespace {
// 教材根判定是路径作用域的：同一个调用在教材根内是违规、在根外不是。
// 夹具现写现删（test_tmp 已被 .gitignore 排除），不往仓库留错误脚本。
// 析构先删文件、再试删目录：remove 对非空目录是无操作，所以 test_tmp 根
// 绝不会被整目录删掉，只有我们自己建的那个空目录会消失。
class ScopedLuaFile {
public:
    ScopedLuaFile(const std::filesystem::path& dir, const std::string& name,
                  const std::string& content)
        : m_dir(dir)
    {
        std::error_code ec;
        std::filesystem::create_directories(m_dir, ec);
        m_path = (m_dir / name).string();
        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        out << content;
    }

    ~ScopedLuaFile()
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
        std::filesystem::remove(m_dir, ec);
    }

    ScopedLuaFile(const ScopedLuaFile&)            = delete;
    ScopedLuaFile& operator=(const ScopedLuaFile&) = delete;

    const char* c_str() const { return m_path.c_str(); }

private:
    std::filesystem::path m_dir;
    std::string m_path;
};
}  // namespace

TEST_F(LuaCheckerRequireTest, TextbookRootBansDeprecatedRandom)
{
    const ScopedLuaFile in(test_tmp_dir() / "lua-default",
                           "textbook_random_bad.lua",
                           "-- textbook must not model the deprecated API\n"
                           "local dmg = 20 + random(81)\n"
                           "return { dmg = dmg }\n");
    EXPECT_EQ(check(in.c_str()), 1)
        << "教材根（lua-default / lua-sample）里的裸 random() 必须判失败";
}

TEST_F(LuaCheckerRequireTest, NonTextbookRandomNotBanned)
{
    const ScopedLuaFile out(test_tmp_dir() / "nontextbook",
                            "nontextbook_random_ok.lua",
                            "-- 存量数据集仍允许 random()，禁用只作用于教材\n"
                            "local dmg = 20 + random(81)\n"
                            "return { dmg = dmg }\n");
    EXPECT_EQ(check(out.c_str()), 0)
        << "教材根之外的 random() 不属于禁用范围，不得误报";
}

// ---------------- 防线 9：模块表字段对账 ----------------
//
// 模块没导出的字段被调用时，运行期抛 "attempt to call a nil value
// (field 'x')"：数据集 mingjiao_db 把 on_mj_special 定义成全局函数，
// 35 处 mj_db.on_mj_special(...) 全部取到 nil，cast 当场失败。
// 本道只判"被调用、且全文件从未被读"的字段——先探测再调用的可选字段是
// 合法写法；模块导出面静态不可判定（动态键 / 元表 / 无返回表）时整模块不报。

TEST_F(LuaCheckerRequireTest, ModuleMissingFieldCallFails)
{
    EXPECT_EQ(check(kModuleFieldMissing), 1)
        << "模块未导出的字段被直接调用应判失败";
}

TEST_F(LuaCheckerRequireTest, ModuleExportedFieldCallPasses)
{
    EXPECT_EQ(check(kModuleFieldOk), 0) << "模块已导出的字段调用不得误报";
}

// 先探测后调用：字段在别处被读过，说明调用点是被守卫的可选 API
TEST_F(LuaCheckerRequireTest, ModuleProbedFieldCallPasses)
{
    EXPECT_EQ(check(kModuleFieldGuardOk), 0)
        << "被探测过的字段不得按缺失字段误报";
}

TEST_F(LuaCheckerRequireTest, ModuleTableCtorMissingFieldFails)
{
    EXPECT_EQ(check(kModuleFieldTableBad), 1)
        << "返回表构造器的模块，未导出的字段调用应判失败";
}

// 导出面静态不可判定的三种形态：动态键、元表 __index、返回值不是表
TEST_F(LuaCheckerRequireTest, ModuleDynamicSurfaceNotReported)
{
    EXPECT_EQ(check(kModuleFieldDynamicOk), 0)
        << "键在运行期才生成的模块不得误报";
}

TEST_F(LuaCheckerRequireTest, ModuleMetatableSurfaceNotReported)
{
    EXPECT_EQ(check(kModuleFieldMetatableOk), 0)
        << "字段可能来自 __index 的模块不得误报";
}

TEST_F(LuaCheckerRequireTest, ModuleNilReturnNotReported)
{
    EXPECT_EQ(check(kModuleFieldNilOk), 0)
        << "不返回表的模块没有导出面可对账，不得误报";
}

TEST_F(LuaCheckerRequireTest, ModuleDirectChainFieldPasses)
{
    EXPECT_EQ(check(kModuleFieldDirectOk), 0)
        << "链在 require() 上的已导出字段调用不得误报";
}

TEST_F(LuaCheckerRequireTest, ModuleDirectChainMissingFieldFails)
{
    EXPECT_EQ(check(kModuleFieldDirectBad), 1)
        << "链在 require() 上的未导出字段调用应判失败";
}

// 局部被重新赋值后不再指向模块，字段调用不属本道范围
TEST_F(LuaCheckerRequireTest, ModuleReassignedBindingNotTracked)
{
    EXPECT_EQ(check(kModuleFieldReassignOk), 0)
        << "已改指向的局部不得按模块字段误报";
}

// ---------------- 防线 8：整型实参 ----------------
//
// 绑定形参经 luaL_checkinteger / luaL_optinteger：实参含 '/' 会产出没有整数
// 表示的 float，运行期抛 "number has no integer representation"（数据集
// lingboweibu 的 start_busy 线上故障，报错形如 cast: Lua skill cast chan
// failed ... bad argument #1 to 'start_busy'）。直接传入、先存局部再传入、
// string.format 的 %d 转换符三条路径各钉一条；绑定清单另与源码对账。
class IntegerArgTest : public LuaCheckerRequireTest {
protected:
    // 夹具现写现删（test_tmp 不入库），返回失败文件数
    int check_snippet(const char* name, const std::string& body)
    {
        const ScopedLuaFile file(test_tmp_dir() / "int_arg", name, body);
        return check(file.c_str());
    }
};

// 直接把 '/' 传给整型绑定：start_busy 的第二参是 luaL_checkinteger
TEST_F(IntegerArgTest, DirectFloatArgFails)
{
    EXPECT_EQ(check_snippet("direct_float_bad.lua",
                            "local function cast(me, target)\n"
                            "    local skill = me:query_skill(\"demo\", 1)\n"
                            "    target:start_busy(skill / 50 + 2)\n"
                            "end\n"
                            "return {}\n"),
              1)
        << "start_busy(skill / 50 + 2) 在 skill 不整除时抛 number has no "
           "integer representation，必须判失败";
}

// 先存局部、再传给绑定（lingboweibu 线上故障的原始形态）
TEST_F(IntegerArgTest, IndirectFloatLocalFails)
{
    EXPECT_EQ(check_snippet("indirect_float_bad.lua",
                            "local function cast(me, target)\n"
                            "    local skill = me:query_skill(\"demo\", 1)\n"
                            "    local busy = skill / 50 + 2\n"
                            "    target:start_busy(busy)\n"
                            "end\n"
                            "return {}\n"),
              1)
        << "浮点局部传给整型绑定同样是运行期故障，必须判失败";
}

// 反面：整除、math.floor 包裹、字面量都是合法写法
TEST_F(IntegerArgTest, IntegerFormsPass)
{
    EXPECT_EQ(
        check_snippet("integer_forms_ok.lua",
                      "local function cast(me, target)\n"
                      "    local skill = me:query_skill(\"demo\", 1)\n"
                      "    target:start_busy(skill // 50 + 2)\n"
                      "    target:start_busy(math.floor(skill / 50) + 2)\n"
                      "    target:start_busy(3)\n"
                      "end\n"
                      "return {}\n"),
        0)
        << "'//'、math.floor 内的 '/' 与整型字面量不得误报";
}

// 取负写法：-x / 2 与 -(x // 2) 同义，'/' 仍是 float
TEST_F(IntegerArgTest, NegatedFloatArgFails)
{
    EXPECT_EQ(check_snippet("negated_float_bad.lua",
                            "local function hurt(me)\n"
                            "    me:add(\"hp\", -me:query(\"hp\") / 2)\n"
                            "end\n"
                            "return {}\n"),
              1)
        << "add 的第三参是整型，'-hp / 2' 必须判失败";
}

// string.format 的 %d 转换符按整型实参判：先存局部再格式化
TEST_F(IntegerArgTest, StringFormatIndirectFloatFails)
{
    EXPECT_EQ(check_snippet("string_format_indirect_bad.lua",
                            "local function report(me)\n"
                            "    local hp = me:query(\"hp\") / 2\n"
                            "    local msg = string.format(\"%d\", hp)\n"
                            "    return msg\n"
                            "end\n"
                            "return {}\n"),
              1)
        << "string.format(\"%d\", float) 抛 number has no integer "
           "representation，浮点局部必须判失败";
}

// string.format 的 %d 转换符按整型实参判：格式化时才做的除法
TEST_F(IntegerArgTest, StringFormatDirectFloatFails)
{
    EXPECT_EQ(
        check_snippet(
            "string_format_direct_bad.lua",
            "local function report(me)\n"
            "    local msg = string.format(\"%d\", me:query(\"hp\") / 2)\n"
            "    return msg\n"
            "end\n"
            "return {}\n"),
        1)
        << "string.format(\"%d\", x / 2) 的直接形态必须判失败";
}

// 反面：非整型绑定不得被牵连（set 的值参数是任意数值）
TEST_F(IntegerArgTest, NonIntegerBindingNotFlagged)
{
    EXPECT_EQ(check_snippet("non_integer_binding_ok.lua",
                            "local function store(me)\n"
                            "    me:set(\"count\", 1 / 2)\n"
                            "end\n"
                            "return {}\n"),
              0)
        << "只有整型形参的绑定才归本道，不得按绑定名一锅端";
}

// 赋值语句右侧的读取看到的是赋值前的值：只报语句之后的那次使用
TEST_F(IntegerArgTest, UseInsideAssignmentNotFlagged)
{
    EXPECT_EQ(check_snippet("use_in_rhs_ok.lua",
                            "local function roll(me, target)\n"
                            "    local damage = 10\n"
                            "    damage = (damage + random(damage)) / 2\n"
                            "    target:receive_damage(\"hp\", damage)\n"
                            "end\n"
                            "return {}\n"),
              1)
        << "语句右侧的 damage 还是赋值前的整型，只应报语句之后的那次使用";
}

// 同名局部跨 function 是两个变量（与 R1 同口径：宁可漏、不可误报）
TEST_F(IntegerArgTest, CrossFunctionSameNameNotFlagged)
{
    EXPECT_EQ(check_snippet("cross_function_ok.lua",
                            "busy = 0\n"
                            "local function setup(me)\n"
                            "    busy = me:query(\"hp\") / 2\n"
                            "end\n"
                            "local function cast(me, target)\n"
                            "    target:start_busy(busy)\n"
                            "end\n"
                            "return {}\n"),
              0)
        << "赋值与使用隔着 function 的同名变量不得串到一起";
}

// ---------------- 防线 8：绑定清单与源码对账 ----------------
//
// kIntegerArgSpecs 是防线8 的判断依据，手抄必然漂移：源码新增整型绑定而未
// 登记，就是"静默漏检"——本用例从 src/ 反查（LuaBind.cpp 的绑定表给出名字，
// 各 l_* 实现体里的 luaL_checkinteger/luaL_optinteger 给出位置），与清单逐条
// 比对，两个方向的差异都判失败。
namespace {
// 分片有独立工作目录（test_tmp/shards/sN，只有夹具树），源码得自己往上找：
// 从当前目录逐级上溯，直到同时看得到 src/ 与 tests/ 的那一层。
std::filesystem::path repo_root()
{
    std::error_code ec;
    std::filesystem::path p = std::filesystem::current_path(ec);
    if (ec) return {};
    for (int i = 0; i < 8 && !p.empty(); ++i) {
        if (std::filesystem::exists(p / "src" / "lua" / "LuaBind.cpp", ec)) {
            return p;
        }
        const std::filesystem::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return {};
}

std::string read_source_file(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

// 从 `int l_xxx(lua_State...` 的声明处取函数体（花括号配对，跳过注释与
// 字符串/字符字面量）；形参表之后是 ';' 的是声明，返回空串。
std::string function_body(const std::string& src, size_t header_pos)
{
    size_t open = header_pos;
    while (open < src.size() && src[open] != '(') ++open;
    if (open >= src.size()) return {};
    int paren = 0;
    size_t j  = open;
    for (; j < src.size(); ++j) {
        if (src[j] == '(')
            ++paren;
        else if (src[j] == ')' && --paren == 0) {
            ++j;
            break;
        }
    }
    if (paren != 0) return {};
    if (src.substr(open, j - open).find("lua_State") == std::string::npos) {
        return {};  // 不是 Lua 绑定实现
    }
    // 形参表之后只许空白与注释，然后才是定义体 '{'
    while (j < src.size()) {
        if (isspace((unsigned char)src[j])) {
            ++j;
        }
        else if (src[j] == '/' && j + 1 < src.size() && src[j + 1] == '/') {
            const size_t nl = src.find('\n', j);
            if (nl == std::string::npos) return {};
            j = nl + 1;
        }
        else if (src[j] == '/' && j + 1 < src.size() && src[j + 1] == '*') {
            const size_t close = src.find("*/", j + 2);
            if (close == std::string::npos) return {};
            j = close + 2;
        }
        else {
            break;
        }
    }
    if (j >= src.size() || src[j] != '{') return {};  // 声明而非定义

    int depth = 0;
    for (size_t k = j; k < src.size(); ++k) {
        const char c = src[k];
        if (c == '/' && k + 1 < src.size() && src[k + 1] == '/') {
            const size_t nl = src.find('\n', k);
            if (nl == std::string::npos) return {};
            k = nl;
            continue;
        }
        if (c == '/' && k + 1 < src.size() && src[k + 1] == '*') {
            const size_t close = src.find("*/", k + 2);
            if (close == std::string::npos) return {};
            k = close + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char q = c;
            ++k;
            while (k < src.size() && src[k] != q) {
                if (src[k] == '\\') ++k;
                ++k;
            }
            continue;
        }
        if (c == '{') {
            ++depth;
        }
        else if (c == '}') {
            if (--depth == 0) return src.substr(j, k - j + 1);
        }
    }
    return {};
}

// 函数体里 luaL_checkinteger / luaL_optinteger 的字面量 L 位置
std::set<int> integer_positions(const std::string& body)
{
    std::set<int> out;
    const char* keys[] = {"luaL_checkinteger", "luaL_optinteger"};
    for (const char* key : keys) {
        const size_t klen = strlen(key);
        size_t p          = 0;
        while ((p = body.find(key, p)) != std::string::npos) {
            size_t j = p + klen;
            while (j < body.size() && isspace((unsigned char)body[j])) ++j;
            if (j < body.size() && body[j] == '(') {
                ++j;
                while (j < body.size() && isspace((unsigned char)body[j])) ++j;
                if (j + 1 < body.size() && body[j] == 'L'
                    && !isalnum((unsigned char)body[j + 1])
                    && body[j + 1] != '_') {
                    ++j;
                    while (j < body.size() && isspace((unsigned char)body[j])) {
                        ++j;
                    }
                    if (j < body.size() && body[j] == ',') {
                        ++j;
                        while (j < body.size()
                               && isspace((unsigned char)body[j])) {
                            ++j;
                        }
                        if (j < body.size()
                            && isdigit((unsigned char)body[j])) {
                            out.insert(atoi(body.c_str() + j));
                        }
                    }
                }
            }
            p += klen;
        }
    }
    return out;
}
}  // namespace

TEST(IntegerArgBindings, ReconcileWithSource)
{
    const std::filesystem::path root = repo_root();
    ASSERT_FALSE(root.empty())
        << "从当前目录上溯找不到仓库根（src/lua/LuaBind.cpp）";

    // 1) 绑定表：绑定名 → 实现符号（object_methods + global_funcs 两张表）
    const std::string bind_src
        = read_source_file(root / "src" / "lua" / "LuaBind.cpp");
    ASSERT_FALSE(bind_src.empty())
        << "读不到 src/lua/LuaBind.cpp（测试必须在仓库树内运行）";
    std::map<std::string, std::string> name_to_sym;
    size_t p = 0;
    while ((p = bind_src.find(", l_", p)) != std::string::npos) {
        const size_t brace   = bind_src.rfind("{\"", p);
        const size_t quote   = (brace == std::string::npos)
                                   ? std::string::npos
                                   : bind_src.find('"', brace + 2);
        const size_t sym_end = bind_src.find('}', p);
        if (brace != std::string::npos && quote != std::string::npos
            && quote < p && p - brace < 64 && sym_end != std::string::npos) {
            name_to_sym[bind_src.substr(brace + 2, quote - brace - 2)]
                = bind_src.substr(p + 2, sym_end - p - 2);
        }
        p += 4;
    }
    ASSERT_GT(name_to_sym.size(), 100u)
        << "LuaBind.cpp 的绑定表解析结果异常（应有 170+ 条）";

    // 2) 各 l_* 实现体 → 整型位置
    std::map<std::string, std::set<int>> impl_positions;
    for (auto it = std::filesystem::recursive_directory_iterator(root / "src");
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        const std::string ext = it->path().extension().string();
        if (ext != ".cpp" && ext != ".h") continue;
        const std::string text = read_source_file(it->path());
        size_t q               = 0;
        while ((q = text.find("int l_", q)) != std::string::npos) {
            size_t e = q + 6;
            while (e < text.size()
                   && (isalnum((unsigned char)text[e]) || text[e] == '_')) {
                ++e;
            }
            size_t s = e;
            while (s < text.size()
                   && (text[s] == ' ' || text[s] == '\t' || text[s] == '\r'
                       || text[s] == '\n')) {
                ++s;
            }
            if (s < text.size() && text[s] == '('
                && text.compare(s + 1, 9, "lua_State") == 0) {
                // q 指向 "int "，l_ 从 q + 4 起
                const std::string sym = text.substr(q + 4, e - q - 4);
                if (impl_positions.find(sym) == impl_positions.end()) {
                    const std::string body = function_body(text, q);
                    if (!body.empty())
                        impl_positions[sym] = integer_positions(body);
                }
            }
            q = e;
        }
    }

    // 3) 源码口径：绑定到、且实现体里确实有整型形参的
    std::map<std::string, std::set<int>> expected;
    for (const auto& kv : name_to_sym) {
        auto it = impl_positions.find(kv.second);
        if (it == impl_positions.end() || it->second.empty()) continue;
        expected[kv.first] = it->second;
    }
    expected.erase("random");  // random 归防线7（R2），不入本表
    ASSERT_GT(expected.size(), 50u)
        << "从 src/ 反查到的整型绑定过少，解析逻辑或源码结构变了";

    // 4) 清单口径：解析 kIntegerArgSpecs
    const std::string checker_src
        = read_source_file(root / "src" / "lua" / "LuaSyntaxChecker.cpp");
    ASSERT_FALSE(checker_src.empty()) << "读不到 LuaSyntaxChecker.cpp";
    const size_t table_begin = checker_src.find("kIntegerArgSpecs[] = {");
    ASSERT_NE(table_begin, std::string::npos)
        << "找不到 kIntegerArgSpecs（防线8 清单位置变了）";
    const size_t table_end = checker_src.find("};", table_begin);
    ASSERT_NE(table_end, std::string::npos) << "kIntegerArgSpecs 没有收尾";

    std::map<std::string, std::set<int>> listed;
    size_t r = table_begin;
    while (r < table_end) {
        const size_t entry = checker_src.find("{\"", r);
        if (entry == std::string::npos || entry >= table_end) break;
        const size_t name_begin = entry + 2;
        const size_t name_end   = checker_src.find('"', name_begin);
        if (name_end == std::string::npos || name_end >= table_end) break;
        // 条目形如 {"name", "1,2"}：位置串在第二对引号里
        const size_t pos_begin = checker_src.find('"', name_end + 1);
        if (pos_begin == std::string::npos || pos_begin >= table_end) break;
        const size_t pos_end = checker_src.find('"', pos_begin + 1);
        if (pos_end == std::string::npos || pos_end >= table_end) break;

        const std::string name
            = checker_src.substr(name_begin, name_end - name_begin);
        const std::string pos_text
            = checker_src.substr(pos_begin + 1, pos_end - pos_begin - 1);
        std::set<int> positions;
        size_t s2 = 0;
        while (s2 < pos_text.size()) {
            positions.insert(atoi(pos_text.c_str() + s2));
            while (s2 < pos_text.size() && pos_text[s2] != ',') ++s2;
            if (s2 < pos_text.size()) ++s2;
        }
        listed[name] = positions;
        r            = pos_end + 1;
    }
    ASSERT_GT(listed.size(), 50u) << "kIntegerArgSpecs 解析结果异常";

    EXPECT_EQ(expected, listed)
        << "kIntegerArgSpecs 必须与 src/ 逐条对账：新增整型绑定（或改了整型"
           "形参位置）而未登记 → 静默漏检；绑定已改非整型而未摘除 → 误报";
}
