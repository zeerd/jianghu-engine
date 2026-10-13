/**
 * @file SqlEscape.h
 * @brief SQL 单引号字面量的安全包裹。
 *
 * 存盘路径把 get_buffer() 产出的 JSON 文本直接写进 MySQL/SQLite 的单引号
 * 字面量。JSON 会大量出现反斜杠（" -> \"、\ -> \\，以及 GBK 尾字节 0x5C），
 * 而 MySQL 默认把字面量内的反斜杠当作转义消耗，导致 JSON 字节被破坏或错位。
 *
 * 对策（见 init_sql_server）：连接建立后设置
 *   set session sql_mode='NO_BACKSLASH_ESCAPES'
 * 使反斜杠退化为普通字符；此后字面量内唯一需要处理的字符是单引号本身。
 * SQLite 本就不做反斜杠转义，与该模式行为一致，因此同一份转义对两种后端均正确。
 */

#pragma once

#include <string>

/**
 * @brief 将任意文本安全地包裹进单引号 SQL 字面量。
 *
 * 仅翻倍单引号（' -> ''）。调用方负责补上外层单引号，例如：
 *   "insert t(obj_data) values('" + sql_escape_literal(data) + "')"
 *
 * @param text 待转义文本，nullptr 视为空串。
 * @return 转义后的副本（不含外层单引号）。
 */
std::string sql_escape_literal(const char* text);
