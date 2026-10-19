/**
 * @file LuaSyntaxChecker.h
 * @brief Lua 脚本语法校验工具（--check-lua 模式）。
 */
//---------------------------------------------------------

#pragma once

#include <string>
#include <vector>

struct lua_State;

// Lua 语法校验主函数
// cmdLine: 原始命令行，格式:
//   --check-lua [--config <ini>] [--dir <path>] <file.lua> ...
//
// 未指定 --config 时使用默认路径：
//   data/config.ini（服务器配置）
//
// --dir <path> : 递归扫描目录下所有 .lua 文件
//
// 校验内容：语法（luaL_loadbuffer）+ require 闭包（字面量 require("...")
// 必须能被 package.searchers[2] 解析到存在的脚本文件）。
//
// 返回: 0=全部通过, 1=有失败
int run_lua_checker(const char* cmdLine);

// 逐文件校验（语法 + require 闭包），打印逐文件结果与汇总，返回失败文件数。
// L 须为已初始化的引擎状态（CLuaEngine::init 之后）；不改变引擎生命周期。
int check_lua_files(lua_State* L, const std::vector<std::string>& files);
