/**
 * @file LuaSyntaxChecker.h
 * @brief Lua 脚本语法校验工具（--check-lua 模式）。
 */
//---------------------------------------------------------

#pragma once

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
// 返回: 0=全部通过, 1=有语法错误
int run_lua_checker(const char* cmdLine);
