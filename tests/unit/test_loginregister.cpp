// test_loginregister.cpp - Unit tests for LoginRegister functions
//
// Covers: CheckValidChar, check_legal_name (C++ fallback path),
//         init_new_char（新角色初始档案；落库不在本文件——save_to_server
//         依赖 DB）。
//
#include "pch.h"

#include "gtest/gtest.h"
#include <cstring>

// Include the header for function declarations
#include "net/LoginRegister.h"
#include "Char.h"
#include "User.h"

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

// ====================================================================
// Tests for init_new_char（新角色初始档案：首次登录新人分支的建档入口）
// ====================================================================

// age 必须在 setup_char 之前给定为 14：一旦缺省，setup_human_base_attrs 会把
// 它随机成 15~44，setup_human_max_hp 于是按随机 age、且 level 此刻尚未写入
// （按 0）算出虚高的 max_hp，setup_char 又把 hp 按这个 max_hp 补满——角色会
// 带着 hp > 实际上限上线，要等心跳 heal_up 才被压回。
// 用 max_hp 恰为 300 反证 age 没被随机（随机时它会落在 300~940）。
TEST(LoginRegister, InitNewChar_FixesAgeBeforeSetupChar)
{
    CUser me;
    init_new_char(me, "测试角色", "1001", "男性", 21, 21, 21, 21);

    EXPECT_EQ(me.query("age"), 14) << "age 必须给定，不能落进随机缺省";
    EXPECT_EQ(me.query("max_hp"), 300) << "age=14 且 level 未设 → 300 + 0*21*3";
    EXPECT_EQ(me.query("hp"), 300) << "hp 按 max_hp 补满，不得被虚高的上限抬高";
    EXPECT_LE(me.query("hp"), me.query("max_hp")) << "hp <= max_hp 不变量";
    EXPECT_EQ(me.query("eff_hp"), 300);
}

// 新角色起手值：起手经验为 0、等级、称号、性别、起始地，以及 setup_char
// 兜底补出的种族。
TEST(LoginRegister, InitNewChar_SetsStartValues)
{
    CUser me;
    init_new_char(me, "测试角色", "1002", "女性", 21, 21, 21, 21);

    EXPECT_EQ(me.query("combat_exp"), 0) << "起手经验为 0，不预置经验";
    EXPECT_EQ(me.query("level"), 1);
    EXPECT_STREQ(me.querystr("title"), "普通百姓");
    EXPECT_STREQ(me.querystr("gender"), "女性");
    EXPECT_STREQ(me.querystr("race"), "人类");
    EXPECT_GT(strlen(me.querystr("start_city")), 0) << "起始地必须给定";
}

// 登录时 level 已定为 1 → max_hp 重算为 363；已存在的 hp 不得悬在新上限之上
TEST(LoginRegister, InitNewChar_SecondSetupKeepsHpWithinMax)
{
    CUser me;
    init_new_char(me, "测试角色", "1003", "男性", 21, 21, 21, 21);

    setup_char(&me);  // 模拟登录时的重算

    EXPECT_EQ(me.query("max_hp"), 363) << "level=1 → 300 + 1*21*3";
    EXPECT_LE(me.query("hp"), me.query("max_hp")) << "上限重算后 hp 不得越界";
}
