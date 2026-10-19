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
