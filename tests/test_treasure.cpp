// test_treasure.cpp - 专属存盘路由（save_table/save_key）契约测试
//
// 通用机制：脚本声明 save_table/save_key（create() 镜像进 dbase），
// C++ 存/取/删按该声明路由表名与对象 id；未声明走默认（t_item + base_name）。
// C++ 不认识 "treasure"/"t_treasure" 等任何数据集名称。
//
// 覆盖：
//   1. create() 镜像脚本字段 + save_key 为空时自动生成 UUID、自动 can_save；
//   2. save()/load()/purge()/remove() 按声明路由表名与 id；
//   3. 未声明路由的普通物品仍走 t_item/base_name，purge 不触库；
//   4. create_item(name, unique_id) 按 save_key 恢复实例存档；
//   5. 箱子析构：声明专属表的箱内物件被 purge（存档行删除）。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Box.h"
#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "HeartBeat.h"
#include "IDatabase.h"
#include "Item.h"
#include "ItemMgr.h"
#include "Mapping.h"
#include "Npc.h"
#include "Room.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"

std::unique_ptr<CContainer> create_item(string base_name, string unique_id);

extern IDatabase* g_db;

namespace {

// 按 load_item 的完整时序造物件：create_item + base_name + create + setup
CContainer* make_item(const char* name, const char* unique_id = "")
{
    std::unique_ptr<CContainer> t = create_item(name, unique_id);
    if (!t) return nullptr;
    t->set("base_name", name);
    t->create();
    t->setup();
    CContainer* obj               = t.release();
    g_item_list[obj->object_id()] = obj;
    return obj;
}

void destroy_obj(CContainer* obj)
{
    if (!obj) return;
    obj->move(nullptr);
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.remove(obj);
    }
    delete obj;
}

// 可存盘的普通 CItem（C++ 侧 CAN_SAVE 默认 0，测试用子类放行默认路由）
class SaveableItem : public CItem {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == CAN_SAVE) return 1;
        return CItem::query_attr(attr);
    }
};

class TreasureRouteTest : public ::testing::Test {
protected:
    MockDatabase db;
    // 换入前保存进程共享管理器（test_env 的 g_testItemMgr），归还而非置空
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr mgr;  // destruct 走 g_item_mgr->remove_item
    std::vector<CContainer*> m_owned;

    void SetUp() override
    {
        ensure_lua_env();
        g_db       = &db;
        saved_mgr  = g_item_mgr;
        g_item_mgr = &mgr;
    }
    void TearDown() override
    {
        for (auto* p : m_owned) destroy_obj(p);
        m_owned.clear();
        g_item_mgr = saved_mgr;
        g_db       = nullptr;
    }

    CContainer* own(CContainer* obj)
    {
        m_owned.push_back(obj);
        return obj;
    }
};

}  // namespace

// ====================================================================
// create()：脚本字段镜像 + UUID 自动生成 + 自动 can_save
// ====================================================================
TEST_F(TreasureRouteTest, CreateMirrorsScriptRouteFields)
{
    CContainer* t = own(make_item("treasure_test"));
    ASSERT_NE(t, nullptr);

    EXPECT_STREQ(t->querystr("save_table"), "t_treasure");
    EXPECT_STREQ(t->querystr("save_key"), "unique_id");
    EXPECT_EQ(t->query_attr(CMainObject::CAN_SAVE), 1);
    // save_key 指向的键为空 → 自动生成实例 id（UUID 形态：含 '-'）
    EXPECT_NE(std::string(t->querystr("unique_id")).find('-'),
              std::string::npos);
}

TEST_F(TreasureRouteTest, CreateKeepsExistingInstanceId)
{
    CContainer* t = own(make_item("treasure_test", "fixed-uuid-1"));
    ASSERT_NE(t, nullptr);
    EXPECT_STREQ(t->querystr("unique_id"), "fixed-uuid-1");
}

// ====================================================================
// save()/load()/purge()/remove()：按声明路由表名与 id
// ====================================================================
TEST_F(TreasureRouteTest, SaveRoutesToDeclaredTableWithSaveKeyValue)
{
    CContainer* t = own(make_item("treasure_test"));
    ASSERT_NE(t, nullptr);

    ASSERT_EQ(t->save(), 1);

    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("insert t_treasure"), std::string::npos) << sql;
    EXPECT_NE(sql.find(t->querystr("unique_id")), std::string::npos) << sql;
    // 不得用 base_name 作 obj_id
    EXPECT_EQ(sql.find("values('treasure_test'"), std::string::npos) << sql;
}

TEST_F(TreasureRouteTest, LoadRoutesToDeclaredTableWithSaveKeyValue)
{
    // 预置一条专属表存档行（obj_data 用 CMapping 缓冲格式）
    CMapping saved;
    saved.set("name", "存盘回来的宝刀");
    db.SetQueryResultMatching(
        "select obj_data from t_treasure",
        MockDatabase::MakeSingleRow({saved.get_buffer()}));

    CContainer* t = own(make_item("treasure_test", "uuid-load-1"));
    ASSERT_NE(t, nullptr);

    ASSERT_EQ(t->load(), 1);
    EXPECT_STREQ(t->querystr("name"), "存盘回来的宝刀");
    EXPECT_NE(db.GetLastSql().find("select obj_data from t_treasure"),
              std::string::npos);
}

TEST_F(TreasureRouteTest, PurgeDeletesRoutedRow)
{
    CContainer* t = own(make_item("treasure_test", "uuid-purge-1"));
    ASSERT_NE(t, nullptr);

    ASSERT_EQ(t->purge(), 1);
    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("delete from t_treasure"), std::string::npos) << sql;
    EXPECT_NE(sql.find("uuid-purge-1"), std::string::npos) << sql;
}

