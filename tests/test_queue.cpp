// test_queue.cpp - Unit tests for CWHQueue (thread-safe queue).
//
// Migrated from test/test_queue.cpp to Google Test.
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe
//
#include "gtest/gtest.h"
#include "Queue.h"

// ---------------------------------------------------------------------------
// Basic push/pop
// ---------------------------------------------------------------------------
TEST(CWHQueue, PushPop_Basic)
{
    CWHQueue q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), (size_t)0);

    BYTE* a = (BYTE*)0x1;
    BYTE* b = (BYTE*)0x2;
    BYTE* c = (BYTE*)0x3;

    EXPECT_TRUE(q.push_q(a));
    EXPECT_TRUE(q.push_q(b));
    EXPECT_TRUE(q.push_q(c));

    EXPECT_EQ(q.size(), (size_t)3);
    EXPECT_FALSE(q.empty());

    EXPECT_EQ((uintptr_t)q.pop_q(), (uintptr_t)a);
    EXPECT_EQ((uintptr_t)q.pop_q(), (uintptr_t)b);
    EXPECT_EQ((uintptr_t)q.pop_q(), (uintptr_t)c);

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), (size_t)0);
}

TEST(CWHQueue, PopEmpty_ReturnsNull)
{
    CWHQueue q;
    EXPECT_EQ(q.pop_q(), nullptr);
}

TEST(CWHQueue, FIFO_Order)
{
    CWHQueue q;
    for (int i = 0; i < 100; i++) {
        q.push_q((BYTE*)(uintptr_t)i);
    }
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ((uintptr_t)q.pop_q(), (uintptr_t)i);
    }
}

// ---------------------------------------------------------------------------
// ClearAll
// ---------------------------------------------------------------------------
TEST(CWHQueue, clear_all)
{
    CWHQueue q;
    for (int i = 0; i < 50; i++) {
        q.push_q((BYTE*)(uintptr_t)i);
    }
    EXPECT_EQ(q.size(), (size_t)50);

    q.clear_all();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), (size_t)0);
}

TEST(CWHQueue, ClearAll_Empty)
{
    CWHQueue q;
    q.clear_all();  // should not crash on empty queue
    EXPECT_TRUE(q.empty());
}
