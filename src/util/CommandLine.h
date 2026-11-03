#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

/**
 * @file CommandLine.h
 * @brief 极简命令行参数解析（服务器进程入口使用）。
 */

#include <stddef.h>

/**
 * @brief 从命令行串解析 `--<name> <value>` / `--<name>=<value>`。
 *
 * 命令行选项的通用取值入口：服务器进程的每个启动选项各写一遍扫描循环会互相
 * 漂移，故收敛为一处；需要回落值的选项（如 --config）在此之上包一层。
 *
 * 容忍注入式启动器给每个参数重新加引号的形式（如 OpenCppCoverage 传给子进程的
 * 是 `"--config" "C:\path\config.ini"`），也容忍值本身带空格时的引号包裹。
 * 选项名以空白或引号结尾才算命中，`--configure` 不会被当成 `--config`。
 *
 * @param cmd_line 命令行串（不含程序名）
 * @param name     选项名，不含前导 `--`
 * @param out      输出缓冲，至少 out_size 字节；未取到值时不改动
 * @param out_size 输出缓冲大小
 * @return true 表示取到非空值（已写入 out）；false 表示未提供、无值或缓冲不足
 */
bool parse_option_value(const char* cmd_line, const char* name, char* out,
                        size_t out_size);

/**
 * @brief 从命令行串解析 `--config <path>` / `--config=<path>`。
 *
 * 供服务器启动时指定配置文件（集成测试据此把专用配置留在 tests/integration/，
 * 不必临时改写 data/config.ini）。未提供或无法解析时返回 default_path。
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
