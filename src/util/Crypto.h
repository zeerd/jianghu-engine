/**
 * @file Crypto.h
 * @brief 口令凭据原语：安全随机盐、PBKDF2-HMAC-SHA256 派生、Base64、定长比较。
 *
 * 账号口令以「盐 + 派生哈希」落库（见 docs/design/encryption.md）：
 * 明文列只作为历史数据的比对来源，新写入一律走这里的原语。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

// 盐长度（字节）与派生哈希长度（字节）
constexpr size_t CRYPTO_SALT_LEN = 16;
constexpr size_t CRYPTO_HASH_LEN = 32;

// PBKDF2 迭代次数：登录/改密/身份验证路径同步计算，100k 在单核上约几十毫秒
constexpr uint32_t PBKDF2_ITERATIONS = 100000;

// 生成密码学安全随机字节（Windows CNG 系统首选 RNG）。失败返回 false。
bool generate_salt(uint8_t* out, size_t len);

// PBKDF2-HMAC-SHA256 派生 out_len 字节密钥。CNG 不可用或参数非法时
// 返回 false 且把 out 清零，调用方必须以返回值为准，不得使用 out 内容。
bool pbkdf2_hmac_sha256(const char* pass, const uint8_t* salt, size_t salt_len,
                        uint32_t iterations, uint8_t* out, size_t out_len);

// Base64 编解码：盐/哈希落库的文本形态（字符集不含引号与控制字节，
// 直接进 SQL 字面量无需转义）。解码遇到非法字符或长度不整返回 false。
std::string base64_encode(const uint8_t* data, size_t len);
bool base64_decode(const std::string& in, std::vector<uint8_t>& out);

// 定长等值比较：口令哈希比对用，避免逐字节比较在首处差异提前返回。
bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len);

// 组合原语：生成随机盐并按 PBKDF2_ITERATIONS 派生哈希，输出落库用的
// Base64 文本形态（字符集不含引号）。注册/改密/巫师自举/明文迁移的
// 凭据写入统一走它；任一环节失败返回 false，输出内容不得使用。
bool make_password_credential(const char* pass, std::string& out_salt,
                              std::string& out_hash);
