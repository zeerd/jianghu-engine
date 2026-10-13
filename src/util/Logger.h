/**
 * @file Logger.h
 * @brief 分级日志系统，支持 TRACE/DEBUG/INFO/WARN/ERROR/FATAL 六个级别。
 */
//---------------------------------------------------------

#pragma once

#include <stdarg.h>
#include <string>

// 日志级别枚举（数值越小越详细）
enum LogLevel {
    LOG_LVL_TRACE = 0,
    LOG_LVL_DEBUG = 1,
    LOG_LVL_INFO  = 2,
    LOG_LVL_WARN  = 3,
    LOG_LVL_ERROR = 4,
    LOG_LVL_FATAL = 5
};

// 获取 / 设置运行时日志级别
// 低于当前级别的日志将被丢弃（LOG_DEBUG / LOG_TRACE 有编译期短路宏保护）
void logger_set_level(LogLevel level);
LogLevel logger_get_level();

// 获取 / 设置日志文件存放目录（不含末尾分隔符）
// 默认值为 "log"，可通过配置文件 [log] path=xxx 覆盖
void logger_set_path(const char* path);
const std::string& logger_get_path();

// 获取 / 设置本次运行的日志标识（run id）
// 文件名由 <path>/M-D.txt 变为 <path>/M-D-<runid>.txt，
// 用于区分同一天内多次运行产生的日志（每次单元测试运行写入独立文件）。
// 传入空串或 nullptr 表示不带 run id，恢复默认文件名。
void logger_set_run_id(const char* id);
const std::string& logger_get_run_id();

// 生成新的 run id，格式 yyyyMMdd-HHmmss-<进程ID>
std::string logger_new_run_id();

// 拼接完整日志文件路径：<path>/<月>-<日>[-<runid>].txt（月、日不补零）
std::string logger_log_file_name(int month, int day);

// 字符串转日志级别（大小写不敏感，支持 TRACE/DEBUG/INFO/WARN/ERROR/FATAL）
// 无法识别时返回 LOG_LVL_TRACE
LogLevel logger_level_from_str(const char* str);

// 核心日志函数（带级别参数，格式化输出到日志文件）
void log_message(LogLevel level, const char* fmt, ...);

// 便捷宏 -------------------------------------------------------
// TRACE / DEBUG：编译期短路 —— release 构建中可被优化器消除
// INFO / WARN / ERROR / FATAL：始终编译，仅运行时过滤

#define LOG_TRACE(fmt, ...)                                                \
    do {                                                                   \
        if (logger_get_level() <= LOG_LVL_TRACE)                           \
            log_message(LOG_LVL_TRACE, "%s: " fmt, __func__, __VA_ARGS__); \
    } while (0)

#define LOG_DEBUG(fmt, ...)                                                \
    do {                                                                   \
        if (logger_get_level() <= LOG_LVL_DEBUG)                           \
            log_message(LOG_LVL_DEBUG, "%s: " fmt, __func__, __VA_ARGS__); \
    } while (0)

#define LOG_INFO(fmt, ...) \
    log_message(LOG_LVL_INFO, "%s: " fmt, __func__, __VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    log_message(LOG_LVL_WARN, "%s: " fmt, __func__, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    log_message(LOG_LVL_ERROR, "%s: " fmt, __func__, __VA_ARGS__)
#define LOG_FATAL(fmt, ...) \
    log_message(LOG_LVL_FATAL, "%s: " fmt, __func__, __VA_ARGS__)
