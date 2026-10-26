/**
 * @file LuaSyntaxChecker.h
 * @brief Lua 脚本静态校验工具（--check-lua 模式）。
 */
//---------------------------------------------------------

#pragma once

#include <string>
#include <vector>

struct lua_State;

// 逐文件校验的可选项；传 nullptr 即全部取默认（无基线豁免）。
struct CheckLuaOptions {
    // 基线文件：其中列出的违规被豁免，只拦新增条目。nullptr = 不豁免。
    const char* baseline_path = nullptr;
    // 路径前缀（即 --dir 的目录）：用于把绝对路径折成相对路径写入基线，
    // 使基线不随机器/调用方式变化。nullptr = 原样使用传入路径。
    const char* path_prefix = nullptr;
    // 定义补充扫描根：跨文件定义（constants.lua 的图标常量、damage_msg.lua
    // 的全局函数等）不在待检集内时，单文件模式会把它们误报成"未绑定全局"。
    // 这里列出的目录只收集定义、不产违规；已扫描过的文件自动跳过。
    std::vector<std::string> definition_roots;
    // true = 把本次发现的全部违规写入基线文件（存量清理的引导手段）。
    // 该模式下违规本身不判失败；但语法错误、文件读不到、require 解析失败
    // 仍照常报 FAIL 并计入返回值——写基线不能把一棵语法写坏的树洗绿。
    bool write_baseline = false;
};

// Lua 语法校验主函数
// cmdLine: 原始命令行，格式:
//   --check-lua [--config <ini>] [--dir <path>]
//               [--baseline <file>] [--write-baseline] <file.lua> ...
//
// 未指定 --config 时使用默认路径：
//   data/config.ini（服务器配置）
//
// --dir <path>       : 递归扫描目录下所有 .lua 文件；目录不存在、或目录里
//                      一个 .lua 都没有，都按失败退出（不许假绿）
// --baseline <file>  : 指定豁免基线；缺省时取 <dir>/check_lua_baseline.txt
// --write-baseline   : 把发现的违规写入基线（违规不判失败；语法/require
// 错误仍失败）
//
// 校验内容（四道防线，详见 LuaSyntaxChecker.cpp 文件头）：
//   1. 语法          luaL_loadbuffer
//   2. require 闭包  字面量 require("...") 须能被 package.searchers[2] 解析
//   3. 未绑定全局    脚本读到的全局必须在 _G 或由某个脚本定义
//   4. 未绑定方法    x:foo() 的 foo 必须在对象元表、标准库，或由某个脚本定义
//
// 返回: 0=全部通过, 1=有失败
int run_lua_checker(const char* cmdLine);

// 逐文件校验，打印逐文件结果与汇总，返回失败文件数。
// L 须为引擎状态；本函数会自持调用 lua_bind_init(L)（幂等），因为第 3、4 道
// 检测必须拿到绑定表才有判断依据，不依赖调用方是否已初始化。不改变引擎生命周期。
int check_lua_files(lua_State* L, const std::vector<std::string>& files,
                    const CheckLuaOptions* opts = nullptr);
