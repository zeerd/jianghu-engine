// windows.h 必须最先包含：bcrypt.h 依赖其基础类型，
// 后置引入会与 Global.h 的 using namespace std; 撞成 C2872。
#include <windows.h>

#include <bcrypt.h>

#include <string.h>

#include "Crypto.h"

#pragma comment(lib, "bcrypt.lib")

// CNG 提供者按次开关：每次派生只 open/derive/close 一次，相对派生本身的
// 开销可忽略；不做全局驻留，避免关服路径上多一处句柄回收。
bool generate_salt(uint8_t* out, size_t len)
{
    if (!out || len == 0) return false;
    return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, out, (ULONG)len,
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool pbkdf2_hmac_sha256(const char* pass, const uint8_t* salt, size_t salt_len,
                        uint32_t iterations, uint8_t* out, size_t out_len)
{
    if (!out || out_len == 0) return false;
    if (!salt && salt_len > 0) {
        memset(out, 0, out_len);
        return false;
    }

    const char* password = pass ? pass : "";

    BCRYPT_ALG_HANDLE alg = nullptr;
    // PBKDF2 的 PRF 必须是 HMAC 形态的哈希提供者，缺 HMAC 标志会拿到
    // STATUS_INVALID_PARAMETER
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptDeriveKeyPBKDF2(
            alg, (PUCHAR)password, (ULONG)strlen(password), (PUCHAR)salt,
            (ULONG)salt_len, (ULONGLONG)iterations, out, (ULONG)out_len, 0);
        BCryptCloseAlgorithmProvider(alg, 0);
    }

    if (!BCRYPT_SUCCESS(status)) {
        memset(out, 0, out_len);  // 派生失败的输出不可被调用方误用
        return false;
    }
    return true;
}

namespace {

const char kBase64Table[]
    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 反查表：非 Base64 字符返回 -1
int base64_reverse(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

std::string base64_encode(const uint8_t* data, size_t len)
{
    std::string out;
    if (!data && len > 0) return out;

    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8)
                     | (uint32_t)data[i + 2];
        out += kBase64Table[(n >> 18) & 0x3F];
        out += kBase64Table[(n >> 12) & 0x3F];
        out += kBase64Table[(n >> 6) & 0x3F];
        out += kBase64Table[n & 0x3F];
    }

    size_t remain = len - i;
    if (remain == 1) {
        uint32_t n = (uint32_t)data[i] << 16;
        out += kBase64Table[(n >> 18) & 0x3F];
        out += kBase64Table[(n >> 12) & 0x3F];
        out += "==";
    }
    else if (remain == 2) {
        uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
        out += kBase64Table[(n >> 18) & 0x3F];
        out += kBase64Table[(n >> 12) & 0x3F];
        out += kBase64Table[(n >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

bool base64_decode(const std::string& in, std::vector<uint8_t>& out)
{
    out.clear();
    if (in.empty()) return true;
    if (in.size() % 4 != 0) return false;

    out.reserve((in.size() / 4) * 3);

    for (size_t i = 0; i < in.size(); i += 4) {
        int quad[4];
        int padding = 0;
        for (int j = 0; j < 4; j++) {
            unsigned char c = (unsigned char)in[i + j];
            if (c == '=') {
                // 填充只允许出现在末组末位
                if (i + 4 != in.size() || j < 2) return false;
                padding++;
                quad[j] = 0;
                continue;
            }
            if (padding > 0) return false;  // 填充后不得再有数据字符
            quad[j] = base64_reverse(c);
            if (quad[j] < 0) return false;
        }
        if (padding > 2) return false;

        uint32_t n = ((uint32_t)quad[0] << 18) | ((uint32_t)quad[1] << 12)
                     | ((uint32_t)quad[2] << 6) | (uint32_t)quad[3];
        out.push_back((uint8_t)((n >> 16) & 0xFF));
        if (padding < 2) out.push_back((uint8_t)((n >> 8) & 0xFF));
        if (padding < 1) out.push_back((uint8_t)(n & 0xFF));
    }
    return true;
}

bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len)
{
    if (len == 0) return true;
    if (!a || !b) return false;

    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

bool make_password_credential(const char* pass, std::string& out_salt,
                              std::string& out_hash)
{
    uint8_t salt[CRYPTO_SALT_LEN];
    uint8_t hash[CRYPTO_HASH_LEN];

    if (!generate_salt(salt, sizeof(salt))) return false;
    if (!pbkdf2_hmac_sha256(pass, salt, sizeof(salt), PBKDF2_ITERATIONS, hash,
                            sizeof(hash)))
        return false;

    out_salt = base64_encode(salt, sizeof(salt));
    out_hash = base64_encode(hash, sizeof(hash));
    return true;
}
