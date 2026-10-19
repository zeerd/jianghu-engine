// test_mgr_loaders.cpp — 加载器的所有权移交

//

// ItemMgr / NpcMgr / RoomMgr / SkillMgr 里的加载器遵循同一模式：

//     std::unique_ptr<T> t = Create_X(...);

//     T* obj = t.release();          // 所有权交给全局裸指针表

//     g_XXXList[key] = obj;

//     return obj;

// 这些用例验证三件事：返回的裸指针在局部 unique_ptr 析构后仍可读写、

// 全局表登记的正是同一个对象、重复加载命中缓存不重复创建。

// 每个用例结束时由 RAII 守卫把对象摘除并销毁，不污染后续用例。

#include "pch.h"

#include <map>

#include <memory>

#include <string>

#include "Condition.h"

#include "Container.h"

#include "Global.h"

#include "Item.h"

#include "ItemMgr.h"

#include "Npc.h"
#include "LuaNpc.h"  // CLuaNpc（greeting 在脚本面公开）

#include "NpcMgr.h"

#include "Room.h"

#include "RoomMgr.h"

#include "Skill.h"

#include "SkillMgr.h"

#include "Vendor.h"  // CVendor（random_vendor 用例）

#include "LuaBindDecls.h"  // l_load_npc

#include "LuaEngine.h"  // g_LuaEngine

#include "test_lua_env.h"

#include "test_scoped_room.h"  // ScopedRoom（purge 分支的房间环境）

namespace {

// 用例退出时（含 ASSERT 提前返回）删除全局表中的条目。

template <typename Map>

class ScopedEntry {
public:
    ScopedEntry(Map& map, typename Map::key_type key)

        : m_map(map), m_key(std::move(key))

    {
    }

    ~ScopedEntry()

    {
        auto it = m_map.find(m_key);

        if (it == m_map.end()) return;

        delete it->second;

        m_map.erase(it);
    }

    ScopedEntry(const ScopedEntry&) = delete;

    ScopedEntry& operator=(const ScopedEntry&) = delete;

private:
    Map& m_map;

    typename Map::key_type m_key;
};

// 物件可能已被 move() 挂进房间，销毁前先脱离房间的物件表。

class ScopedItem {
public:
    ScopedItem(MAP2ITEM& map, int32_t id) : m_map(map), m_id(id) {}

    ~ScopedItem()

    {
        auto it = m_map.find(m_id);

        if (it == m_map.end()) return;

        it->second->move(nullptr);

        delete it->second;

        m_map.erase(it);
    }

    ScopedItem(const ScopedItem&) = delete;

    ScopedItem& operator=(const ScopedItem&) = delete;

private:
    MAP2ITEM& m_map;

    int32_t m_id;
};

class MgrLoadersTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

}  // namespace

// ---------------------------------------------------------------------

// SkillMgr::load_skill — 全局表登记 + 缓存命中

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadSkillTransfersOwnership)

{
    const char* name = "mgr_skill_zzz";

    ScopedEntry<MAP2SKILL> skillGuard(g_skill_list, name);

    CSkill* skill = load_skill(name);

    ASSERT_NE(skill, nullptr);

    auto it = g_skill_list.find(name);

    ASSERT_NE(it, g_skill_list.end());

    EXPECT_EQ(it->second, skill);

    skill->set("mgr_probe", 7);

    EXPECT_EQ(skill->query("mgr_probe"), 7);

    EXPECT_EQ(load_skill(name), skill);
}

// ---------------------------------------------------------------------

// SkillMgr::load_condition — 同一模式，状态表

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadConditionTransfersOwnership)

{
    const char* name = "mgr_condition_zzz";

    ScopedEntry<MAP2CND> cndGuard(g_condition_list, name);

    CCondition* cnd = load_condition(name);

    ASSERT_NE(cnd, nullptr);

    auto it = g_condition_list.find(name);

    ASSERT_NE(it, g_condition_list.end());

    EXPECT_EQ(it->second, cnd);

    cnd->set("mgr_probe", 7);

    EXPECT_EQ(cnd->query("mgr_probe"), 7);

    EXPECT_EQ(load_condition(name), cnd);
}

// ---------------------------------------------------------------------

// RoomMgr::load_room — 房间表登记 + 缓存命中

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadRoomTransfersOwnership)

{
    const char* name = "mgr_room_zzz";

    ScopedEntry<MAP2ROOM> roomGuard(g_room_list, name);

    CRoom* room = load_room(name);

    ASSERT_NE(room, nullptr);

    auto it = g_room_list.find(name);

    ASSERT_NE(it, g_room_list.end());

    EXPECT_EQ(it->second, room);

    room->set("mgr_probe", 7);

    EXPECT_EQ(room->query("mgr_probe"), 7);

    EXPECT_EQ(load_room(name), room);
}

// ---------------------------------------------------------------------

// NpcMgr::load_npc — 按 object_id 登记

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadNpcTransfersOwnership)

