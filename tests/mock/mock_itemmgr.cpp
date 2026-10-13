// mock_itemmgr.cpp — Mock implementation of IItemMgr for testing
//
#include "mock_itemmgr.h"

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
