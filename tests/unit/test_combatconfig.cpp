// test_combatconfig.cpp - Unit tests for CombatConfig.
//
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe --gtest_filter=CombatConfig*
//
#include "pch.h"
#include "CombatConfig.h"

// ====================================================================
// Default values
// ====================================================================
TEST(CCombatConfig, DefaultValues)
{
    CCombatConfig cfg;
    EXPECT_EQ(cfg.query("ip_limit"), 30);
    EXPECT_EQ(cfg.query("login_limit"), 2500);
    EXPECT_EQ(cfg.query("kick_idle"), 1);
    EXPECT_EQ(cfg.query("no_check_house"), 1);
    EXPECT_EQ(cfg.query("debug"), 0);
    EXPECT_EQ(cfg.query("makelove"), 0);
}

// ====================================================================
// set(int) + query for typed members
// ====================================================================
TEST(CCombatConfig, SetIpLimit)
{
    CCombatConfig cfg;
    cfg.set("ip_limit", 100);
    EXPECT_EQ(cfg.ip_limit(), 100);
    EXPECT_EQ(cfg.query("ip_limit"), 100);
}

TEST(CCombatConfig, SetLoginLimit)
{
    CCombatConfig cfg;
    cfg.set("login_limit", 5000);
    EXPECT_EQ(cfg.login_limit(), 5000);
    EXPECT_EQ(cfg.query("login_limit"), 5000);
}

TEST(CCombatConfig, SetKickIdle)
{
    CCombatConfig cfg;
    cfg.set("kick_idle", 0);
    EXPECT_FALSE(cfg.kick_idle());
    EXPECT_EQ(cfg.query("kick_idle"), 0);

    cfg.set("kick_idle", 1);
    EXPECT_TRUE(cfg.kick_idle());
    EXPECT_EQ(cfg.query("kick_idle"), 1);
}

TEST(CCombatConfig, SetNoCheckHouse)
{
    CCombatConfig cfg;
    cfg.set("no_check_house", 0);
    EXPECT_FALSE(cfg.no_check_house());
}

TEST(CCombatConfig, SetDebug)
{
    CCombatConfig cfg;
    cfg.set("debug", 1);
    EXPECT_TRUE(cfg.debug());
    EXPECT_EQ(cfg.query("debug"), 1);
}

// ====================================================================
// Dynamic key-value (fallback store)
// ====================================================================
TEST(CCombatConfig, DynamicSetQuery)
{
    CCombatConfig cfg;
    cfg.set("custom_key", 42);
    EXPECT_EQ(cfg.query("custom_key"), 42);
}

TEST(CCombatConfig, DynamicSetQueryString)
{
    CCombatConfig cfg;
    cfg.set("msg", "hello");
    EXPECT_STREQ(cfg.querystr("msg"), "hello");
}

TEST(CCombatConfig, DynamicDelete)
{
    CCombatConfig cfg;
    cfg.set("temp_key", 99);
    EXPECT_EQ(cfg.query("temp_key"), 99);
    cfg.del("temp_key");
    EXPECT_EQ(cfg.query("temp_key"), 0);
}

TEST(CCombatConfig, QueryMissingKey)
{
    CCombatConfig cfg;
    EXPECT_EQ(cfg.query("nonexistent"), 0);
    EXPECT_STREQ(cfg.querystr("nonexistent"), "");
}

// ====================================================================
// Edge cases
// ====================================================================
TEST(CCombatConfig, NullKey)
{
    CCombatConfig cfg;
    cfg.set(nullptr, 1);
    EXPECT_EQ(cfg.query(nullptr), 0);
    cfg.del(nullptr);
}

TEST(CCombatConfig, EmptyKey)
{
    CCombatConfig cfg;
    cfg.set("", 1);
    EXPECT_EQ(cfg.query(""), 0);
}

TEST(CCombatConfig, OverwriteTypedKey)
{
    CCombatConfig cfg;
    cfg.set("ip_limit", 50);
    EXPECT_EQ(cfg.ip_limit(), 50);
    cfg.set("ip_limit", 200);
    EXPECT_EQ(cfg.ip_limit(), 200);
}