{
    const char* name = "mgr_npc_zzz";

    CNpc* npc = load_npc(name);

    ASSERT_NE(npc, nullptr);

    const int32_t id = npc->object_id();

    ScopedEntry<MAP2NPC> npcGuard(g_npc_list, id);

    auto it = g_npc_list.find(id);

    ASSERT_NE(it, g_npc_list.end());

    EXPECT_EQ(it->second, npc);

    npc->set("mgr_probe", 7);

    EXPECT_EQ(npc->query("mgr_probe"), 7);
}

// ---------------------------------------------------------------------

// ItemMgr::load_item — 生产实现把物件登记进 g_ItemList

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadItemTransfersOwnership)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    CContainer* item = mgr->load_item("coin", "");

    ASSERT_NE(item, nullptr);

    const int32_t id = item->object_id();

    ScopedItem itemGuard(g_item_list, id);

    auto it = g_item_list.find(id);

    ASSERT_NE(it, g_item_list.end());

    EXPECT_EQ(it->second, item);

    item->set("mgr_probe", 7);

    EXPECT_EQ(item->query("mgr_probe"), 7);
}

// ---------------------------------------------------------------------

// ItemMgr::load_fb — 副本脚本不存在时直接返回，不写全局表

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadFbMissingScriptReturnsNull)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    const size_t before = g_item_list.size();

    EXPECT_EQ(mgr->load_fb("mgr_fb_missing_zzz", 0), nullptr);

    EXPECT_EQ(g_item_list.size(), before);
}

// ---------------------------------------------------------------------

// ItemMgr::load_fb — 成功路径：登记进 g_ItemList 并挂到 fb_registry

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadFbTransfersOwnership)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    CContainer* fb = mgr->load_fb("sample_stronghold_fb", 0);

    ASSERT_NE(fb, nullptr);

    const int32_t id = fb->object_id();

    ScopedItem fbGuard(g_item_list, id);

    auto it = g_item_list.find(id);

    ASSERT_NE(it, g_item_list.end());

    EXPECT_EQ(it->second, fb);

    auto room = g_room_list.find("fb_registry");

    ASSERT_NE(room, g_room_list.end());

    EXPECT_EQ(fb->environment(), room->second);
}

// ---------------------------------------------------------------------

// ItemMgr::load_item — 未知 base 创建失败：记日志并返回空，不污染登记表

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, LoadItemUnknownBaseFallsBackToBlankItem)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    // 换表隔离：兜底件也进登记表，用例收尾自行摘除

    MAP2ITEM saved;

    saved.swap(g_item_list);

    // create_item 对未知 base 不返回空，而是退化为空白 CItem：

    // load 失败仅记日志并把名字打上 "fi-" 前缀，登记照常完成

    CContainer* item = mgr->load_item("mgr_unknown_base_zzz", "");

    ASSERT_NE(item, nullptr);

    EXPECT_EQ(std::string("mgr_unknown_base_zzz"),

              std::string(item->querystr("base_name")))

        << "base_name 原样落库";

    EXPECT_EQ(std::string("fi-mgr_unknown_base_zzz"), std::string(item->name()))

        << "load 失败 → fi- 前缀名（约定而非崩溃）";

    EXPECT_EQ(item, mgr->find_item(item->object_id())) << "登记照常完成";

    g_item_list.swap(saved);

    delete item;
}

// ---------------------------------------------------------------------

// ItemMgr::find_item / remove_item — 登记表命中、落空与摘除

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, FindItemHitMissAndRemove)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    // 换表隔离：本用例的登记不掺入其他套件的残留，收尾原样换回

    MAP2ITEM saved;

    saved.swap(g_item_list);

    CContainer* item = mgr->load_item("coin", "");

    ASSERT_NE(item, nullptr);

    const int32_t id = item->object_id();

    EXPECT_EQ(item, mgr->find_item(id)) << "登记表命中(91,93)";

    EXPECT_EQ(nullptr, mgr->find_item(0x77ABCDEF)) << "未登记 → 0(92)";

    mgr->remove_item(id);  // 摘表(86)

    EXPECT_EQ(nullptr, mgr->find_item(id)) << "摘除后不可见";

    g_item_list.swap(saved);

    delete item;  // 已出登记表，所有权归本用例
}

// ---------------------------------------------------------------------

// ItemMgr::item_release — 逐件销毁并清空登记表（房间内件走 purge 支）

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, ItemRelease_PurgesAndClearsRegistry)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    ScopedRoom room("pk");

    ASSERT_NE(room.get(), nullptr);

    MAP2ITEM saved;

    saved.swap(g_item_list);  // 隔离：只销毁本用例登记的两件

    CContainer* bare = mgr->load_item("coin", "");  // 无环境 → 跳过 purge

    ASSERT_NE(bare, nullptr);

    CContainer* inside = mgr->load_item("coin", "");

    ASSERT_NE(inside, nullptr);

    inside->move(room.get());  // 房内 → 走 purge 支(74-75)

    ASSERT_EQ(2u, g_item_list.size());

    const int32_t bare_id = bare->object_id();

    const int32_t in_id = inside->object_id();

    mgr->item_release();

    EXPECT_TRUE(g_item_list.empty()) << "登记表清空(82)";

    EXPECT_EQ(nullptr, mgr->find_item(bare_id)) << "已销毁(68-79)";

    EXPECT_EQ(nullptr, mgr->find_item(in_id));

    g_item_list.swap(saved);
}

