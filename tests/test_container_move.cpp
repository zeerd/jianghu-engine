// test_container_move.cpp - Unit tests for CContainer::move.

//

// Tests the real Container.cpp code (compiled in the project).

// Stubs for dependencies are in test_hooks.cpp.

//

// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32

// Run:    tests\Debug\tests.exe --gtest_filter=ContainerMove*

//

#include "pch.h"

#include "Item.h"

#include "Money.h"  // CMCoin（唯一 CAN_COMBINE，自动合并）

#include "HeartBeat.h"  // g_destruct_list（destruct 出队）          // CItem 成堆语义

#include "test_lua_env.h"  // load_room("pk") 需脚本环境

#include "Container.h"

#include "CObject.h"

// ====================================================================

// Test fixture

// ====================================================================

class ContainerMoveTest : public ::testing::Test {
protected:
    CContainer* room;

    CContainer* item;

    void SetUp() override

    {
        room = new CContainer();

        item = new CContainer();
    }

    void TearDown() override

    {
        delete item;

        delete room;
    }
};

// ====================================================================

// Basic move tests

// ====================================================================

TEST_F(ContainerMoveTest, MoveToRoom)

{
    int result = item->move(room);

    EXPECT_EQ(result, 1);

    EXPECT_EQ(item->environment(), room);
}

TEST_F(ContainerMoveTest, MoveToNull)

{
    item->move(room);

    int result = item->move(nullptr);

    EXPECT_EQ(result, 1);

    EXPECT_EQ(item->environment(), nullptr);
}

TEST_F(ContainerMoveTest, MoveTwice)

{
    CContainer* room2 = new CContainer();

    item->move(room);

    int result = item->move(room2);

    EXPECT_EQ(result, 1);

    EXPECT_EQ(item->environment(), room2);

    delete room2;
}

TEST_F(ContainerMoveTest, MoveSelfFails)

{
    item->move(room);

    int result = item->move(item);

    EXPECT_EQ(result, 0);
}

// ====================================================================

// Object ID

// ====================================================================

TEST_F(ContainerMoveTest, ObjectIDIncrements)

{
    int id1 = room->object_id();

    CContainer* another = new CContainer();

    int id2 = another->object_id();

    EXPECT_GT(id2, id1);

    delete another;
}

// ====================================================================

// Query

// ====================================================================

TEST_F(ContainerMoveTest, QueryIsContainer)

{
    EXPECT_EQ(room->query_attr(CMainObject::IS_CONTAINER), 1);
}

// ====================================================================

// Weight and encumbrance

// ====================================================================

TEST_F(ContainerMoveTest, WeightDefault) { EXPECT_EQ(room->weight(), 0); }

TEST_F(ContainerMoveTest, SetWeight)

{
    room->set_weight(100);

    EXPECT_EQ(room->weight(), 100);
}

TEST_F(ContainerMoveTest, QueryEncumbDefault)

{
    EXPECT_EQ(room->query_encumb(), 0);
}

TEST_F(ContainerMoveTest, QueryMaxEncumbrance)

{
    EXPECT_EQ(room->query_max_encumbrance(), 0);
}

// ====================================================================

// Item list operations

// ====================================================================

TEST_F(ContainerMoveTest, GetItemList)

{
    DTItemList* list = room->get_item_list();

    EXPECT_NE(list, nullptr);

    EXPECT_EQ(list->size(), (size_t)0);
}

TEST_F(ContainerMoveTest, ItemListAfterMove)

{
    item->move(room);

    DTItemList* list = room->get_item_list();

    EXPECT_EQ(list->size(), (size_t)1);
}

// ====================================================================

// Query methods

// ====================================================================

TEST_F(ContainerMoveTest, QueryEntireDatabase)

{
    CMapping* db = room->query_entire_dbase();

    EXPECT_NE(db, nullptr);
}

TEST_F(ContainerMoveTest, QueryTempDatabase)

{
    CMapping* temp = room->query_entire_temp_dbase();

    EXPECT_NE(temp, nullptr);
}

TEST_F(ContainerMoveTest, QueryAmount) { EXPECT_EQ(item->query_amount(), 1); }

TEST_F(ContainerMoveTest, QueryMaxAmount)

{
    EXPECT_EQ(item->query_max_amount(), 1);
}

// ====================================================================

// 成堆物品不可携带 → 拆分改投(33-39)

// ====================================================================

TEST_F(ContainerMoveTest, MoveStackedItemSplitsAndSendsToPk)

{
    ASSERT_NE(ensure_lua_env(), nullptr);  // load_room("pk") 走脚本环境

    CItem* pile = new CItem();

    pile->set_name("石堆", "stack_pile");

    pile->set_amount(3);

    ASSERT_EQ(3, pile->query_amount()) << "前置：堆量已设";

    int r = pile->move(room);

    EXPECT_EQ(0, r) << "成堆 → 拆分支后直接返回(33-39)";

    EXPECT_EQ(1, pile->query_amount()) << "拆为单件(38)";

    EXPECT_EQ(3, pile->query("数目")) << "记录原堆量(35)";

    pile->move(nullptr);  // 无论 pk 房是否存在都摘出

    delete pile;
}

// ====================================================================
// CItem::query_max_amount（CItem 实例才命中 124-126）
// ====================================================================
TEST_F(ContainerMoveTest, CItemMaxAmountDefaultsAndTracks)
{
    CItem* it = new CItem();
    it->set_name("药丸", "ma_pill");
    EXPECT_EQ(1, it->query_max_amount()) << "未设堆量 → 1(124-126)";
    it->set_amount(4);
    EXPECT_EQ(4, it->query_max_amount()) << "首设堆量即记为上限(133-134)";
    delete it;
}

// ====================================================================
// init()：非 user 环境下的成堆拆分改投(249-266)
// ====================================================================
TEST_F(ContainerMoveTest, ItemInitSplitsStackToPk)
{
    ASSERT_NE(ensure_lua_env(), nullptr);  // load_room("pk") 走脚本环境
    CItem* it = new CItem();
    it->set_name("石堆", "init_pile");
    it->move(room);     // 先单件入容器（避免 move 时的即时拆分抢跑）
    it->set_amount(3);  // 入容器后再堆量 → init 才见 amount>1

    it->init();

    EXPECT_EQ(3, it->query("数目")) << "先记录原堆量(263)";
    EXPECT_EQ(1, it->query_amount()) << "拆为单件(265)";
    EXPECT_NE(room, it->environment()) << "已改投 pk/空环境(266)";
    it->move(nullptr);
    delete it;
}

// ====================================================================
// check_auto_combine：同容器同名堆合并(71-73)
// ====================================================================
TEST_F(ContainerMoveTest, MoneyAutoCombineOnMove)
{
    CMCoin* a = new CMCoin();
    a->create();
    a->set_amount(1);
    CMCoin* b = new CMCoin();
    b->create();
    b->set_amount(2);

    a->move(item);  // 尚无同名伴 → 原样入容器
    b->move(item);  // 入容器即自动 check_auto_combine → 吞并 a(71-73)

    EXPECT_EQ(3, b->query_amount()) << "move 后同名自动合并：total=1+2(71-73)";
    EXPECT_EQ(1u, item->get_item_list()->size()) << "伴生堆已 destruct 出列表";

    // a 已 destruct 入队 → 出队并释放，防全局队列残留
    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);
        g_destruct_list.remove(a);
    }
    delete a;

    b->move(nullptr);
    delete b;
}