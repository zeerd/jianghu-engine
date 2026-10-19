// test_container_ownership.cpp - Unit tests for CContainer call_out ownership
// (m_CalloutList migrated from raw CALLOUTNODE* to std::unique_ptr).
//
// These verify the use-after-free fix: erase() destroys the node, so the
// callback fields must be snapshotted before erase(). If they were read after
// erase(), the params would be garbage / crash under ASAN.
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Container.h"
#include "Char.h"
#include "Channel.h"

// Global counters set by the test callbacks.
static int g_call_count  = 0;
static int g_last_param1 = -1;
static int g_last_param2 = -1;
static void* g_last_self = nullptr;

// 被兄弟节点移除的"受害者"回调，独立计数。
static int g_sibling_count = 0;

static void TestCallback(CContainer* me, int32_t p1, int32_t p2)
{
    g_call_count++;
    g_last_param1 = p1;
    g_last_param2 = p2;
    g_last_self   = me;
}

static void SiblingCallback(CContainer* me, int32_t p1, int32_t p2)
{
    g_sibling_count++;
    g_last_param1 = p1;
    g_last_param2 = p2;
    g_last_self   = me;
}

// 回调执行期间按 id 移除另一条挂起节点（Lua remove_call_out 的真实路径）：
// param2 携带目标回调 ID。
static void RemoveSuccessorByIdCallback(CContainer* me, int32_t p1, int32_t p2)
{
    g_call_count++;
    g_last_param1 = p1;
    g_last_param2 = p2;
    g_last_self   = me;
    me->remove_call_out_by_id(p2);
}

// 回调执行期间移除同名兄弟节点（remove_call_out 首匹配语义）。
static void ReentrantRemoveCallback(CContainer* me, int32_t p1, int32_t p2)
{
    g_call_count++;
    g_last_param1 = p1;
    g_last_param2 = p2;
    g_last_self   = me;
    me->remove_call_out(&ReentrantRemoveCallback, 0);
}

class ContainerOwnershipTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_call_count    = 0;
        g_last_param1   = -1;
        g_last_param2   = -1;
        g_last_self     = nullptr;
        g_sibling_count = 0;
    }
};

// ---- call_out + heart_beat: delay reaching 0 fires the callback ----
TEST_F(ContainerOwnershipTest, HeartBeat_FiresDueCallout)
{
    CContainer c;
    c.call_out(&TestCallback, 1, 111, 222);  // delay 1 -> fires on first beat
    c.heart_beat();
    EXPECT_EQ(g_call_count, 1);
    // Params must be the values passed at call_out time (proves the fields
    // were snapshotted before erase(), not read from freed memory).
    EXPECT_EQ(g_last_param1, 111);
    EXPECT_EQ(g_last_param2, 222);
    EXPECT_EQ(g_last_self, &c);
    c.set_heart_beat(0);
}

// ---- call_out with delay > 1 does not fire on first beat ----
TEST_F(ContainerOwnershipTest, HeartBeat_NotYetDue)
{
    CContainer c;
    c.call_out(&TestCallback, 3, 1, 2);
    c.heart_beat();  // delay 3 -> 2, not fired
    EXPECT_EQ(g_call_count, 0);
    c.heart_beat();  // 2 -> 1, not fired
    EXPECT_EQ(g_call_count, 0);
    c.heart_beat();  // 1 -> 0, fired
    EXPECT_EQ(g_call_count, 1);
    c.set_heart_beat(0);
}

// ---- 回调执行期间移除它的后继节点：心跳的迭代器不得悬垂 ----
// push_front 使后注册者位于链表头：先注册受害者，再注册移除者。
TEST_F(ContainerOwnershipTest, HeartBeat_CallbackRemovesSuccessorById)
{
    CContainer c;
    c.call_out(&SiblingCallback, 1, 4242, 0);                // 受害者 -> 后继
    c.call_out(&RemoveSuccessorByIdCallback, 1, 111, 4242);  // 移除者 -> 链表头

    c.heart_beat();
    EXPECT_EQ(g_call_count, 1);     // 移除者执行
    EXPECT_EQ(g_sibling_count, 0);  // 后继被移除，永不执行

    c.heart_beat();  // 后继已删除，不得再次触发
    EXPECT_EQ(g_call_count, 1);
    EXPECT_EQ(g_sibling_count, 0);
    c.set_heart_beat(0);
}

// ---- 同一场景在战斗延迟链路上同样成立 ----
TEST_F(ContainerOwnershipTest, EvaluateCombat_CallbackRemovesSuccessorById)
{
    CContainer c;
    c.call_out_combat(&SiblingCallback, 0, 4242, 0);  // delay 0 = 战斗结束哨兵
    c.call_out_combat(&RemoveSuccessorByIdCallback, 0, 111, 4242);

    c.evaluate_call_out_combat();
    EXPECT_EQ(g_call_count, 1);
    EXPECT_EQ(g_sibling_count, 0);
    c.set_heart_beat(0);
}

// ---- 回调移除同名兄弟：执行中的节点由心跳回收，兄弟被取消 ----
TEST_F(ContainerOwnershipTest, HeartBeat_RemoveSameFuncSiblingDuringCallback)
{
    CContainer c;
    c.call_out(&ReentrantRemoveCallback, 1, 1, 0);  // 兄弟 -> 后继
    c.call_out(&ReentrantRemoveCallback, 1, 2, 0);  // 移除者 -> 链表头

    c.heart_beat();
    // 两条 delay 都已到点：若兄弟未被取消，本拍就会执行第二次。
    EXPECT_EQ(g_call_count, 1);
    c.heart_beat();
    EXPECT_EQ(g_call_count, 1);
    c.set_heart_beat(0);
}