// ---------------------------------------------------------------------

// ItemMgr::random_vendor — 空表返回空；登记表唯一个体必中

// ---------------------------------------------------------------------

TEST_F(MgrLoadersTest, RandomVendorUsesRegistry)

{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();

    ASSERT_NE(mgr, nullptr);

    MAP2VENDOR saved;

    saved.swap(g_vendor_list);  // 隔离

    EXPECT_EQ(nullptr, mgr->random_vendor()) << "空表 → 0(109-110)";

    CVendor* v = new CVendor();

    g_vendor_list[v->object_id()] = v;

    EXPECT_EQ(v, mgr->random_vendor()) << "唯一登记个体必中(103-106)";

    g_vendor_list.erase(v->object_id());

    delete v;

    g_vendor_list.swap(saved);
}

// ====================================================================

// NpcMgr 深路：vendor 注册 / random_npc 遍历 / l_load_npc 绑定

// ====================================================================

TEST_F(MgrLoadersTest, LoadNpcVendorRegistersVendorList)

{
    CNpc* npc = load_npc("fixture_vendor_npc");

    ASSERT_NE(npc, nullptr);

    auto it = g_vendor_list.find(npc->object_id());

    EXPECT_TRUE(npc->query_attr(CMainObject::IS_VENDOR))

        << "type=vendor 解析为 CVendor";

    ASSERT_NE(it, g_vendor_list.end())

        << "IS_VENDOR → 注册进 g_vendor_list(34)";

    EXPECT_EQ(reinterpret_cast<CContainer*>(npc), it->second)

        << "登记指针与本体一致";

    g_vendor_list.erase(npc->object_id());

    g_npc_list.erase(npc->object_id());

    npc->set_heart_beat(0);  // 摘心跳：泄漏对象不得再被全局心跳触及

    npc->move(nullptr);

    npc->set_destructed();

    // 不 delete：create/setup 曾把对象绑定进 lua，delete 会留悬垂引用
}

TEST_F(MgrLoadersTest, RandomNpcPicksFromRegistry)

{
    auto* npc = new CNpc();

    npc->set_name("游荡者", "wanderer1");

    g_npc_list[npc->object_id()] = npc;

    CNpc* picked = random_npc();

    EXPECT_EQ(npc, picked) << "注册表唯一成员被选中(92-94)";

    g_npc_list.erase(npc->object_id());

    delete npc;
}

TEST_F(MgrLoadersTest, LoadNpcBindPushesResult)

{
    lua_State* L = g_LuaEngine->get_state();

    int top0 = lua_gettop(L);

    // Call load_npc through real lua (pcall frame), not by invoking the C

    // bind directly - direct invocation bypasses the lua call protocol and

    // corrupts engine state observed downstream (Resident.Housename crash).

    std::map<int32_t, CNpc*> before = g_npc_list;

    int rc = luaL_dostring(L,

                           "local n = load_npc('fixture_vendor_npc'); "

                           "return n ~= nil");

    ASSERT_EQ(LUA_OK, rc) << lua_tostring(L, -1);

    EXPECT_TRUE(lua_toboolean(L, -1)) << "lua side load_npc returns object";

    lua_pop(L, 1);

    EXPECT_EQ(top0, lua_gettop(L)) << "lua stack balanced";

    // Remove the newly created npc (kept alive: it was bound in lua).

    for (auto it = g_npc_list.begin(); it != g_npc_list.end();) {
        if (before.find(it->first) == before.end()) {
            it->second->set_heart_beat(0);

            it->second->move(nullptr);

            it = g_npc_list.erase(it);
        }

        else {
            ++it;
        }
    }
}

// ====================================================================

// RoomMgr::draw_map：把注册表里的房间键写进 map.txt（54-64 全程）

// ====================================================================

TEST_F(MgrLoadersTest, DrawMapWritesRegisteredRooms)

{
    CRoom probe;

    probe.set_name("出图房", "drawmap_probe");

    g_room_list["drawmap_probe"] = &probe;

    draw_map();

    FILE* fp = fopen("map.txt", "rt");

    ASSERT_NE(fp, nullptr) << "map.txt 已生成(57-58)";

    char buf[4096] = {0};

    fread(buf, 1, sizeof(buf) - 1, fp);

    fclose(fp);

    EXPECT_NE(std::string(buf).find("drawmap_probe"), std::string::npos)

        << "注册房间键写入文件(60-61)";

    remove("map.txt");  // 清理副作用

    g_room_list.erase("drawmap_probe");
}
