/**
 * @file SqlEscape.cpp
 * @brief sql_escape_literal 实现。
 */

#include "SqlEscape.h"

std::string sql_escape_literal(const char* text)
{
    std::string out;
    if (!text) return out;

    // 连接已设 NO_BACKSLASH_ESCAPES，反斜杠是普通字符，无需处理；
    // 字面量以单引号定界，只需把内部的单引号翻倍。
    for (const char* p = text; *p; ++p) {
        if (*p == '\'') out += "'";
        out += *p;
    }
    return out;
}
