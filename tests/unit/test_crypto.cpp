// test_crypto.cpp — Crypto.h：口令凭据原语（PBKDF2 / Base64 / 随机盐）
//
// PBKDF2 期望值来自 .NET Rfc2898DeriveBytes(HMAC-SHA256) 独立算出，
// 不是被测实现自己产生的，避免"实现错、测试跟着错"。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>
#include <vector>

#include "Crypto.h"

namespace {

// 把字节串转成十六进制小写
std::string ToHex(const uint8_t* data, size_t len)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < len; i++) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0F];
    }
    return out;
}

std::string ToHex(const std::vector<uint8_t>& v)
{
    return ToHex(v.data(), v.size());
}

}  // namespace

// ====================================================================
// PBKDF2-HMAC-SHA256：跨实现向量
// ====================================================================

TEST(CryptoPbkdf2, MatchesIndependentVector_SaltSALT_Iter1)
{
    uint8_t out[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("password", (const uint8_t*)"saltSALT", 8, 1,
                                   out, sizeof(out)));
    EXPECT_EQ(
        ToHex(out, sizeof(out)),
        "9a87d25e37408fd72f423d2285c7086d3a642a26e65fb7a02803b599b2933728");
}

TEST(CryptoPbkdf2, MatchesIndependentVector_SaltSALT_Iter2)
{
    uint8_t out[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("password", (const uint8_t*)"saltSALT", 8, 2,
                                   out, sizeof(out)));
    EXPECT_EQ(
        ToHex(out, sizeof(out)),
        "a0f30e25e8659c36a09589490d64a6b12e0c383c57aec18eda5ebea3f137aedb");
}

TEST(CryptoPbkdf2, MatchesIndependentVector_EmptyPassword)
{
    uint8_t out[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("", (const uint8_t*)"saltSALT", 8, 1, out,
                                   sizeof(out)));
    EXPECT_EQ(
        ToHex(out, sizeof(out)),
        "616058c477c89b058508a93c82df85046925a20f050e97e317dc00611d3b83de");
}

// 16 字节二进制盐（生产形态：随机字节而非可打印文本）
TEST(CryptoPbkdf2, MatchesIndependentVector_BinarySalt)
{
    uint8_t salt[16];
    for (int i = 0; i < 16; i++) salt[i] = (uint8_t)(i + 1);

    uint8_t out[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("password", salt, sizeof(salt), 1, out,
                                   sizeof(out)));
    EXPECT_EQ(
        ToHex(out, sizeof(out)),
        "850b9c399e6809acd55509a9f0223b0588cf5249d78cfa853defa7139975c583");
}

// 迭代次数改变派生结果（否则常量形同虚设）
TEST(CryptoPbkdf2, IterationCountChangesResult)
{
    uint8_t a[CRYPTO_HASH_LEN], b[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("password", (const uint8_t*)"saltSALT", 8, 1,
                                   a, sizeof(a)));
    ASSERT_TRUE(pbkdf2_hmac_sha256("password", (const uint8_t*)"saltSALT", 8, 2,
                                   b, sizeof(b)));
    EXPECT_FALSE(constant_time_equal(a, b, sizeof(a)));
}

// 派生失败（非法参数）时输出被清零，调用方不得拿到半成品
TEST(CryptoPbkdf2, InvalidArgumentsFailAndZeroOutput)
{
    uint8_t out[CRYPTO_HASH_LEN];
    memset(out, 0xAB, sizeof(out));
    EXPECT_FALSE(
        pbkdf2_hmac_sha256("password", nullptr, 4, 1, out, sizeof(out)));
    EXPECT_EQ(ToHex(out, sizeof(out)), std::string(sizeof(out) * 2, '0'));

    memset(out, 0xAB, sizeof(out));
    EXPECT_FALSE(
        pbkdf2_hmac_sha256("password", (const uint8_t*)"salt", 4, 1, out, 0));
}

// ====================================================================
// 随机盐
// ====================================================================

TEST(CryptoSalt, GeneratesRequestedLength)
{
    uint8_t salt[CRYPTO_SALT_LEN];
    ASSERT_TRUE(generate_salt(salt, sizeof(salt)));
    // 长度由调用方给定：这里只能验证"读满缓冲不越界"，唯一性见下例
    uint8_t probe[CRYPTO_SALT_LEN];
    memset(probe, 0, sizeof(probe));
    EXPECT_FALSE(constant_time_equal(salt, probe, sizeof(salt)))
        << "全零盐等价于没有盐，概率上不应出现";
}

TEST(CryptoSalt, TwoSaltsDiffer)
{
    uint8_t a[CRYPTO_SALT_LEN], b[CRYPTO_SALT_LEN];
    ASSERT_TRUE(generate_salt(a, sizeof(a)));
    ASSERT_TRUE(generate_salt(b, sizeof(b)));
    EXPECT_FALSE(constant_time_equal(a, b, sizeof(a)))
        << "同一口令两次落库必须用不同盐";
}

TEST(CryptoSalt, RejectsNullOrZeroLength)
{
    uint8_t salt[CRYPTO_SALT_LEN];
    EXPECT_FALSE(generate_salt(nullptr, sizeof(salt)));
    EXPECT_FALSE(generate_salt(salt, 0));
}

