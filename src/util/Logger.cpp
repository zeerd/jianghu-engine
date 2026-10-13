/** @file Logger.cpp @brief 分级日志系统实现 输出格式： [INFO ] (2026-08-21
 * 14:30:15) 房间 dali 重置 [ERROR] (2026-08-21 14:30:16) 缓冲越界: 用户 12345
 * 日志文件：log/M-D.txt（按日分文件，设置 run id 后为 M-D-<runid>.txt） */

#include <windows.h>

#include "Logger.h"
#include "Encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 级别名称（6 字符宽度，对齐输出）
static const char* s_level_names[] = {
    "TRACE ",  // 0
    "DEBUG ",  // 1
    "INFO  ",  // 2
    "WARN  ",  // 3
    "ERROR ",  // 4
    "FATAL "   // 5
};

// 默认日志级别
static LogLevel s_current_level = LOG_LVL_TRACE;

// 默认日志目录
static std::string s_log_path = "log";

// 本次运行的日志标识（空表示不参与文件名）
static std::string s_run_id;

// ----------------------------------------------------------------
// 公开 API
// ----------------------------------------------------------------

void logger_set_level(LogLevel level)
{
    if (level < LOG_LVL_TRACE) level = LOG_LVL_TRACE;
    if (level > LOG_LVL_FATAL) level = LOG_LVL_FATAL;
    s_current_level = level;
}

LogLevel logger_get_level() { return s_current_level; }

void logger_set_path(const char* path)
{
    s_log_path = (path && path[0]) ? path : "log";
}

const std::string& logger_get_path() { return s_log_path; }

void logger_set_run_id(const char* id) { s_run_id = (id && id[0]) ? id : ""; }

const std::string& logger_get_run_id() { return s_run_id; }

// logger_new_run_id — 生成 "yyyyMMdd-HHmmss-<进程ID>" 形式的运行标识
std::string logger_new_run_id()
{
    time_t t       = time(NULL);
    struct tm* now = localtime(&t);

    char buf[64];
    snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d-%lu",
             now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
             now->tm_min, now->tm_sec,
             static_cast<unsigned long>(GetCurrentProcessId()));
    return buf;
}

// logger_log_file_name — run id 非空时在日期后追加 "-<runid>"
std::string logger_log_file_name(int month, int day)
{
    std::string fname
        = s_log_path + '/' + std::to_string(month) + '-' + std::to_string(day);
    if (!s_run_id.empty()) fname += '-' + s_run_id;
    return fname + ".txt";
}

// logger_level_from_str — 将字符串转换为 LogLevel 枚举
// 支持大小写不敏感，无法识别时返回 LOG_LVL_TRACE
LogLevel logger_level_from_str(const char* str)
{
    if (!str || !str[0]) return LOG_LVL_TRACE;

    if (_stricmp(str, "trace") == 0) return LOG_LVL_TRACE;
    if (_stricmp(str, "debug") == 0) return LOG_LVL_DEBUG;
    if (_stricmp(str, "info") == 0) return LOG_LVL_INFO;
    if (_stricmp(str, "warn") == 0) return LOG_LVL_WARN;
    if (_stricmp(str, "error") == 0) return LOG_LVL_ERROR;
    if (_stricmp(str, "fatal") == 0) return LOG_LVL_FATAL;

    return LOG_LVL_TRACE;
}

// ----------------------------------------------------------------
// log_message — 核心日志函数
// ----------------------------------------------------------------
// 1. 格式化用户消息到临时缓冲区
// 2. 拼接 [LEVEL] 前缀和时间戳
// 3. 写入 <log_path>/M-D[-<runid>].txt
// ----------------------------------------------------------------

void log_message(LogLevel level, const char* fmt, ...)
{
    // 级别过滤（INFO 及以下的宏直接调用此函数，需在此兜底）
    if (level < s_current_level) return;

    // ---- 1. 格式化用户消息 ----
    char msg_buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, ap);
    va_end(ap);

    // ---- 2. 构造完整日志行 ----
    //  目标格式：[LEVEL ] (YYYY-MM-DD HH:MM:SS) <message>\n
    //  粗略估算：6 + 2 + 24 + 2 + 8192 + 1 ≈ 8227，使用 8384 缓冲区
    char line[8384];

    // 获取当前时间
    time_t t       = time(NULL);
    struct tm* now = localtime(&t);

    // 拼接
    int n = snprintf(
        line, sizeof(line), "[%s] (%04d-%02d-%02d %02d:%02d:%02d) %s\n",
        (level >= 0 && level <= LOG_LVL_FATAL) ? s_level_names[level]
                                               : "???    ",
        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
        now->tm_min, now->tm_sec, msg_buf);

    if (n <= 0) return;

    // ---- 3. 写入日志文件 ----
    // 文件名：<log_path>/M-D[-<runid>].txt
    // 确保日志目录存在，不存在则创建
    CreateDirectoryA(s_log_path.c_str(), NULL);

    std::string fname = logger_log_file_name(now->tm_mon + 1, now->tm_mday);

    FILE* fp = fopen(fname.c_str(), "at");
    if (fp) {
        // 将 GBK 日志内容转为 UTF-8 输出（纯 ASCII 行 GbkToUtf8 会原样返回）
        std::string utf8_line = gbk_to_utf8(line);
        fputs(!utf8_line.empty() ? utf8_line.c_str() : line, fp);
        fclose(fp);
    }
}

//---------------------------------------------------------
// [LuaBind] 级别感知的日志绑定：log_trace/log_debug/log_info/
// log_warn/log_error/log_fatal(fmt, ...)，多参时按 string.format 语义
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

static int l_log_at_level(lua_State* L, LogLevel level)
{
    const char* msg = luaL_checkstring(L, 1);
    int n           = lua_gettop(L);
    if (n > 1) {
        // string.format(fmt, ...)
        lua_getglobal(L, "string");
        lua_getfield(L, -1, "format");
        lua_pushvalue(L, 1);
        for (int i = 2; i <= n; i++) lua_pushvalue(L, i);
        lua_call(L, n, 1);
        const char* fmt = lua_tostring(L, -1);
        log_message(level, "%s", fmt ? fmt : "");
        lua_pop(L, 2);
    }
    else {
        log_message(level, "%s", msg ? msg : "");
    }
    return 0;
}

int l_log_trace(lua_State* L) { return l_log_at_level(L, LOG_LVL_TRACE); }
int l_log_debug(lua_State* L) { return l_log_at_level(L, LOG_LVL_DEBUG); }
int l_log_info(lua_State* L) { return l_log_at_level(L, LOG_LVL_INFO); }
int l_log_warn(lua_State* L) { return l_log_at_level(L, LOG_LVL_WARN); }
int l_log_error(lua_State* L) { return l_log_at_level(L, LOG_LVL_ERROR); }
int l_log_fatal(lua_State* L) { return l_log_at_level(L, LOG_LVL_FATAL); }
