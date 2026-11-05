// test_connection_handle.cpp -
// 连接句柄表（src/net/ConnectionHandleManager.cpp） 的契约
//
// 句柄把「连接标识」与「对象地址」分开：单调递增、永不复用，释放即永久失效
// （结构上无 ABA）；0 是「无连接」保留值。夹具负责配对归还本用例分配的句柄，
// 避免悬垂条目污染后续用例的查表。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <atomic>
#include <set>
#include <thread>
#include <vector>

#include "ConnectionHandleManager.h"
#include "GameSvr.h"  // CGateInfo：构造/析构不触碰 socket，可作查表载荷

class ConnectionHandleTest : public ::testing::Test {
protected:
    // 配对归还：本用例 allocate 过的句柄一律在收尾释放（release 幂等，
    // 用例内已释放的再释放一次是空操作）
    CConnectionHandleManager::Handle Track(
        CConnectionHandleManager::Handle handle)
    {
        m_tracked.push_back(handle);
        return handle;
    }

    void TearDown() override
    {
        for (auto handle : m_tracked) {
            CConnectionHandleManager::release(handle);
        }
        m_tracked.clear();
    }

    std::vector<CConnectionHandleManager::Handle> m_tracked;
};

TEST_F(ConnectionHandleTest, AllocateRegistersObjectForLookup)
{
    CGateInfo gate;
    auto handle = Track(CConnectionHandleManager::allocate(&gate));

    EXPECT_NE(handle, CConnectionHandleManager::INVALID_CONNECTION_HANDLE);
    EXPECT_EQ(&gate, CConnectionHandleManager::lookup(handle))
        << "allocate 后必须能按句柄取回对象";
}

TEST_F(ConnectionHandleTest, ZeroHandleIsTheNoConnectionSentinel)
{
    EXPECT_EQ(nullptr, CConnectionHandleManager::lookup(
                           CConnectionHandleManager::INVALID_CONNECTION_HANDLE))
        << "0 是「无连接」保留值，永远查不到";
    CConnectionHandleManager::release(
        CConnectionHandleManager::INVALID_CONNECTION_HANDLE);  // 空操作
    EXPECT_EQ(nullptr, CConnectionHandleManager::lookup(0));
}

TEST_F(ConnectionHandleTest, NullGateAllocatesNoHandle)
{
    EXPECT_EQ(CConnectionHandleManager::INVALID_CONNECTION_HANDLE,
              CConnectionHandleManager::allocate(nullptr))
        << "无对象即无句柄";
}

TEST_F(ConnectionHandleTest, ReleasedHandleNeverResolvesAgain)
{
    CGateInfo gate;
    auto handle = Track(CConnectionHandleManager::allocate(&gate));

    CConnectionHandleManager::release(handle);
    EXPECT_EQ(nullptr, CConnectionHandleManager::lookup(handle))
        << "释放即永久失效";

    CConnectionHandleManager::release(handle);  // 幂等：重复释放无副作用
    EXPECT_EQ(nullptr, CConnectionHandleManager::lookup(handle));
}

TEST_F(ConnectionHandleTest, HandlesAreNeverReused)
{
    CGateInfo first;
    CGateInfo second;
    auto old_handle = Track(CConnectionHandleManager::allocate(&first));
    CConnectionHandleManager::release(old_handle);
    auto new_handle = Track(CConnectionHandleManager::allocate(&second));

    EXPECT_NE(new_handle, old_handle)
        << "句柄不复用：旧句柄不会被新连接顶替（无 ABA）";
    EXPECT_EQ(nullptr, CConnectionHandleManager::lookup(old_handle));
    EXPECT_EQ(&second, CConnectionHandleManager::lookup(new_handle));
}

// 并发 allocate/lookup/release：表锁保证自洽，全局序号保证不撞号。
// 4 线程 × 25000 轮 = 10 万次分配/释放，收尾断言表内无残留条目。
TEST_F(ConnectionHandleTest, ConcurrentAllocateLookupReleaseHasNoLeak)
{
    constexpr int kThreads = 4;
    constexpr int kRounds  = 25000;

    std::atomic<bool> failed{false};
    std::vector<std::vector<CConnectionHandleManager::Handle>> collected(
        kThreads);
    std::vector<std::thread> workers;

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&, t] {
            CGateInfo gate;
            collected[t].reserve(kRounds);
            for (int i = 0; i < kRounds; ++i) {
                auto handle = CConnectionHandleManager::allocate(&gate);
                if (handle
                        == CConnectionHandleManager::INVALID_CONNECTION_HANDLE
                    || CConnectionHandleManager::lookup(handle) != &gate) {
                    failed.store(true);
                }
                CConnectionHandleManager::release(handle);
                if (CConnectionHandleManager::lookup(handle) != nullptr) {
                    failed.store(true);
                }
                collected[t].push_back(handle);
            }
        });
    }
    for (auto& worker : workers) worker.join();

    EXPECT_FALSE(failed.load()) << "并发下 allocate→lookup→release 必须自洽";

    std::set<CConnectionHandleManager::Handle> unique;
    for (const auto& handles : collected) {
        unique.insert(handles.begin(), handles.end());
    }
    EXPECT_EQ(unique.size(),
              static_cast<size_t>(kThreads) * static_cast<size_t>(kRounds))
        << "句柄全局唯一：并发分配不撞号";

    size_t leaked = 0;
    for (const auto& handles : collected) {
        for (auto handle : handles) {
            if (CConnectionHandleManager::lookup(handle) != nullptr) ++leaked;
        }
    }
    EXPECT_EQ(0u, leaked) << "10 万次分配/释放后句柄表无残留条目";
}
