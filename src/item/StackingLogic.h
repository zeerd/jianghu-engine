/**
 * @file StackingLogic.h
 * @brief 可堆叠物品的纯逻辑层，定义重量/价值/污染检测。
 */

#pragma once

namespace StackingLogic {
// Total weight of a stack: unit weight * amount.
// Returns 0 when the unit weight is unset (<= 0) or amount is invalid.
inline int32_t total_weight(int32_t baseWeight, int32_t amount)
{
    if (baseWeight <= 0 || amount <= 0) return 0;
    return baseWeight * amount;
}

// Total value of a stack: unit value * amount.
// Returns 0 when the unit value is unset (<= 0) or amount is invalid.
inline int32_t total_value(int32_t baseValue, int32_t amount)
{
    if (baseValue <= 0 || amount <= 0) return 0;
    return baseValue * amount;
}

// Validate a Split request. Returns the amount the split piece should
// carry (0 means the request is invalid). The remaining stack keeps
// currentAmount - returnedAmount. Splitting the whole stack returns
// currentAmount itself (no-op semantics, caller handles it).
inline int32_t valid_split_amount(int32_t currentAmount, int32_t splitAmount)
{
    if (splitAmount <= 0) return 0;
    if (splitAmount > currentAmount) return 0;
    return splitAmount;
}

// Amount after merging two stacks (simple addition, kept here so tests
// lock the semantics).
inline int32_t merged_amount(int32_t amountA, int32_t amountB)
{
    return amountA + amountB;
}
}  // namespace StackingLogic
