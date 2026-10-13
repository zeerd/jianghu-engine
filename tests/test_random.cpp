// test_random.cpp - Unit tests for g_utils.random() and urandom().
//
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe --gtest_filter=Random*
//
#include "pch.h"
#include <cstdlib>
#include <cmath>
#include "Utils.h"

double urandom(double seed);

// ====================================================================
// g_utils.random(): returns int in [0, nSeed)
// ====================================================================
TEST(Random, BasicRange)
{
    srand(42);
    for (int i = 0; i < 1000; i++) {
        int32_t r = g_utils.random(100);
        EXPECT_GE(r, 0);
        EXPECT_LT(r, 100);
    }
}

TEST(Random, ZeroSeed) { EXPECT_EQ(g_utils.random(0), 0); }

TEST(Random, NegativeSeed) { EXPECT_EQ(g_utils.random(-10), 0); }

TEST(Random, Seed1)
{
    srand(1);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(g_utils.random(1), 0);
    }
}

// ====================================================================
// urandom(): returns double in [0, nSeed)
// ====================================================================
TEST(URandom, BasicRange)
{
    srand(42);
    for (int i = 0; i < 1000; i++) {
        double r = g_utils.urandom(100.0);
        EXPECT_GE(r, 0.0);
        EXPECT_LT(r, 100.0);
    }
}

TEST(URandom, ZeroSeed) { EXPECT_EQ(g_utils.urandom(0.0), 0.0); }

TEST(URandom, NegativeSeed) { EXPECT_EQ(g_utils.urandom(-10.0), 0.0); }

TEST(URandom, Distribution)
{
    srand(12345);
    double sum = 0.0;
    int n      = 10000;
    for (int i = 0; i < n; i++) {
        sum += g_utils.urandom(100.0);
    }
    double avg = sum / n;
    // Average should be roughly 50.0 (uniform distribution on [0,100))
    EXPECT_GT(avg, 40.0);
    EXPECT_LT(avg, 60.0);
}
