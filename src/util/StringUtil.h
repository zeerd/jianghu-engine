/**
 * @file StringUtil.h
 * @brief 通用字符串工具函数（分析/格式化/编码/校验）。
 */
#pragma once

// --- 字符串分析 ---
void analyse_string(char* Comm, char* arg, int len);
char* get_value(const char* cmd_str, const char* keyword, char* pValue,
                int nValueLen);

// --- 格式化 ---
char* snprintf(char* buf, int len, const char* fmt, ...);

// --- 编码 ---
char* decode_str(char* msg);

// --- 校验 ---
int valid_string(const char* msg);
