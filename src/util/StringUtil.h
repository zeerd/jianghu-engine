/**
 * @file StringUtil.h
 * @brief 通用字符串工具函数（分析/格式化/编码/校验）。
 */
#pragma once

#include <sal.h>

#include <cstddef>
#include <string>

// --- 字符串分析 ---
void analyse_string(char* Comm, char* arg, int len);
char* get_value(const char* cmd_str, const char* keyword, char* pValue,
                int nValueLen);
// 指令劈分（首个空格切分）：comm 收头部（含后续原样）、arg 收尾部并截到
// len-1；无空格时 arg 置空、comm 不变。char* 形态为就地版，保留给可变
// 缓冲调用点。
void analyse_string(std::string& comm, std::string& arg, size_t len);

// --- 定宽字段装配 ---
// 按字段宽度截尾拷贝：源串超长时只取前 max_len 字节——按字节截，可能截在
// 多字节字符中间，与历史定宽字段口径一致。字段宽度是协议契约（超长值截断
// 是既定行为）；空指针按空串处理。
std::string truncated_copy(const char* src, size_t max_len);

// --- 格式化 ---
// _Printf_format_string_ 让编译器按格式串核对实参宽度：槽位值已是 64 位，
// 必须配 %lld；若留 %d，x64 上会静默只取低 32 位——那正是本次要消灭的收窄。
char* snprintf(char* buf, int len, _Printf_format_string_ const char* fmt, ...);

// --- 编码 ---
// 通讯帧 XOR 自逆编解码（收发两侧同一变换、同一密钥）。密钥取
// [security] comm_key，缺省或留空回退内置默认密钥。
char* decode_str(char* msg);

// --- 密码规范化 ---
// 密码契约：密码不含单引号。登录口把整个登录串的单引号剥除后才比对
// （Login.cpp replace_string），因此所有写入/校验密码的入口必须先把
// 单引号剥掉，否则含引号密码存进去后永远登不上（存取两侧语义不一致）。
void normalize_password(char* pass);
