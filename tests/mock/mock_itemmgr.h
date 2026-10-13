// mock_itemmgr.h — Mock implementation of IItemMgr for testing
//
// Provides a fake item manager that delegates load_item to a configurable
// factory function, enabling unit tests without real item creation.
//
#pragma once

#include "../src/item/ItemMgr.h"

// ====================================================================
// CMockItemMgr — 测试用物品管理 Mock 实现
// ====================================================================
class CMockItemMgr : public IItemMgr {
public:
    // 可配置的工厂钩子：测试通过此函数提供 mock 物品
    CContainer* (*load_factory)(const char*) = nullptr;

    // Mock 调用计数（供测试断言）
    int load_item_call_count     = 0;
    int load_fb_call_count       = 0;
    int find_item_call_count     = 0;
    int item_release_call_count  = 0;
    int remove_item_call_count   = 0;
    int random_vendor_call_count = 0;

    CContainer* load_item(const char* base_name,
                          string unique_id = "") override;
    CContainer* load_fb(const char* base_name, int flag = 0) override;
    CContainer* find_item(int32_t id) override;
    void item_release() override;
    void remove_item(int32_t item) override;
    CVendor* random_vendor() override;

    // 重置所有计数
    void reset_counts();
};
