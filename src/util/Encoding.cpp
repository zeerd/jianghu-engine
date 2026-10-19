/** @file Encoding.cpp @brief GBK <-> UTF-8 编码转换实现 返回
 * std::string，无缓冲区限制，天然线程安全。 */
#include <windows.h>

#include "Encoding.h"

#include <string.h>
#include <vector>

// UTF-8 转 GBK（服务器内部用 GBK 编码）
std::string utf8_to_gbk(const char* utf8)
{
    if (!utf8) return "";

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen <= 0) return std::string(utf8);

    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf.data(), wlen);

    int glen
        = WideCharToMultiByte(CP_ACP, 0, wbuf.data(), -1, NULL, 0, NULL, NULL);
    if (glen <= 0) return std::string(utf8);

    std::string result(glen, '\0');
    WideCharToMultiByte(CP_ACP, 0, wbuf.data(), -1, &result[0], glen, NULL,
                        NULL);
    result.resize(glen - 1);
    return result;
}

// GBK 转 UTF-8（服务器内部是 GBK，转成 UTF-8 供 Lua 脚本使用）
std::string gbk_to_utf8(const char* gbk)
{
    if (!gbk) return "";

    int wlen = MultiByteToWideChar(CP_ACP, 0, gbk, -1, NULL, 0);
    if (wlen <= 0) return std::string(gbk);

    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_ACP, 0, gbk, -1, wbuf.data(), wlen);

    int ulen
        = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, NULL, 0, NULL, NULL);
    if (ulen <= 0) return std::string(gbk);

    std::string result(ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, &result[0], ulen, NULL,
                        NULL);
    result.resize(ulen - 1);
    return result;
}
