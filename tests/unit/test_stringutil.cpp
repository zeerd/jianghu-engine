// test_stringutil.cpp - Unit tests for StringUtil functions.
//
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe --gtest_filter=StringUtil*
//
#include "pch.h"
#include <cstring>

// --- Forward declare functions under test ---
void analyse_string(char* Comm, char* arg, int len);
char* get_value(const char* cmd_str, const char* keyword, char* pValue,
                int nValueLen);
int valid_string(const char* msg);

// ====================================================================
// analyse_string: split command and arguments
// ====================================================================
TEST(StringUtil, AnalyseString_Basic)
{
    char buf[]   = "kill monster";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "kill");
    EXPECT_STREQ(arg, "monster");
}

TEST(StringUtil, AnalyseString_NoArg)
{
    char buf[]   = "look";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "look");
    EXPECT_STREQ(arg, "");
}

TEST(StringUtil, AnalyseString_MultipleArgs)
{
    char buf[]   = "say hello world";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "say");
    EXPECT_STREQ(arg, "hello world");
}

TEST(StringUtil, AnalyseString_EmptyString)
{
    char buf[]   = "";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "");
    EXPECT_STREQ(arg, "");
}

TEST(StringUtil, AnalyseString_ArgTruncated)
{
    char buf[]  = "cmd shortarg";
    char arg[4] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "cmd");
    EXPECT_STREQ(arg, "sho");  // truncated to nLen-1
}

// ====================================================================
// GetValue: parse &key=value& from a command string
// ====================================================================
TEST(StringUtil, GetValue_Basic)
{
    char val[64];
    const char* cmd = "&C=10&M=hello&";
    get_value(cmd, "C", val, sizeof(val));
    EXPECT_STREQ(val, "10");
    get_value(cmd, "M", val, sizeof(val));
    EXPECT_STREQ(val, "hello");
}

TEST(StringUtil, GetValue_MissingKey)
{
    char val[64];
    const char* cmd = "&C=10&M=hello&";
    get_value(cmd, "X", val, sizeof(val));
    EXPECT_STREQ(val, "");
}

TEST(StringUtil, GetValue_NumericValue)
{
    char val[64];
    const char* cmd = "&P=256&N=test_item&";
    get_value(cmd, "P", val, sizeof(val));
    EXPECT_STREQ(val, "256");
}

TEST(StringUtil, GetValue_Truncated)
{
    char val[4];
    const char* cmd = "&M=longvalue&";
    get_value(cmd, "M", val, sizeof(val));
    EXPECT_STREQ(val, "lon");  // truncated to nValueLen-1
}

// ====================================================================
// valid_string: check for illegal characters (double quotes)
// ====================================================================
TEST(StringUtil, ValidString_Normal)
{
    EXPECT_EQ(valid_string("hello world"), 1);
}

TEST(StringUtil, ValidString_Empty) { EXPECT_EQ(valid_string(""), 1); }

TEST(StringUtil, ValidString_WithQuote)
{
    EXPECT_NE(valid_string("say \"hello\""), 1);
}
