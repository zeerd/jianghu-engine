/**
 * @file CommandLine.cpp
 * @brief 极简命令行参数解析实现（见 CommandLine.h）。
 */

#include "CommandLine.h"

#include <string.h>
#include <string>

bool parse_option_value(const char* cmd_line, const char* name, char* out,
                        size_t out_size)
{
    if (!cmd_line || !name || !name[0] || !out || out_size == 0) return false;

    // 搜索串为 "--" + 选项名，命中位置必然带完整前缀
    const std::string opt = std::string("--") + name;

    const char* p = cmd_line;
    while ((p = strstr(p, opt.c_str())) != NULL) {
        const char* q = p + opt.size();

        if (*q == '=') {
            // --<name>=<value>
            ++q;
        }
        else {
            // 选项名自身可能被引号包裹（注入式启动器会产生 "--name" "value"）
            if (*q == '"') {
                ++q;
            }
            else if (*q != ' ' && *q != '\t') {
                // 其后必须跟分隔符才有值，否则是 --nameable 这类同前缀参数
                p = q;
                continue;
            }
        }

        // 跳过选项与值之间的空白
        while (*q == ' ' || *q == '\t') ++q;

        // 取值：引号包裹时读到闭合引号（值可含空格），否则读到空白或引号
        const char* start = q;
        if (*q == '"') {
            start = ++q;
            while (*q && *q != '"') ++q;
        }
        else {
            while (*q && *q != ' ' && *q != '\t' && *q != '"') ++q;
        }

        size_t len = (size_t)(q - start);
        if (len > 0 && len < out_size) {
            memcpy(out, start, len);
            out[len] = '\0';
            return true;
        }
        p = (q > p) ? q : p + 1;
    }
    return false;
}

const char* parse_config_path(const char* cmd_line, char* out, size_t out_size,
                              const char* default_path)
{
    if (!default_path) default_path = "data/config.ini";
    // 未命中/无值/缓冲不足时 out 不被改动，回落 default_path
    if (parse_option_value(cmd_line, "config", out, out_size)) return out;
    return default_path;
}