// ====================================================================
// Base64（RFC 4648 向量 + 往返）
// ====================================================================

TEST(CryptoBase64, Rfc4648Vectors)
{
    EXPECT_EQ(base64_encode((const uint8_t*)"", 0), "");
    EXPECT_EQ(base64_encode((const uint8_t*)"f", 1), "Zg==");
    EXPECT_EQ(base64_encode((const uint8_t*)"fo", 2), "Zm8=");
    EXPECT_EQ(base64_encode((const uint8_t*)"foo", 3), "Zm9v");
    EXPECT_EQ(base64_encode((const uint8_t*)"foob", 4), "Zm9vYg==");
    EXPECT_EQ(base64_encode((const uint8_t*)"fooba", 5), "Zm9vYmE=");
    EXPECT_EQ(base64_encode((const uint8_t*)"foobar", 6), "Zm9vYmFy");
}

TEST(CryptoBase64, RoundTripsEveryRemainder)
{
    for (size_t len = 0; len <= 65; len++) {
        std::vector<uint8_t> in(len);
        for (size_t i = 0; i < len; i++) in[i] = (uint8_t)(i * 7 + 3);

        const std::string encoded = base64_encode(in.data(), in.size());
        EXPECT_EQ(encoded.size(), ((len + 2) / 3) * 4) << "len=" << len;

        std::vector<uint8_t> decoded;
        ASSERT_TRUE(base64_decode(encoded, decoded)) << "len=" << len;
        EXPECT_EQ(decoded, in) << "len=" << len;
    }
}

TEST(CryptoBase64, DecodeRejectsMalformedInput)
{
    std::vector<uint8_t> out;
    EXPECT_FALSE(base64_decode("Zg=", out)) << "长度必须是 4 的倍数";
    EXPECT_FALSE(base64_decode("Zg!=", out)) << "非法字符";
    EXPECT_FALSE(base64_decode("=Zg=", out)) << "填充不得出现在首两位";
    EXPECT_FALSE(base64_decode("Z=8=", out)) << "填充后不得再有数据字符";
    EXPECT_FALSE(base64_decode("Z===", out)) << "单字符组最多两位填充";
    EXPECT_TRUE(out.empty());
}

TEST(CryptoBase64, DecodeEmptyStringYieldsEmptyOutput)
{
    std::vector<uint8_t> out;
    out.push_back(9);
    EXPECT_TRUE(base64_decode("", out));
    EXPECT_TRUE(out.empty());
}

// ====================================================================
// 定长比较
// ====================================================================

TEST(CryptoCompare, ConstantTimeEqualSemantics)
{
    const uint8_t a[] = {1, 2, 3, 4};
    const uint8_t b[] = {1, 2, 3, 4};
    const uint8_t c[] = {1, 2, 3, 5};

    EXPECT_TRUE(constant_time_equal(a, b, sizeof(a)));
    EXPECT_FALSE(constant_time_equal(a, c, sizeof(a)));
    EXPECT_TRUE(constant_time_equal(a, b, 0)) << "空串视为相等";
    EXPECT_FALSE(constant_time_equal(nullptr, b, sizeof(b)));
    EXPECT_FALSE(constant_time_equal(a, nullptr, sizeof(a)));
}

// ====================================================================
// 落库凭据形态
// ====================================================================

TEST(CryptoCredential, ProducesDecodableSaltAndHash)
{
    std::string salt, hash;
    ASSERT_TRUE(make_password_credential("secret1", salt, hash));

    std::vector<uint8_t> salt_bytes, hash_bytes;
    ASSERT_TRUE(base64_decode(salt, salt_bytes));
    ASSERT_TRUE(base64_decode(hash, hash_bytes));
    EXPECT_EQ(salt_bytes.size(), CRYPTO_SALT_LEN);
    EXPECT_EQ(hash_bytes.size(), CRYPTO_HASH_LEN);

    // 哈希必须等于「该盐 + 该口令 + 生产迭代次数」的派生结果
    uint8_t expected[CRYPTO_HASH_LEN];
    ASSERT_TRUE(pbkdf2_hmac_sha256("secret1", salt_bytes.data(),
                                   salt_bytes.size(), PBKDF2_ITERATIONS,
                                   expected, sizeof(expected)));
    EXPECT_EQ(base64_encode(expected, sizeof(expected)), hash);
}

TEST(CryptoCredential, DifferentSaltsForSamePassword)
{
    std::string salt1, hash1, salt2, hash2;
    ASSERT_TRUE(make_password_credential("secret1", salt1, hash1));
    ASSERT_TRUE(make_password_credential("secret1", salt2, hash2));
    EXPECT_NE(salt1, salt2) << "同口令两次落库盐必须不同";
    EXPECT_NE(hash1, hash2) << "盐不同则哈希必然不同";
}

TEST(CryptoCredential, EncodedFormHasNoSqlMetacharacters)
{
    // 落库字面量直接拼 SQL：字符集里不得出现引号/反斜杠等需要转义的字符
    std::string salt, hash;
    ASSERT_TRUE(make_password_credential("a'b\"c\\d", salt, hash));
    EXPECT_EQ(salt.find_first_of("'\"\\;"), std::string::npos);
    EXPECT_EQ(hash.find_first_of("'\"\\;"), std::string::npos);
}
