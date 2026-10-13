/**
 * @file Encoding.h
 * @brief GBK ↔ UTF-8 编码转换。
 */
//---------------------------------------------------------

#pragma once

#include <string>

// UTF-8 转 GBK
std::string utf8_to_gbk(const char* utf8);

// GBK 转 UTF-8
std::string gbk_to_utf8(const char* gbk);
