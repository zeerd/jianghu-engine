// test_log_hits.h — 测试共用：当前运行日志的读取与 needle 命中计数
//
// 日志是"只产生日志副作用"的可观察面（如脚本加载失败的 BACKTRACE 回溯、
// 无谓脚本加载的 ERROR 行、回溯打出的真实行号），用法：断言前后各
// count_log_hits 一次比较差值，或 read_log_content 后 find 关键子串。
// 日志文件 = <log 路径>/M-D[-runid].txt，runid 由 TestEnvironment 统一设置。
#pragma once

#include <ctime>
#include <fstream>
#include <iterator>
#include <string>

#include "Logger.h"

inline std::string read_log_content()
{
    time_t t         = time(NULL);
    struct tm* now   = localtime(&t);
    std::string path = logger_log_file_name(now->tm_mon + 1, now->tm_mday);

    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::string();
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

inline int count_log_hits(const char* needle)
{
    const std::string content = read_log_content();
    int count                 = 0;
    for (size_t pos = content.find(needle); pos != std::string::npos;
         pos        = content.find(needle, pos + 1)) {
        ++count;
    }
    return count;
}
