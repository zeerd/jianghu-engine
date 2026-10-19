#include "color.h"

#include <gtest/gtest.h>

// ===== convert 测试 =====

TEST(ColorConvertTest, EmptyString) { EXPECT_EQ(color::convert(""), ""); }

TEST(ColorConvertTest, PlainText)
{
    EXPECT_EQ(color::convert("Hello"), "Hello");
}

TEST(ColorConvertTest, SingleColorCode)
{
    std::string result = color::convert("$REDtext$COMmore");
    EXPECT_EQ(result, "\033[31mtext\033[0mmore");
}

TEST(ColorConvertTest, NormalReset)
{
    EXPECT_EQ(color::convert("$COM"), "\033[0m");
}

TEST(ColorConvertTest, HighlightColor)
{
    std::string result = color::convert("$$HIRtext");
    // $ at i=0 -> "$$HI" not in map, output '$'; then $HIR at i=1
    EXPECT_EQ(result, "$\033[1;31mtext");
}

TEST(ColorConvertTest, AllBasicColors)
{
    // convert always appends reset at end
    EXPECT_EQ(color::convert("$RED"), "\033[31m");
    EXPECT_EQ(color::convert("$YEL"), "\033[33m");
    EXPECT_EQ(color::convert("$GRN"), "\033[32m");
    EXPECT_EQ(color::convert("$BLU"), "\033[34m");
    EXPECT_EQ(color::convert("$CYN"), "\033[36m");
    EXPECT_EQ(color::convert("$MAG"), "\033[35m");
    EXPECT_EQ(color::convert("$WHT"), "\033[37m");
}

TEST(ColorConvertTest, AllHighlightColors)
{
    EXPECT_EQ(color::convert("$HIR"), "\033[1;31m");
    EXPECT_EQ(color::convert("$HIG"), "\033[1;32m");
    EXPECT_EQ(color::convert("$HIB"), "\033[1;34m");
    EXPECT_EQ(color::convert("$HIC"), "\033[1;36m");
    EXPECT_EQ(color::convert("$HIM"), "\033[1;35m");
    EXPECT_EQ(color::convert("$HIW"), "\033[1;37m");
}

TEST(ColorConvertTest, EndAlwaysHasReset)
{
    // convert always appends a reset at the end
    EXPECT_EQ(color::convert("hello"), "hello");
    EXPECT_EQ(color::convert("$REDx"), "\033[31mx");
    EXPECT_EQ(color::convert("a$YELb"), "a\033[33mb");
}

// ===== strip 测试 =====

TEST(ColorStripTest, EmptyString) { EXPECT_EQ(color::strip(""), ""); }

TEST(ColorStripTest, PlainTextNoChange)
{
    EXPECT_EQ(color::strip("Hello World"), "Hello World");
}

TEST(ColorStripTest, SingleColorCode)
{
    EXPECT_EQ(color::strip("$REDtext"), "text");
}

TEST(ColorStripTest, NormalReset) { EXPECT_EQ(color::strip("$COM"), ""); }

TEST(ColorStripTest, MultipleColors)
{
    EXPECT_EQ(color::strip("$REDx$YELy$GRNz$COM"), "xyz");
}

TEST(ColorStripTest, ConsecutiveColors)
{
    // $$RED -> first $ ignored, second $RED stripped
    EXPECT_EQ(color::strip("$$REDtext"), "$text");
}

TEST(ColorStripTest, NoColorCodes)
{
    EXPECT_EQ(color::strip("no codes here"), "no codes here");
}
