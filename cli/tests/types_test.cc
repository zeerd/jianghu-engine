// cli/tests/types_test.cc — GameMessage 访问器契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=GameMessage*

#include "types.h"

#include <gtest/gtest.h>

TEST(GameMessage, GetReturnsValueOrDefault)
{
    GameMessage m;
    m.params["M"] = "hello";
    EXPECT_EQ(m.get("M"), "hello");
    EXPECT_EQ(m.get("X"), "");
    EXPECT_EQ(m.get("X", "fallback"), "fallback");
}

TEST(GameMessage, GetIntParsesAndFallsBack)
{
    GameMessage m;
    m.params["C"] = "42";
    m.params["bad"] = "abc";
    EXPECT_EQ(m.get_int("C"), 42);
    EXPECT_EQ(m.get_int("bad"), 0);
    EXPECT_EQ(m.get_int("bad", -5), -5);
    EXPECT_EQ(m.get_int("missing", 7), 7);
}

TEST(GameMessage, HasChecksPresence)
{
    GameMessage m;
    m.params["k"] = "";
    EXPECT_TRUE(m.has("k"));
    EXPECT_FALSE(m.has("absent"));
}

TEST(GameMessage, ToStringContainsCmdAndParams)
{
    GameMessage m;
    m.cmd = 19;
    m.params["M"] = "hi";
    std::string s = m.to_string();
    EXPECT_NE(s.find("cmd=19"), std::string::npos);
    EXPECT_NE(s.find("M=hi"), std::string::npos);
}
