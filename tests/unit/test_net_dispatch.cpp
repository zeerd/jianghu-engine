// test_net_dispatch.cpp — 网络层纯逻辑单元测试
//
// ClassifyCompletion（IOCP 完成包分类）与 DispatchGateRecv（按登录状态入队）
// 都不触碰 socket，可直接驱动；真实 accept/recv/send 由 test_server_sock 覆盖。
#include "pch.h"

#include <cstring>
#include <string>

#include "GameSvr.h"
#include "GateRecvDispatch.h"
#include "NetCompletion.h"

namespace {

ULONG_PTR FakeGateKey()
{
    static int dummy = 0;
    return reinterpret_cast<ULONG_PTR>(&dummy);
}

// 预置一次接收的数据：内容写入 Buffer，bufLen 归零由 Dispatch 累加
void FeedRecv(CGateInfo& gate, const std::string& data)
{
    ASSERT_LT(data.size(), static_cast<size_t>(DATA_BUFSIZE));
    memcpy(gate.OverlappedEx[0].Buffer, data.data(), data.size());
    gate.OverlappedEx[0].bufLen = 0;
}

_LPTRECVBUFF PopRecv(CGateInfo& gate)
{
    return reinterpret_cast<_LPTRECVBUFF>(gate.m_recv_buff_q.pop_q());
}

}  // namespace

// ---------------------------------------------------------- ClassifyCompletion

TEST(NetCompletionTest, ClassifyCompletion_RoutesCompletionKinds)
{
    struct CompletionCase {
        const char* name;
        bool queued;
        bool hasOverlapped;
        DWORD bytes;
        ULONG_PTR key;
        int ovFlag;
        WorkerAction expected;
    };

    const ULONG_PTR gateKey      = FakeGateKey();
    const CompletionCase cases[] = {
        {"关停信号", false, false, 0, 0, 0, WorkerAction::Exit},
        {"GQCS失败且无重叠结构", false, false, 64, 0x1234, 0,
         WorkerAction::Skip},
        {"accept 完成", true, true, 0, IOCP_ACCEPT_KEY, 0,
         WorkerAction::Accept},
        {"accept 但无重叠结构", true, false, 0, IOCP_ACCEPT_KEY, 0,
         WorkerAction::Accept},
        {"数据 key 为空", true, true, 8, 0, OVERLAPPED_RECV,
         WorkerAction::Skip},
        {"重叠结构为空", true, false, 8, gateKey, OVERLAPPED_RECV,
         WorkerAction::Skip},
        {"发送完成", true, true, 8, gateKey, OVERLAPPED_SEND,
         WorkerAction::SendDone},
        {"接收完成", true, true, 8, gateKey, OVERLAPPED_RECV,
         WorkerAction::RecvDone},
        {"未知标志", true, true, 8, gateKey, 99, WorkerAction::Skip},
    };

    for (const auto& c : cases) {
        EXPECT_EQ(classify_completion(c.queued, c.hasOverlapped, c.bytes, c.key,
                                      c.ovFlag),
                  c.expected)
            << c.name;
    }
}

// -------------------------------------------------------------
// DispatchGateRecv

TEST(GateRecvDispatchTest, FirstDataBecomesLoginMessage)
{
    CGateInfo gate;
    FeedRecv(gate, "hello\n");

    EXPECT_EQ(dispatch_gate_recv(&gate, 6, "127.0.0.1"), RecvAction::Rearm);
    EXPECT_EQ(gate.m_login, GATE_LOGIN_LOGGING_IN);
    EXPECT_EQ(gate.OverlappedEx[0].bufLen, 0);

    _LPTRECVBUFF msg = PopRecv(gate);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->type, RECV_MSG_LOGIN);
    EXPECT_EQ(msg->len, 512);
    EXPECT_STREQ(msg->szData, "hello");
    EXPECT_STREQ(msg->szData + 255, "127.0.0.1");
    delete msg;
}

TEST(GateRecvDispatchTest, DataWhileLoggingInIsDiscarded)
{
    CGateInfo gate;
    gate.m_login = GATE_LOGIN_LOGGING_IN;
    FeedRecv(gate, "junk");

    EXPECT_EQ(dispatch_gate_recv(&gate, 4, ""), RecvAction::Rearm);
    EXPECT_EQ(gate.m_recv_buff_q.size(), 0u);
    EXPECT_EQ(gate.OverlappedEx[0].bufLen, 0);
}

TEST(GateRecvDispatchTest, NormalStateSplitsCommandsByTab)
{
    CGateInfo gate;
    gate.m_login = GATE_LOGIN_NORMAL;
    FeedRecv(gate, "look\tsay hi\t");

    EXPECT_EQ(dispatch_gate_recv(&gate, 12, ""), RecvAction::Rearm);

    _LPTRECVBUFF first = PopRecv(gate);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->type, RECV_MSG_COMMAND);
    EXPECT_STREQ(first->szData, "look");
    delete first;

    _LPTRECVBUFF second = PopRecv(gate);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->type, RECV_MSG_COMMAND);
    EXPECT_STREQ(second->szData, "say hi");
    delete second;

    EXPECT_EQ(gate.m_recv_buff_q.size(), 0u);
}

TEST(GateRecvDispatchTest, PendingDeleteStopsReceiving)
{
    CGateInfo gate;
    gate.m_login = GATE_LOGIN_PENDING_DELETE;
    FeedRecv(gate, "late");

    EXPECT_EQ(dispatch_gate_recv(&gate, 4, ""), RecvAction::release);
    EXPECT_EQ(gate.m_recv_buff_q.size(), 0u);
}

// ---------------------------------------------------------- 换行剥离(140-145)
TEST(NetDispatchTest, HasCompletionPacketStripsNewlines)
{
    CGateInfo gate;
    // '\n' 先于 '\t' → memmove 剥离换行 + bufLen-- + 继续扫描(140-145)
    std::string data = "C1\nMhello\t";
    memcpy(gate.OverlappedEx[0].Buffer, data.data(), data.size());
    gate.OverlappedEx[0].bufLen = static_cast<int>(data.size());

    int n = gate.has_completion_packet();
    EXPECT_GT(n, 0) << "遇到 \\t 返回包长";
    EXPECT_EQ(gate.OverlappedEx[0].bufLen, static_cast<int>(data.size()) - 1)
        << "换行被剥离，bufLen 收缩";
    gate.OverlappedEx[0].bufLen = 0;
}
