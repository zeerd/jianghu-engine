// mock_itemmgr.h — Mock implementation of IItemMgr for testing
//
// Provides a fake item manager that delegates load_item to a configurable
// factory function, enabling unit tests without real item creation.
//
#pragma once

#include <memory>

#include "../../src/item/ItemMgr.h"

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

// ====================================================================
// RegisterFbItemMgr — 注册大厅副本实例支的 Mock 实现
// ====================================================================
// 进注册副本走 g_item_mgr->load_fb：缺省仍恒空（逼出调用方的失败兜底支），
// 置 real_load_fb 后委托生产实现走真实副本链路（create_fb → 副本房 →
// g_item_list 登记）。remove_item 一律委托生产实现，保证副本析构能从
// g_item_list 摘掉自己，否则 delete 后全局表里留悬垂指针。
class RegisterFbItemMgr : public CMockItemMgr {
public:
    RegisterFbItemMgr();

    CContainer* load_fb(const char* base_name, int flag = 0) override;
    void remove_item(int32_t item) override;

    // true → load_fb 走生产实现（实例支）；false（缺省）→ 恒空（兜底支）
    bool real_load_fb = false;
    // true → 造一个没有房间的空副本（start_map 为空 → move_in 必失败）
    bool roomless_fb = false;

private:
    std::unique_ptr<IItemMgr> m_real;
};
