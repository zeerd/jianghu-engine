// test_gate_info.cpp - Unit tests for GateInfo.cpp
//
// Tests CGateInfo constructor and basic state.
//
#include "pch.h"
#include "GameSvr.h"
#include "GateClient.h"  // close_client

class GateInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GateInfoTest, Constructor_DefaultState)
{
    CGateInfo info;
    EXPECT_EQ(info.m_player, 0);
    EXPECT_EQ(info.m_login, GATE_LOGIN_NOT_LOGGED_IN);
    EXPECT_EQ(info.m_closing, false);
}

TEST_F(GateInfoTest, CloseConnection_SetsClosing)
{
    CGateInfo info;
    info.m_sock = INVALID_SOCKET;
    info.close_connection();
    EXPECT_TRUE(info.m_closing);
}

TEST_F(GateInfoTest, Send_NullMsg)
{
    CGateInfo info;
    info.m_sock = INVALID_SOCKET;
    int result  = info.send(nullptr);
    EXPECT_EQ(result, SOCKET_ERROR);
}

TEST_F(GateInfoTest, Send_InvalidSocket)
{
    CGateInfo info;
    info.m_sock = INVALID_SOCKET;
    int result  = info.send("test");
    EXPECT_EQ(result, SOCKET_ERROR);
}

TEST_F(GateInfoTest, xSend_EmptyQueue)
{
    CGateInfo info;
    info.m_sock = INVALID_SOCKET;
    info.x_send();
}

// ====================================================================
// close_client：主动关闭统一走 close_connection（入队 EXIT + 失效句柄）
// ====================================================================
TEST_F(GateInfoTest, CloseClient_QueuesExitAndInvalidatesSocket)
{
    CGateInfo gate;
    WSADATA wsa;
    ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(s, INVALID_SOCKET);
    gate.m_sock   = s;
    gate.m_login  = GATE_LOGIN_NORMAL;
    gate.m_player = 42;

    close_client((uint32_t)&gate);

    EXPECT_EQ(GATE_LOGIN_CLOSING, gate.m_login);
    EXPECT_EQ(0, gate.m_player);
    EXPECT_TRUE(gate.m_closing);
    EXPECT_EQ(INVALID_SOCKET, gate.m_sock) << "关闭后句柄必须失效";

    // 删除流程由游戏线程消费 EXIT 驱动——必须恰好入队一条
    auto* exitMsg = reinterpret_cast<_LPTRECVBUFF>(gate.m_recv_buff_q.pop_q());
    ASSERT_NE(exitMsg, nullptr) << "主动关闭必须入队 RECV_MSG_EXIT";
    EXPECT_EQ(RECV_MSG_EXIT, exitMsg->type);
    delete exitMsg;
    EXPECT_EQ(nullptr, gate.m_recv_buff_q.pop_q()) << "只入队一条 EXIT";
}

// m_closing 幂等守卫：重复 close 不得重复入队 EXIT
TEST_F(GateInfoTest, CloseClient_IdempotentNoDuplicateExit)
{
    CGateInfo gate;
    gate.m_sock = INVALID_SOCKET;

    close_client((uint32_t)&gate);
    close_client((uint32_t)&gate);

    auto* exitMsg = reinterpret_cast<_LPTRECVBUFF>(gate.m_recv_buff_q.pop_q());
    ASSERT_NE(exitMsg, nullptr);
    EXPECT_EQ(RECV_MSG_EXIT, exitMsg->type);
    delete exitMsg;
    EXPECT_EQ(nullptr, gate.m_recv_buff_q.pop_q());
}

// ====================================================================
// 发送失败支(86-91)：WSASend 同步失败 → 清 sendBuf/归还引用/断开
// ====================================================================
TEST_F(GateInfoTest, SendOnDeadSocketFailsAndCloses)
{
    CGateInfo gate;
    WSADATA wsa;
    ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
    SOCKET dead = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(dead, INVALID_SOCKET);
    gate.m_sock = dead;
    closesocket(dead);  // 已关闭句柄 → WSASend 同步 WSAENOTSOCK ≠ IO_PENDING

    EXPECT_EQ(0, gate.send("hello\n\t")) << "入队后立即派发";

    // 失败支(87-91)：引用归还(2→1)、发送位复位、连接断开
    EXPECT_EQ(INVALID_SOCKET, gate.m_sock) << "发送失败 → close_connection(91)";
    EXPECT_EQ(0, gate.m_do_sending) << "发送位复位(90)";
    EXPECT_EQ(1, gate.m_ref_count) << "失败路径归还 add_ref(90 release)";

    // 断开时按约定投递 RECV_MSG_EXIT（close_connection 头部逻辑）
    auto* exitMsg = reinterpret_cast<_LPTRECVBUFF>(gate.m_recv_buff_q.pop_q());
    ASSERT_NE(exitMsg, nullptr) << "断开消息已入队";
    EXPECT_EQ(RECV_MSG_EXIT, exitMsg->type);
    delete exitMsg;
}

// ====================================================================
// close_connection 清扫未发送缓冲(43-44)
// ====================================================================
TEST_F(GateInfoTest, CloseDrainsQueuedSendItems)
{
    CGateInfo gate;
    WSADATA wsa;
    ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(s, INVALID_SOCKET);
    gate.m_sock       = s;
    gate.m_do_sending = 1;  // 传输中 → send 仅入队不派发(55-56 守卫)
    EXPECT_EQ(0, gate.send("queued item"));
    EXPECT_EQ(1, gate.m_do_sending) << "忙守卫保持发送中 → 条目滞留队列";

    gate.close_connection();  // 清扫队列(43-44) + closesocket

    EXPECT_EQ(INVALID_SOCKET, gate.m_sock) << "连接已断";
    EXPECT_EQ(nullptr, gate.m_send_buff_q.pop_q()) << "发送队列已清空";

    auto* exitMsg = reinterpret_cast<_LPTRECVBUFF>(gate.m_recv_buff_q.pop_q());
    ASSERT_NE(exitMsg, nullptr);
    delete exitMsg;
}
