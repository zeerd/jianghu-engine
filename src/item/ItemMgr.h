/**
 * @file ItemMgr.h
 * @brief 物品/副本/商人管理接口，支持 Mock 依赖注入。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

class CContainer;
class CVendor;

using std::string;

/**
 * @brief 物品管理接口，定义加载/查找/释放物品的抽象方法。
 */
class IItemMgr {
public:
    virtual ~IItemMgr() = default;
    virtual CContainer* load_item(const char* base_name, string unique_id = "")
        = 0;
    virtual CContainer* load_fb(const char* base_name, int flag = 0) = 0;
    virtual CContainer* find_item(int32_t id)                        = 0;
    virtual void item_release()                                      = 0;
    virtual void remove_item(int32_t item)                           = 0;
    virtual CVendor* random_vendor()                                 = 0;
};

extern IItemMgr* g_item_mgr;
extern std::unique_ptr<IItemMgr> create_item_mgr();
