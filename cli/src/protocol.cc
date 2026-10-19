#include "protocol.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace protocol {

namespace {

#ifdef _WIN32

/// 用 Windows API 在两个代码页之间转码（src_cp → dst_cp）。
/// 输入为空或转码失败时返回原串。
std::string codepage_convert(const std::string &input, UINT src_cp,
                             UINT dst_cp)
{
    if (input.empty()) return {};

    int in_len = static_cast<int>(input.size());

    int wide_len
        = MultiByteToWideChar(src_cp, 0, input.data(), in_len, nullptr, 0);
    if (wide_len <= 0) return input;

    std::wstring wide(wide_len, L'\0');
    MultiByteToWideChar(src_cp, 0, input.data(), in_len, &wide[0], wide_len);

    int out_len = WideCharToMultiByte(dst_cp, 0, wide.data(), wide_len, nullptr,
                                      0, nullptr, nullptr);
    if (out_len <= 0) return input;

    std::string out(out_len, '\0');
    WideCharToMultiByte(dst_cp, 0, wide.data(), wide_len, &out[0], out_len,
                        nullptr, nullptr);
    return out;
}

constexpr UINT kGbkCodePage = 936;  // GBK 在 Windows 上的代码页

#else

/// iconv 封装：src_code → dst_code。失败时返回空串。
std::string iconv_convert(const char *dst_code, const char *src_code,
                          const std::string &input)
{
    iconv_t cd = iconv_open(dst_code, src_code);
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        // Fallback: return as-is
        return input;
    }

    // Output buffer: worst case expansion is ~2x size
    size_t in_bytes  = input.size();
    size_t out_bytes = in_bytes * 2 + 8;
    std::string result(out_bytes, '\0');

    char *in_buf    = const_cast<char *>(input.data());
    char *out_buf   = &result[0];
    size_t in_left  = in_bytes;
    size_t out_left = out_bytes;

    if (iconv(cd, &in_buf, &in_left, &out_buf, &out_left)
        == static_cast<size_t>(-1)) {
        // Partial conversion or error — keep what we have
    }

    iconv_close(cd);
    result.resize(out_bytes - out_left);
    return result;
}

#endif

}  // namespace

void dump_hex(const std::string &data, size_t offset, size_t len)
{
    for (size_t i = offset; i < offset + len && i < data.size(); i++) {
        printf("%02X ", (unsigned char)data[i]);
        if ((i - offset + 1) % 16 == 0) printf("\n");
    }
    printf("\n%zu\n", len);
}

std::optional<GameMessage> parse_message(const std::string &raw)
{
    if (raw.empty()) return std::nullopt;

    // Find &C=N& at the start
    if (raw.size() < 4 || raw[0] != '&' || raw[1] != 'C' || raw[2] != '=') {
        return std::nullopt;
    }

    GameMessage msg;

    // Parse key=val pairs
    size_t i = 1;  // skip first '&'
    std::string key, val;
    bool in_key = true;

    auto append_char = [&](char c) {
        if (in_key)
            key += c;
        else
            val += c;
    };

    for (; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\t') break;  // terminator

        if (c == '&') {
            // End of pair
            if (!key.empty()) {
                if (key == "C") {
                    try {
                        msg.cmd = std::stoi(val);
                    } catch (...) {
                        msg.cmd = 0;
                    }
                }
                else {
                    msg.params[key] = val;
                }
            }
            key.clear();
            val.clear();
            in_key = true;
        }
        else if (c == '=') {
            in_key = false;
        }
        else {
            append_char(c);
        }
    }

    // Handle last pair (trailing &)
    if (!key.empty()) {
        if (key == "C") {
            try {
                msg.cmd = std::stoi(val);
            } catch (...) {
                msg.cmd = 0;
            }
        }
        else {
            msg.params[key] = val;
        }
    }

    return msg;
}

std::string command(const std::string &cmd,
                    const std::vector<std::string> &args)
{
    std::ostringstream oss;
    oss << cmd;
    for (const auto &a : args) oss << ' ' << a;
    oss << '\t';
    std::string final = oss.str();
    return final;
}

std::string immediate(const std::string &cmd,
                      const std::vector<std::string> &args)
{
    std::ostringstream oss;
    oss << "0 " << cmd;
    for (const auto &a : args) oss << ' ' << a;
    oss << '\t';
    return oss.str();
}

std::string format_login(const std::string &local_ip,
                         const std::string &version_prefix,
                         const std::string &account,
                         const std::string &password, const std::string &card,
                         const std::string &cardpass)
{
    // Server uses analyse_string() which splits on SPACE (' ').
    // The DecodeStr() layer strips '\n' and '\t' during XOR, so
    // we send space-delimited format:
    //   login client_name account password card cardpass\t
    std::ostringstream oss;
    oss << "login " << local_ip << " " << version_prefix << " " << account
        << " " << password << " " << card << " " << cardpass;
    // PCap from xmud shows no '\t' at the end.
    return oss.str();
}

std::string format_register(const std::string &version_prefix,
                            const std::string &account,
                            const std::string &password,
                            const std::string &gender, int strength,
                            int constitution, int intelligence, int dexterity)
{
    // Server register format: "register client_name account password gender str
    // con int dex"\t
    std::ostringstream oss;
    oss << "register " << version_prefix << " " << account << " " << password
        << " " << gender << " " << strength << " " << constitution << " "
        << intelligence << " " << dexterity << '\t';
    return oss.str();
}

std::string format_tell(int64_t uid, int64_t target_id,
                        const std::string &content)
{
    // tell wire format: "{uid} tell {target_id} <content>\t"
    std::ostringstream oss;
    oss << uid << " tell " << target_id << ' ' << content << '\t';
    return oss.str();
}

std::string format_chat(int64_t uid, const std::string &content)
{
    // xchat wire format: "{uid} chat 0 <content>\t"  (public broadcast to room)
    std::ostringstream oss;
    oss << uid << " chat 0 " << content << '\t';
    return oss.str();
}

std::string encode_str(const std::string &input)
{
    // XOR cipher matching server's DecodeStr (Globle.cpp:1314-1339)
    // Key from Globle.cpp:1317: "\15L\26A\18N\12W\25O\33O\6D"
    static const unsigned char key[] = "\15L\26A\18N\12W\25O\33O\6D";
    static const int key_len         = strlen((const char *)key);

    std::string result = input;
    int t              = 0;
    for (size_t p = 0; p < result.size(); p++) {
        if (result[p] != ' ' && result[p] != '\n' && result[p] != '\t') {
            result[p] ^= key[t];
            // Re-check: if XOR produced \t, \n, space, or \0, re-XOR
            if (result[p] == '\t') result[p] = '\t' ^ key[t];
            if (result[p] == '\n') result[p] = '\n' ^ key[t];
            if (result[p] == ' ') result[p] = ' ' ^ key[t];
            if (result[p] == '\0') result[p] = '\0' ^ key[t];
        }
        t++;
        if (t >= key_len) t = 0;
    }
    return result;
}

std::string gbk_to_utf8(const std::string &input)
{
    if (input.empty()) return {};
#ifdef _WIN32
    return codepage_convert(input, kGbkCodePage, CP_UTF8);
#else
    return iconv_convert("UTF-8", "GBK", input);
#endif
}

std::string utf8_to_gbk(const std::string &input)
{
    if (input.empty()) return {};
#ifdef _WIN32
    return codepage_convert(input, CP_UTF8, kGbkCodePage);
#else
    return iconv_convert("GBK", "UTF-8", input);
#endif
}

}  // namespace protocol
