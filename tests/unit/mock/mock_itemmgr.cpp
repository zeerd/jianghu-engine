// mock_itemmgr.cpp — Mock implementation of IItemMgr for testing
//
#include "mock_itemmgr.h"

#include "FB.h"      // CFB：roomless_fb 造空副本
#include "Global.h"  // g_item_list：副本析构的摘除目标

CContainer* CMockItemMgr::load_item(const char* base_name, string unique_id)
{
    load_item_call_count++;
    if (load_factory) return load_factory(base_name);
    return nullptr;
}

CContainer* CMockItemMgr::load_fb(const char* base_name, int flag)
{
    load_fb_call_count++;
    return nullptr;
}

CContainer* CMockItemMgr::find_item(int32_t id)
{
    find_item_call_count++;
    return nullptr;
}

void CMockItemMgr::item_release() { item_release_call_count++; }

void CMockItemMgr::remove_item(int32_t item) { remove_item_call_count++; }

CVendor* CMockItemMgr::random_vendor()
{
    random_vendor_call_count++;
    return nullptr;
}

void CMockItemMgr::reset_counts()
{
    load_item_call_count     = 0;
    load_fb_call_count       = 0;
    find_item_call_count     = 0;
    item_release_call_count  = 0;
    remove_item_call_count   = 0;
    random_vendor_call_count = 0;
}

// ====================================================================
// RegisterFbItemMgr — 注册大厅副本实例支
// ====================================================================

RegisterFbItemMgr::RegisterFbItemMgr() : m_real(create_item_mgr()) {}

CContainer* RegisterFbItemMgr::load_fb(const char* base_name, int flag)
{
    load_fb_call_count++;

    if (roomless_fb) {
        // 空副本：不装载任何房间 → start_map 为空 → move_in 必失败
        CFB* fb = new CFB();
        fb->set("base_name", base_name);
        g_item_list[fb->object_id()] = fb;
        return fb;
    }

    if (real_load_fb) return m_real->load_fb(base_name, flag);
    return nullptr;
}

void RegisterFbItemMgr::remove_item(int32_t item)
{
    CMockItemMgr::remove_item(item);  // 计数（沿用 Mock 的观察面）
    m_real->remove_item(item);        // 真摘除 g_item_list，防悬垂指针
}
