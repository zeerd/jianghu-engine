// test_encoding.cpp - Unit tests for Utf8ToGbk / GbkToUtf8 encoding conversion.
//
// Build+Run: .\run-coverage.ps1
//
#include "gtest/gtest.h"
#include <cstring>
#include <string>
#include <windows.h>
#include "Encoding.h"

// ---------------------------------------------------------------------------
// Test: Utf8ToGbk converts Chinese characters correctly
// ---------------------------------------------------------------------------
TEST(Encoding, Utf8ToGbk_Chinese)
{
    const char* utf8str
        = "\xe5\xb1\xb1\xe4\xb8\x8a\xe6\x9c\x89\xe5\x8d\xb1\xe9\x99\xa9\xe5\x90"
          "\x97";
    std::string gbk = utf8_to_gbk(utf8str);
    EXPECT_EQ((int)gbk.size(), 12);
}

TEST(Encoding, Utf8ToGbk_Ascii)
{
    const char* ascii = "shan";
    std::string gbk   = utf8_to_gbk(ascii);
    EXPECT_EQ(gbk, std::string("shan"));
}

TEST(Encoding, Utf8ToGbk_Empty)
{
    std::string gbk = utf8_to_gbk("");
    EXPECT_EQ(gbk, std::string(""));
}

TEST(Encoding, Utf8ToGbk_Null)
{
    std::string gbk = utf8_to_gbk(NULL);
    EXPECT_EQ(gbk, std::string(""));
}

// 长文本：转换结果的字节数必须与输入一致（转换缓冲区多写一个 NUL 会截断）
TEST(Encoding, Utf8ToGbk_LongAsciiKeepsAllBytes)
{
    std::string ascii(501, 'x');
    std::string gbk = utf8_to_gbk(ascii.c_str());
    EXPECT_EQ(gbk.size(), ascii.size());
    EXPECT_EQ(gbk, ascii);
}

TEST(Encoding, GbkToUtf8_LongAsciiKeepsAllBytes)
{
    std::string ascii(501, 'x');
    std::string utf8 = gbk_to_utf8(ascii.c_str());
    EXPECT_EQ(utf8.size(), ascii.size());
    EXPECT_EQ(utf8, ascii);
}

// ---------------------------------------------------------------------------
// Test: Round-trip UTF-8 -> GBK -> UTF-8 preserves original
// ---------------------------------------------------------------------------
class EncodingRoundTrip : public ::testing::TestWithParam<const char*> {};

TEST_P(EncodingRoundTrip, RoundTrip)
{
    const char* original = GetParam();
    std::string gbk      = utf8_to_gbk(original);
    std::string back     = gbk_to_utf8(gbk.c_str());
    EXPECT_EQ(back, std::string(original));
}

INSTANTIATE_TEST_CASE_P(
    RoundTrips, EncodingRoundTrip,
    ::testing::Values(
        "\xe8\x80\x81\xe6\x9d\x8e\xe5\xa4\xb4",  // "老头"
        "\xe5\xb1\xb1\xe4\xb8\x8a\xe6\x9c\x89\xe5\x8d\xb1\xe9\x99\xa9\xe5\x90"
        "\x97\xef\xbc\x9f",  // "山上有危险吗？"
        "\xe9\x99\x84\xe8\xbf\x91\xe6\x9c\x89\xe4\xbb\x80\xe4\xb9\x88\xe5\x95"
        "\x86\xe4\xba\xba\xe5\x90\x97\xef\xbc\x9f"  // "附近有什么商人吗？"
        ));

// ---------------------------------------------------------------------------
// 契约：编译期字符串字面量（执行字符集）与运行期 utf8_to_gbk 的产物必须一致。
// 二者分别由工程的 /execution-charset:gbk 与 Encoding.cpp 的 GBK 代码页 936
// 钉死；任一侧漂移（如换到非中文区域的机器上按系统默认编译/转换），
// 静态文本与动态转换文本就不再相等——C++ 字面量、协议、Lua 字符串空间
// 的 GBK 跨层契约随之断裂。
// ---------------------------------------------------------------------------
TEST(Encoding, LiteralMatchesRuntimeConversion)
{
    EXPECT_EQ(std::string("你好"), utf8_to_gbk("\xe4\xbd\xa0\xe5\xa5\xbd"));
    EXPECT_EQ(std::string("山上有危险吗？"),
              utf8_to_gbk("\xe5\xb1\xb1\xe4\xb8\x8a\xe6\x9c\x89\xe5\x8d\xb1"
                          "\xe9\x99\xa9\xe5\x90\x97\xef\xbc\x9f"));
}

// GBK 回转的字节值必须精确等于 GBK 编码（C4E3 BAC3 = "你好"），
// 钉住代码页 936 本身，而不是"某个碰巧能往返的编码"。
TEST(Encoding, GbkBytesAreCodePage936)
{
    std::string gbk = utf8_to_gbk("\xe4\xbd\xa0\xe5\xa5\xbd");
    ASSERT_EQ(gbk.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(gbk[0]), 0xC4);
    EXPECT_EQ(static_cast<unsigned char>(gbk[1]), 0xE3);
    EXPECT_EQ(static_cast<unsigned char>(gbk[2]), 0xBA);
    EXPECT_EQ(static_cast<unsigned char>(gbk[3]), 0xC3);
}
