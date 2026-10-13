/** @file ItemMgr.cpp @brief 物品 / 副本 / 商人管理
 * 架构：IItemMgr 接口 + CItemMgr 生产实现 + 自由函数转发包装 测试时通过
 * g_ItemMgr 注入 CMockItemMgr */

#include "ItemMgr.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "Item.h"
#include "Npc.h"
#include "../room/FB.h"
#include "../room/FBRoom.h"
#include "Vendor.h"
#include "../util/Logger.h"

//---------------------------------------------------------
// 生产实现 CItemMgr
//---------------------------------------------------------

// 外部工厂函数（定义在 SetupObjects.cpp / FBRoom.cpp）
std::unique_ptr<CContainer> create_item(string base_name, string unique_id);
std::unique_ptr<CFB> create_fb(string fb_name);

class CItemMgr : public IItemMgr {
public:
    CContainer* load_item(const char* base_name, string unique_id = "") override
    {
        std::unique_ptr<CContainer> t = create_item(base_name, unique_id);
        if (!t) {
            LOG_ERROR("物件%s创建失败！", base_name);
            return 0;
        }

        t->set("base_name", base_name);
        t->create();
        t->setup();
        CContainer* obj               = t.release();
        g_item_list[obj->object_id()] = obj;

        return obj;
    }

    CContainer* load_fb(const char* base_name, int flag) override
    {
        std::unique_ptr<CContainer> t = create_fb(base_name);
        if (!t) {
            LOG_ERROR("物件%s创建失败！", base_name);
            return 0;
        }

        t->set("base_name", base_name);
        t->create(flag);
        t->setup();
        t->set("no_get", 1);
        CContainer* obj               = t.release();
        g_item_list[obj->object_id()] = obj;
        obj->move(load_room("fb_registry"));

        return obj;
    }

    void item_release() override
    {
        LOG_DEBUG("Item_Release():started");

        iterator_item p;
        CContainer* item;

        for (p = g_item_list.begin(); p != g_item_list.end(); p++) {
            item = p->second;
            item->set_destructed();
            item->set_heart_beat(0);

            CContainer* env = item->environment();
            if (env != NULL && env->is_room()) {
                item->purge();
            }
            item->move(NULL);
            item->clean_up();
            delete item;
        }

        g_item_list.clear();
        LOG_DEBUG("Item_Release():finished");
    }

    void remove_item(int32_t item) override { g_item_list.erase(item); }

    CContainer* find_item(int32_t id) override
    {
        iterator_item p;
        p = g_item_list.find(id);
        if (p == g_item_list.end()) return 0;
        return p->second;
    }

    CVendor* random_vendor() override
    {
        MAP2VENDOR::iterator p2;
        int i = 0;

        int index = g_utils.random(g_vendor_list.size());

        for (p2 = g_vendor_list.begin(); p2 != g_vendor_list.end(); p2++) {
            CVendor* npc = p2->second;
            if (i == index) return npc;
            i++;
        }

        return 0;
    }
};

//---------------------------------------------------------
// 全局实例
//---------------------------------------------------------
IItemMgr* g_item_mgr = nullptr;

// 创建生产实现（服务器启动时调用），返回拥有所有权的 unique_ptr；
// 调用方将 .get() 存入非拥有的 g_ItemMgr 指针。
std::unique_ptr<IItemMgr> create_item_mgr()
{
    return std::make_unique<CItemMgr>();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// load_item(name) -> object
int l_load_item(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    lua_bind_push_object(L, g_item_mgr->load_item(name));
    return 1;
}

// find_item(objectid) -> object - 按全局 object_id 查找物品（自制武器方位查询）
int l_find_item(lua_State* L)
{
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    lua_bind_push_object(L, g_item_mgr->find_item(id));
    return 1;
}
