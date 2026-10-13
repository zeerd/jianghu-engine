// test_stacking.cpp - Unit tests for StackingLogic (stackable item invariants).
//
// Migrated from test/test_stacking.cpp to Google Test.
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe
//
#include "gtest/gtest.h"
#include "StackingLogic.h"

// ---------------------------------------------------------------------------
// TotalWeight / TotalValue: the core invariants
// ---------------------------------------------------------------------------
TEST(StackingLogic, TotalWeight_Basic)
{
    EXPECT_EQ(StackingLogic::total_weight(1, 80), 80);     // tea leaf: 1 x 80
    EXPECT_EQ(StackingLogic::total_weight(300, 5), 1500);  // dart: 300 x 5
    EXPECT_EQ(StackingLogic::total_weight(4000, 2),
              8000);  // iron pellet: 4000 x 2
}

TEST(StackingLogic, TotalWeight_ZeroAndNegative)
{
    // unset unit weight or invalid amount -> 0 (never negative)
    EXPECT_EQ(StackingLogic::total_weight(0, 5), 0);
    EXPECT_EQ(StackingLogic::total_weight(-1, 5), 0);
    EXPECT_EQ(StackingLogic::total_weight(10, 0), 0);
    EXPECT_EQ(StackingLogic::total_weight(10, -3), 0);
}

TEST(StackingLogic, TotalValue_Basic)
{
    EXPECT_EQ(StackingLogic::total_value(1, 80), 80);    // tea leaf: 1 x 80
    EXPECT_EQ(StackingLogic::total_value(50, 3), 150);   // dart: 50 x 3
    EXPECT_EQ(StackingLogic::total_value(100, 1), 100);  // single item
}

TEST(StackingLogic, TotalValue_ZeroAndNegative)
{
    EXPECT_EQ(StackingLogic::total_value(0, 5), 0);
    EXPECT_EQ(StackingLogic::total_value(10, 0), 0);
    EXPECT_EQ(StackingLogic::total_value(10, -1), 0);
}

// ---------------------------------------------------------------------------
// ValidSplitAmount: Split() semantics
// ---------------------------------------------------------------------------
TEST(StackingLogic, ValidSplitAmount_Basic)
{
    // split out 30 from a stack of 80
    EXPECT_EQ(StackingLogic::valid_split_amount(80, 30), 30);
    // split whole stack is allowed (caller treats as no-op / transfer)
    EXPECT_EQ(StackingLogic::valid_split_amount(80, 80), 80);
    // invalid requests
    EXPECT_EQ(StackingLogic::valid_split_amount(80, 0), 0);
    EXPECT_EQ(StackingLogic::valid_split_amount(80, -5), 0);
    EXPECT_EQ(StackingLogic::valid_split_amount(80, 81), 0);
}

TEST(StackingLogic, ValidSplitAmount_PreservesInvariants)
{
    // Simulate the Split() math the server performs:
    //   remaining = current - split
    //   both parts must satisfy TotalWeight/TotalValue invariants.
    long current = 80, split = 30;
    long v = StackingLogic::valid_split_amount(current, split);
    if (v > 0) {
        long remaining = current - v;
        EXPECT_EQ(StackingLogic::total_weight(1, remaining), remaining);
        EXPECT_EQ(StackingLogic::total_weight(1, v), v);
        // value conserved: 1*80 == 1*remaining + 1*v
        EXPECT_EQ(StackingLogic::total_value(1, current),
                  StackingLogic::total_value(1, remaining)
                      + StackingLogic::total_value(1, v));
    }
}

// ---------------------------------------------------------------------------
// MergedAmount: Combine() semantics
// ---------------------------------------------------------------------------
TEST(StackingLogic, MergedAmount_Basic)
{
    EXPECT_EQ(StackingLogic::merged_amount(30, 50), 80);
    EXPECT_EQ(StackingLogic::merged_amount(0, 80), 80);
    // merging onto an empty container must not lose the incoming stack
    EXPECT_EQ(StackingLogic::merged_amount(80, 0), 80);
}

// ---------------------------------------------------------------------------
// Simulated CItem state machine: locks set_amount() behavior
// (mirrors the invariant CItem::set_amount will implement in Phase 2)
// ---------------------------------------------------------------------------
namespace Sim {
struct Stack {
    long amount;
    long baseWeight;  // unit weight, never scaled
    long baseValue;   // unit price, never scaled
    long weight;      // derived: baseWeight * amount
    long value;       // derived: baseValue * amount

    explicit Stack(long baseWeight_, long baseValue_, long amount_)
        : amount(amount_), baseWeight(baseWeight_), baseValue(baseValue_)
    {
        weight = StackingLogic::total_weight(baseWeight, amount);
        value  = StackingLogic::total_value(baseValue, amount);
    }

    void SetAmount(long n)
    {
        amount = n;
        weight = StackingLogic::total_weight(baseWeight, amount);
        value  = StackingLogic::total_value(baseValue, amount);
    }
};
}  // namespace Sim

TEST(StackingLogic, Simulated_SetAmount_Invariants)
{
    Sim::Stack s(1, 1, 80);  // tea leaf stack, 80 pieces
    EXPECT_EQ(s.weight, 80);
    EXPECT_EQ(s.value, 80);

    s.SetAmount(5);  // after consuming 75
    EXPECT_EQ(s.amount, 5);
    EXPECT_EQ(s.weight, 5);  // not the old bogus 1000
    EXPECT_EQ(s.value, 5);   // base_value stays 1

    // amount back up to 80 via merge: invariants must hold again
    s.SetAmount(80);
    EXPECT_EQ(s.weight, 80);
    EXPECT_EQ(s.value, 80);
}
