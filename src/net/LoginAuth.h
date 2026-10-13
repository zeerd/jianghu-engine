/**
 * @file LoginAuth.h
 * @brief 认证/安全/IP工具函数，提供密码验证、账号冻结与 IP 查询。
 */

#pragma once

#include <cstdint>

class CUser;
class CChar;
class CContainer;

// --- 认证 ---
int ban_me(CChar* me);
int verify_password(const char* id, const char* pass);
int change_verify_password(const char* id, const char* pass);
int change_password(const char* id, const char* pass);
int set_invalid_account(const char* id, int freeze);
int query_invalid_account(const char* id);

// --- 工具 ---
char* make_ip(char* main_ip, char* local_ip);
char* get_ip(CChar* me);
int wizardp(CChar* me);
int userp(CContainer* me);
