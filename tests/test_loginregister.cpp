// test_loginregister.cpp - Unit tests for LoginRegister functions
//
// Covers: CheckValidChar, check_legal_name (C++ fallback path)
// MakeBody is not unit-tested here due to heavy dependencies (file I/O, DB).
//
#include "gtest/gtest.h"
#include <cstring>

// Include the header for function declarations
#include "net/LoginRegister.h"

// ====================================================================
// Tests for CheckValidChar
// ====================================================================
TEST(LoginRegister, CheckValidChar_EmptyString)
{
    char empty[] = "";
    EXPECT_EQ(check_valid_char(empty), 1);
}

TEST(LoginRegister, CheckValidChar_PureAscii)
{
    char ascii[] = "hello";
    EXPECT_EQ(check_valid_char(ascii), 1);
}

TEST(LoginRegister, CheckValidChar_PureAsciiUpper)
{
    char upper[] = "HELLO";
    EXPECT_EQ(check_valid_char(upper), 1);
}

TEST(LoginRegister, CheckValidChar_MixedAscii)
{
    // Digits (0-9) are NOT in allowed ranges (>160 or A-Z/a-z)
    char mixed[] = "Hello";
    EXPECT_EQ(check_valid_char(mixed), 1);
}

TEST(LoginRegister, CheckValidChar_InvalidChar)
{
    // Digits are NOT in allowed ranges (>160 or A-Z/a-z)
    char digit[] = "123";
    EXPECT_EQ(check_valid_char(digit), 0);
}

TEST(LoginRegister, CheckValidChar_ChineseChar)
{
    // GBK Chinese character: first byte > 160
    char chinese[] = "\xc4\xe3";  // "你" in GBK
    EXPECT_EQ(check_valid_char(chinese), 1);
}

TEST(LoginRegister, CheckValidChar_ChineseThenAscii)
{
    char mixed[] = "\xc4\xe3hello";  // "你" + "hello"
    EXPECT_EQ(check_valid_char(mixed), 1);
}

// ====================================================================
// Tests for check_legal_name (C++ fallback, no Lua)
// When g_LuaEngine is nullptr, only length check runs.
// ====================================================================
TEST(LoginRegister, CheckLegalName_EmptyString)
{
    EXPECT_STREQ(check_legal_name(""),
                 "对不起，你的中文名字必须是 1 到 6 个中文字。");
}

TEST(LoginRegister, CheckLegalName_TooShort)
{
    EXPECT_STREQ(check_legal_name("a"),
                 "对不起，你的中文名字必须是 1 到 6 个中文字。");
}

TEST(LoginRegister, CheckLegalName_TooLong)
{
    EXPECT_STREQ(check_legal_name("abcdefghijklm"),
                 "对不起，你的中文名字必须是 1 到 6 个中文字。");
}

TEST(LoginRegister, CheckLegalName_ExactMin)
{
    EXPECT_STREQ(check_legal_name("ab"), "");
}

TEST(LoginRegister, CheckLegalName_ExactMax)
{
    EXPECT_STREQ(check_legal_name("abcdefghijkl"), "");
}

TEST(LoginRegister, CheckLegalName_WithinRange)
{
    EXPECT_STREQ(check_legal_name("hello"), "");
}
