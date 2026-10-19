#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

/**
 * @file CommandLine.h
 * @brief 极简命令行参数解析（服务器进程入口使用）。
 */

#include <stddef.h>

/**
 * @brief 从命令行串解析 `--config <path>` / `--config=<path>`。
 *
 * 供服务器启动时指定配置文件（集成测试据此把专用配置留在 tests/integration/，
 * 不必临时改写 data/config.ini）。未提供或无法解析时返回 default_path。
 *
 * 容忍注入式启动器给每个参数重新加引号的形式（如 OpenCppCoverage 传给子进程的
 * 是 `"--config" "C:\path\config.ini"`），也容忍值本身带空格时的引号包裹。
 *
 * @param cmd_line     命令行串（不含程序名）
 * @param out          输出缓冲，至少 out_size 字节
 * @param out_size     输出缓冲大小
 * @param default_path 未指定时的默认配置文件路径
 * @return 解析出的路径（写入 out）或 default_path
 */
const char* parse_config_path(const char* cmd_line, char* out, size_t out_size,
                              const char* default_path = "data/config.ini");

#endif  // COMMAND_LINE_H
