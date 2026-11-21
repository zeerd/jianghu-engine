// test_message.cpp - 消息管线字符串工具（replace_string 行为锁定）
//
//
//
#include "pch.h"

#include <string>

// 被测函数（Message.cpp 实现）
void replace_string(char* msg, const char* oldstr, const char* newstr, int len);
void replace_string(std::string& msg, const char* oldstr, const char* newstr,
                    size_t len);

// ====================================================================
// replace_string：就地替换。宽度契约——结果截到 len-1，替换文本不被再次
// 扫描。
// ====================================================================

TEST(ReplaceString, ReplacesSingleOccurrence)
{
    char msg[64] = "hello $N world";
    replace_string(msg, "$N", "abc", 64);
    EXPECT_STREQ(msg, "hello abc world");
}

TEST(ReplaceString, ReplacesAllOccurrences)
{
    char msg[64] = "$N met $N";
    replace_string(msg, "$N", "kai", 64);
    EXPECT_STREQ(msg, "kai met kai");
}

TEST(ReplaceString, NoMatchLeavesMessageUnchanged)
{
    char msg[64] = "plain text";
    replace_string(msg, "$N", "x", 64);
    EXPECT_STREQ(msg, "plain text");
}

// 宽度契约：超长结果截到 len-1 为止，缓冲外一字节不写
TEST(ReplaceString, ResultTruncatedAtLenMinusOne)
{
    char msg[8] = "$N";
    replace_string(msg, "$N", "abcdefghij", 8);
    EXPECT_STREQ(msg, "abcdefg");
}

// 空模式不构成匹配：消息原样（否则空串会命中每个位置）
TEST(ReplaceString, EmptyPatternIsNoOp)
{
    char msg[8] = "abc";
    replace_string(msg, "", "x", 8);
    EXPECT_STREQ(msg, "abc");
}

// 替换产物不回扫：a→aa 在单个 a 上得到 aa 而不是无限增长
TEST(ReplaceString, ReplacementNotRescanned)
{
    char msg[8] = "a";
    replace_string(msg, "a", "aa", 8);
    EXPECT_STREQ(msg, "aa");
}

// ====================================================================
// std::string 形态（唯一算法实现，char* 形态委托于此）
// ====================================================================

TEST(ReplaceStringStd, ReplacesAndTruncatesAtLenMinusOne)
{
    std::string msg = "hi $N";
    replace_string(msg, "$N", "abc", 64);
    EXPECT_EQ(msg, "hi abc");

    std::string tight = "$N";
    replace_string(tight, "$N", "abcdefghij", 8);
    EXPECT_EQ(tight, "abcdefg") << "宽度契约：截到 len-1";
}

// 结果不携带终止符：string 形态的 size 必须就是内容长度
TEST(ReplaceStringStd, ResultHasNoEmbeddedNul)
{
    std::string msg = "plain";
    replace_string(msg, "$N", "x", 64);
    EXPECT_EQ(msg.size(), 5u);
    EXPECT_EQ(msg, "plain");
}

TEST(ReplaceStringStd, EmptyPatternIsNoOp)
{
    std::string msg = "abc";
    replace_string(msg, "", "x", 8);
    EXPECT_EQ(msg, "abc");
}
