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
// 通讯帧 XOR 自逆编解码（收发两侧同一变换、同一密钥）。密钥取
// [security] comm_key，缺省或留空回退内置默认密钥。
char* decode_str(char* msg);

// --- 校验 ---
int valid_string(const char* msg);

// --- 密码规范化 ---
// 密码契约：密码不含单引号。登录口把整个登录串的单引号剥除后才比对
// （Login.cpp replace_string），因此所有写入/校验密码的入口必须先把
// 单引号剥掉，否则含引号密码存进去后永远登不上（存取两侧语义不一致）。
void normalize_password(char* pass);
