// cli/tests/protocol_test.cc — wire 格式与编解码契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=Protocol*

#include "protocol.h"

#include <gtest/gtest.h>

#include <string>

// ==================== parse_message ====================

TEST(Protocol, ParseMessageSplitsParamsFromCmd)
{
    auto msg = protocol::parse_message("&C=19&M=hello&N=x&\t");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->cmd, 19);
    EXPECT_EQ(msg->get("M"), "hello");
    EXPECT_EQ(msg->get("N"), "x");
}

TEST(Protocol, ParseMessageAcceptsTrailingPairWithoutAmpersand)
{
    auto msg = protocol::parse_message("&C=19&M=hello");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->cmd, 19);
    EXPECT_EQ(msg->get("M"), "hello");
}

TEST(Protocol, ParseMessageRejectsEmptyOrMalformed)
{
    EXPECT_FALSE(protocol::parse_message("").has_value());
    EXPECT_FALSE(protocol::parse_message("&C=").has_value());
    EXPECT_FALSE(protocol::parse_message("C=19&").has_value());
    EXPECT_FALSE(protocol::parse_message("&X=1&").has_value());
}

TEST(Protocol, ParseMessageBadCmdValueBecomesZero)
{
    auto msg = protocol::parse_message("&C=abc&");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->cmd, 0);
}

// ==================== command / immediate ====================

TEST(Protocol, CommandJoinsArgsAndTerminates)
{
    EXPECT_EQ(protocol::command("go", {"east"}), "go east\t");
    EXPECT_EQ(protocol::command("look"), "look\t");
    EXPECT_EQ(protocol::command("tell", {"123", "hi"}), "tell 123 hi\t");
}

TEST(Protocol, ImmediatePrefixesPlayerZero)
{
    EXPECT_EQ(protocol::immediate("save"), "0 save\t");
    EXPECT_EQ(protocol::immediate("go", {"east"}), "0 go east\t");
}

// ==================== 格式化 ====================

TEST(Protocol, FormatLoginUsesSpacesWithoutTab)
{
    std::string s = protocol::format_login("1.2.3.4", "JJ.0.0.1", "acc",
                                           "pwd", "card", "cp");
    EXPECT_EQ(s, "login 1.2.3.4 JJ.0.0.1 acc pwd card cp");
    EXPECT_EQ(s.find('\t'), std::string::npos);
}

TEST(Protocol, FormatRegisterAppendsTab)
{
    std::string s = protocol::format_register("JJ.0.0.1", "acc", "pwd", "M",
                                              21, 21, 21, 21);
    EXPECT_EQ(s,
              "register JJ.0.0.1 acc pwd M 21 21 21 21\t");
}

TEST(Protocol, FormatTellAndChatWireFormats)
{
    EXPECT_EQ(protocol::format_tell(1001, 2002, "你好"),
              "1001 tell 2002 你好\t");
    EXPECT_EQ(protocol::format_chat(1001, "大家好"),
              "1001 chat 0 大家好\t");
}

// ==================== XOR 编解码 ====================

TEST(Protocol, EncodeStrPreservesWhitespace)
{
    // 空格/换行/制表符不参与 XOR，原样保留
    EXPECT_EQ(protocol::encode_str(" "), " ");
    EXPECT_EQ(protocol::encode_str("\n"), "\n");
    EXPECT_EQ(protocol::encode_str("\t"), "\t");
    EXPECT_EQ(protocol::encode_str(""), "");
}

TEST(Protocol, EncodeDecodeRoundTrips)
{
    const std::string samples[] = {"look", "tell 1001 hi",
                                   "\xC4\xE3\xBA\xC3" " world"};  // GBK 你好
    for (const auto &s : samples) {
        SCOPED_TRACE(s);
        EXPECT_EQ(protocol::decode_str(protocol::encode_str(s)), s);
    }
}

// ==================== GBK/UTF-8 转码 ====================

TEST(Protocol, GbkUtf8RoundTrips)
{
    std::string utf8 = "\xE6\xAD\xA6\xE5\xBD\x93" "abc";  // 武当abc
    std::string gbk  = protocol::utf8_to_gbk(utf8);
    EXPECT_NE(gbk, utf8);
    EXPECT_EQ(protocol::gbk_to_utf8(gbk), utf8);
}

TEST(Protocol, EmptyConversionReturnsEmpty)
{
    EXPECT_EQ(protocol::utf8_to_gbk(""), "");
    EXPECT_EQ(protocol::gbk_to_utf8(""), "");
}

TEST(Protocol, DecodeEqualsEncodeBySymmetry)
{
    std::string raw = "abc123";
    EXPECT_EQ(protocol::decode_str(raw), protocol::encode_str(raw));
}