// ---- remove_call_out (evalute=0) drops the node without firing ----
TEST_F(ContainerOwnershipTest, RemoveCallout_NoEvaluate)
{
    CContainer c;
    c.call_out(&TestCallback, 1, 5, 6);
    c.remove_call_out(&TestCallback, 0);
    c.heart_beat();  // nothing left to fire
    EXPECT_EQ(g_call_count, 0);
    c.set_heart_beat(0);
}

// ---- remove_call_out (evalute=1) fires immediately then drops ----
TEST_F(ContainerOwnershipTest, RemoveCallout_Evaluate)
{
    CContainer c;
    c.call_out(&TestCallback, 5, 7, 8);
    c.remove_call_out(&TestCallback, 1);
    EXPECT_EQ(g_call_count, 1);
    EXPECT_EQ(g_last_param1, 7);
    EXPECT_EQ(g_last_param2, 8);
    c.set_heart_beat(0);
}

// ---- remove_call_out_by_id matches on param1 ----
TEST_F(ContainerOwnershipTest, RemoveCalloutById)
{
    CContainer c;
    c.call_out(&TestCallback, 1, 42, 0);
    c.remove_call_out_by_id(42);
    c.heart_beat();
    EXPECT_EQ(g_call_count, 0);  // was removed by id
    c.set_heart_beat(0);
}

TEST_F(ContainerOwnershipTest, RemoveCalloutById_NoMatch)
{
    CContainer c;
    c.call_out(&TestCallback, 1, 42, 0);
    c.remove_call_out_by_id(999);  // no match -> stays queued
    c.heart_beat();
    EXPECT_EQ(g_call_count, 1);
    c.set_heart_beat(0);
}

// ---- call_out_combat with delay 0 is a "wait for combat end" sentinel:
//      heart_beat must skip it (nType==1 && !delay), leaving it for evaluate.
//      ----
TEST_F(ContainerOwnershipTest, CombatCallout_SkippedByHeartBeat)
{
    CContainer c;
    c.call_out_combat(&TestCallback, 0, 3, 4);  // delay 0 = wait for combat end
    c.heart_beat();
    EXPECT_EQ(g_call_count, 0);  // skipped by the sentinel branch
    c.set_heart_beat(0);
}

// ---- evaluate_call_out_combat fires pending combat callouts ----
TEST_F(ContainerOwnershipTest, EvaluateCombatCallout_Fires)
{
    CContainer c;
    c.call_out_combat(&TestCallback, 0, 9,
                      10);  // delay 0 = wait for combat end
    c.evaluate_call_out_combat();
    EXPECT_EQ(g_call_count, 1);
    EXPECT_EQ(g_last_param1, 9);
    EXPECT_EQ(g_last_param2, 10);
    c.set_heart_beat(0);
}

// ---- destructor releases pending callouts without crashing ----
TEST_F(ContainerOwnershipTest, DestructorReleasesPendingCallouts)
{
    const int before = g_call_count;  // 文件级计数器无每用例 reset
    CContainer* c    = new CContainer();
    c->call_out(&TestCallback, 100, 1, 2);  // never due
    c->call_out(&TestCallback, 100, 3, 4);
    c->set_heart_beat(0);
    delete c;  // unique_ptr auto-frees the nodes
    // 契约：析构是释放而非触发回调
    EXPECT_EQ(g_call_count, before);
}

// ====================================================================
// CChannel::snoop_list (migrated from raw snoop_t* to unique_ptr)
// Pure in-memory list ops + add_temp/delete_temp on the CChar operands;
// no network/DB, so stack CChar/CChannel are safe.
// ====================================================================
TEST(ChannelSnoopList, SnoopThenQuery)
{
    CChannel ch;
    CChar a, b;
    ch.snoop(&a, &b);
    EXPECT_EQ(ch.query_snooping(&a), &b);
}

TEST(ChannelSnoopList, ReSnoopReplacesEntry)
{
    CChannel ch;
    CChar a, b, c;
    ch.snoop(&a, &b);
    ch.snoop(&a, &c);  // same listener -> old node erased, new pushed
    EXPECT_EQ(ch.query_snooping(&a), &c);
}

TEST(ChannelSnoopList, RemoveSnoopByListener)
{
    CChannel ch;
    CChar a, b;
    ch.snoop(&a, &b);
    ch.remove_snoop(&a);  // matches listener
    EXPECT_EQ(ch.query_snooping(&a), nullptr);
}

TEST(ChannelSnoopList, RemoveSnoopByVictim)
{
    CChannel ch;
    CChar a, b;
    ch.snoop(&a, &b);
    ch.remove_snoop(&b);  // matches victim
    EXPECT_EQ(ch.query_snooping(&a), nullptr);
}

TEST(ChannelSnoopList, ReleaseMeClearsList)
{
    CChannel ch;
    CChar a, b, c;
    ch.snoop(&a, &b);
    ch.snoop(&a, &c);
    ch.release_me();
    EXPECT_EQ(ch.query_snooping(&a), nullptr);
}

TEST(ChannelSnoopList, UntrackedReturnsNull)
{
    CChannel ch;
    CChar x;
    EXPECT_EQ(ch.query_snooping(&x), nullptr);
}
