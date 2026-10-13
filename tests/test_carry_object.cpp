// test_carry_object.cpp - Unit tests for CNpc::carry_object with CAN_COMBINE
// logic.
//
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    .\tests\Debug\tests.exe
//
#include "pch.h"
#include <string>
#include "CObject.h"
#include "Container.h"
#include "Npc.h"
#include "mock/mock_itemmgr.h"

// ====================================================================
// Mock item: tracks method calls and controls CAN_COMBINE behavior
// ====================================================================
class MockItem : public CContainer {
public:
    bool m_can_combine;
    int m_set_amount_calls;
    int32_t m_last_set_amount;
    int m_move_calls;
    CContainer* m_move_target;
    int m_create_calls;

    MockItem(bool can_combine = false)
        : CContainer(),
          m_can_combine(can_combine),
          m_set_amount_calls(0),
          m_last_set_amount(0),
          m_move_calls(0),
          m_move_target(nullptr),
          m_create_calls(0)
    {
    }

    int32_t query_attr(int attr) override
    {
        if (attr == CAN_COMBINE) return m_can_combine ? 1 : 0;
        return CContainer::query_attr(attr);
    }

    void set_amount(int32_t amount) override
    {
        m_set_amount_calls++;
        m_last_set_amount = amount;
    }

    int move(CContainer* me) override
    {
        m_move_calls++;
        m_move_target = me;
        return 1;
    }

    void create(int flag) override { m_create_calls++; }
};

// ====================================================================
// Mock tracking (供测试断言)
// ====================================================================
static const int MAX_MOCKS = 64;
static MockItem* g_mocks[MAX_MOCKS];
static int g_mock_count           = 0;
static int g_load_item_call_count = 0;

static void reset_mocks()
{
    for (int i = 0; i < g_mock_count; i++) {
        delete g_mocks[i];
        g_mocks[i] = nullptr;
    }
    g_mock_count           = 0;
    g_load_item_call_count = 0;
}

// ====================================================================
// 工厂函数：CMockItemMgr 通过此函数创建 mock 物品
// ====================================================================
static CContainer* mock_load_item_factory(const char* base_name)
{
    g_load_item_call_count++;

    // "yuchandan" (food/yaopin) and "kuihuazhen" (autocombineequip) are
    // stackable
    bool can_combine = (strcmp(base_name, "yuchandan") == 0
                        || strcmp(base_name, "kuihuazhen") == 0);

    MockItem* item = new MockItem(can_combine);
    if (g_mock_count < MAX_MOCKS) g_mocks[g_mock_count++] = item;
    return item;
}

// ====================================================================
// Test fixture: inject CMockItemMgr via g_ItemMgr
// ====================================================================
class CarryObjectTest : public ::testing::Test {
protected:
    CMockItemMgr mockItemMgr;
    // 换入专属 mock 前保存进程共享管理器（test_env 的 g_testItemMgr），
    // TearDown 归还——置空会让下游所有隐式触达 g_item_mgr 的路径踩空
    IItemMgr* saved_mgr = nullptr;

    void SetUp() override
    {
        reset_mocks();
        mockItemMgr.reset_counts();
        mockItemMgr.load_factory = mock_load_item_factory;
        saved_mgr                = g_item_mgr;
        g_item_mgr               = &mockItemMgr;
    }

    void TearDown() override { g_item_mgr = saved_mgr; }
};

// ====================================================================
// Tests
// ====================================================================
TEST_F(CarryObjectTest, StackableItem_SingleLoad)
{
    CNpc npc;

    // yuchandan is stackable (CAN_COMBINE=1), amount=5
    CContainer* result = npc.carry_object("yuchandan", 5);

    EXPECT_EQ(g_load_item_call_count, 1);
    EXPECT_EQ(mockItemMgr.load_item_call_count, 1);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_mocks[0]->m_set_amount_calls, 1);
    EXPECT_EQ(g_mocks[0]->m_last_set_amount, 5);
    EXPECT_EQ(g_mocks[0]->m_move_calls, 1);
    EXPECT_EQ(g_mocks[0]->m_move_target, &npc);
}

TEST_F(CarryObjectTest, NonStackableItem_MultipleLoads)
{
    CNpc npc;

    // "caoxie" is non-stackable, amount=3 → should create 3 separate items
    CContainer* result = npc.carry_object("caoxie", 3);

    EXPECT_EQ(g_load_item_call_count, 3);
    EXPECT_NE(result, nullptr);

    // Each item should have amount=1
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(g_mocks[i]->m_set_amount_calls, 1);
        EXPECT_EQ(g_mocks[i]->m_last_set_amount, 1);
        EXPECT_EQ(g_mocks[i]->m_move_calls, 1);
        EXPECT_EQ(g_mocks[i]->m_move_target, &npc);
    }
}

TEST_F(CarryObjectTest, NonStackableItem_Amount1_SingleLoad)
{
    CNpc npc;

    // Non-stackable with amount=1 → single item
    CContainer* result = npc.carry_object("caoxie", 1);

    EXPECT_EQ(g_load_item_call_count, 1);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_mocks[0]->m_set_amount_calls, 1);
    EXPECT_EQ(g_mocks[0]->m_last_set_amount, 1);
    EXPECT_EQ(g_mocks[0]->m_move_calls, 1);
}

TEST_F(CarryObjectTest, Amount0_DefaultsTo1)
{
    CNpc npc;

    // amount=0 should default to 1
    CContainer* result = npc.carry_object("caoxie", 0);

    EXPECT_EQ(g_load_item_call_count, 1);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_mocks[0]->m_last_set_amount, 1);
}

TEST_F(CarryObjectTest, NegativeAmount_DefaultsTo1)
{
    CNpc npc;

    // Negative amount should default to 1
    CContainer* result = npc.carry_object("caoxie", -5);

    EXPECT_EQ(g_load_item_call_count, 1);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_mocks[0]->m_last_set_amount, 1);
}

TEST_F(CarryObjectTest, NonStackableItem_LargeAmount)
{
    CNpc npc;

    // Non-stackable with amount=20 → 20 items
    CContainer* result = npc.carry_object("mandao", 20);

    EXPECT_EQ(g_load_item_call_count, 20);
    EXPECT_NE(result, nullptr);

    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(g_mocks[i]->m_last_set_amount, 1);
        EXPECT_EQ(g_mocks[i]->m_move_calls, 1);
    }
}

TEST_F(CarryObjectTest, StackableItem_NotMultipleLoads)
{
    CNpc npc;

    // kuihuazhen is stackable, amount=10 → single load
    CContainer* result = npc.carry_object("kuihuazhen", 10);

    EXPECT_EQ(g_load_item_call_count, 1);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_mocks[0]->m_last_set_amount, 10);
    EXPECT_EQ(g_mocks[0]->m_move_calls, 1);
}

TEST_F(CarryObjectTest, WithIdx_PassedToCreate)
{
    CNpc npc;

    // idx parameter should be passed to create()
    CContainer* result = npc.carry_object("caoxie", 2, 42);

    EXPECT_EQ(g_load_item_call_count, 2);
    // Each item's create() should have been called
    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(g_mocks[i]->m_create_calls, 1);
    }
}