TEST_F(TreasureRouteTest, PurgeFailReturnsZero)
{
    CContainer* t = own(make_item("treasure_test", "uuid-purge-2"));
    ASSERT_NE(t, nullptr);
    db.SetShouldFail(true);
    EXPECT_EQ(t->purge(), 0);
}

TEST_F(TreasureRouteTest, RemoveRoutesToDeclaredTable)
{
    CContainer* t = own(make_item("treasure_test"));
    ASSERT_NE(t, nullptr);

    t->remove();
    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("delete from t_treasure"), std::string::npos) << sql;
    EXPECT_NE(sql.find(t->querystr("unique_id")), std::string::npos) << sql;
}

// ====================================================================
// 未声明路由的物件：默认 t_item + base_name（回归护栏）
// ====================================================================
TEST_F(TreasureRouteTest, PlainItemStillRoutesToTItemByBaseName)
{
    SaveableItem* it = new SaveableItem();
    own(it);
    it->set("base_name", "plain_item");
    it->set("name", "普通物品");

    ASSERT_EQ(it->save(), 1);
    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("insert t_item"), std::string::npos) << sql;
    EXPECT_NE(sql.find("plain_item"), std::string::npos) << sql;

    ASSERT_EQ(it->purge(), 1);
    EXPECT_EQ(db.GetLastSql().find("delete from"), std::string::npos)
        << "未声明专属表的物件 purge 不应触库";
}

// ====================================================================
// 箱子析构：声明专属表的箱内物件被 purge
// ====================================================================
TEST_F(TreasureRouteTest, BoxDestructPurgesRoutedItem)
{
    CRoom env;
    env.set("name", "box_room");
    env.set("base_name", "box_room");

    CBox* box = new CBox();
    box->set("db_room_id", "treasure_box_1");
    box->move(&env);

    CContainer* t = own(make_item("treasure_test", "uuid-box-1"));
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->move(box), 1);

    db.Clear();
    delete box;  // 环境非存盘房间 → 析构清理箱内物件

    EXPECT_TRUE(db.ExecutedContaining("delete from t_treasure"))
        << "箱内专属物件的存档行应随箱子析构删除";
}

// ====================================================================
// 路由分支补全：save_table 有、save_key 无 → 专属表 + base_name 作 id
// ====================================================================
TEST_F(TreasureRouteTest, RoutedWithoutSaveKeyUsesBaseNameAsId)
{
    CContainer* t = own(make_item("routed_nokey_test"));
    ASSERT_NE(t, nullptr);

    EXPECT_STREQ(t->querystr("save_table"), "t_routed");
    EXPECT_STREQ(t->querystr("save_key"), "");

    ASSERT_EQ(t->save(), 1);
    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("insert t_routed"), std::string::npos) << sql;
    EXPECT_NE(sql.find("values('routed_nokey_test'"), std::string::npos) << sql;
}

// ====================================================================
// 默认路由：角色 → t_npc
// ====================================================================
TEST_F(TreasureRouteTest, CharacterDefaultsToNpcTable)
{
    CNpc npc;
    npc.set("name", "路人");
    npc.set("base_name", "npc_route_test");
    npc.set("can_save", 1);

    ASSERT_EQ(npc.save(), 1);
    const std::string sql = db.GetLastSql();
    EXPECT_NE(sql.find("insert t_npc"), std::string::npos) << sql;
    EXPECT_NE(sql.find("npc_route_test"), std::string::npos) << sql;
}

// ====================================================================
// 存盘写入失败：db_save_container 返回 false → save()=0
// ====================================================================
TEST_F(TreasureRouteTest, SaveReturnsZeroWhenWriteFails)
{
    CContainer* t = own(make_item("treasure_test", "uuid-savefail"));
    ASSERT_NE(t, nullptr);

    db.SetShouldFail(true);  // 查存在性与写入都失败
    EXPECT_EQ(t->save(), 0);
}

// ====================================================================
// 载入坏数据：obj_data 不是合法映射缓冲 → fail_ob 并返回 0
// ====================================================================
TEST_F(TreasureRouteTest, LoadCorruptSavedataMarksFailOb)
{
    db.SetQueryResultMatching("select obj_data from t_treasure",
                              MockDatabase::MakeSingleRow({"~~not a mapping"}));

    CContainer* t = own(make_item("treasure_test", "uuid-corrupt"));
    ASSERT_NE(t, nullptr);

    EXPECT_EQ(t->load(), 0);
    EXPECT_EQ(t->query("fail_ob"), 2);
}

// ====================================================================
// 基类选择：脚本未声明 base → 沿用 type 字段（CLuaItem 分支）
// ====================================================================
TEST_F(TreasureRouteTest, ItemWithoutBaseFieldFallsBackToType)
{
    CContainer* t = own(make_item("plain_test"));
    ASSERT_NE(t, nullptr);

    // type="food" → CLuaItem（非装备基类）：无专属路由字段
    EXPECT_STREQ(t->querystr("save_table"), "");
    EXPECT_EQ(t->query_attr(CMainObject::IS_FOOD), 1);
}

// ====================================================================
// DB 查询失败（非异常）：存盘走 INSERT、载入返回 0
// （save_impl/load_impl 的 catch(...) 为防御块，db_* 助手已内部吞异常，
//  不经由常规 DB 故障触达，故不在此强测）
// ====================================================================
TEST_F(TreasureRouteTest, LoadReturnsZeroWhenQueryFails)
{
    CContainer* t = own(make_item("treasure_test", "uuid-qfail"));
    ASSERT_NE(t, nullptr);
    db.SetQueryErrorCode(1);  // query 返回非 0（失败）
    EXPECT_EQ(t->load(), 0);
}
