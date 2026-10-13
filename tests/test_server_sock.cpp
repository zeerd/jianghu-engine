// test_server_sock.cpp — 网络层集成测试（真实回环 socket + IOCP worker）
//
// 用本机回环连接驱动 accept/recv/send 完成包，覆盖 ServerSockHandler 的
// socket 生命周期、ServerWorkerThread 的完成包分发与 ProcessUserMessage
// 的消息处理；线程与 socket 全部在用例内回收。
#include "pch.h"

#include <cstring>
#include <functional>
#include <list>
#include <mutex>
#include <string>

#include "Container.h"
#include "GameSvr.h"
#include "HeartBeat.h"
#include "ServerSockHandler.h"
#include "test_lua_env.h"

extern SOCKADDR_IN g_saddr;
extern std::list<CGateInfo*> g_gate_list;
extern std::mutex g_gate_list_mutex;
void process_user_message();

namespace {

size_t GateCount()
{
    std::lock_guard<std::mutex> lock(g_gate_list_mutex);
    return g_gate_list.size();
}

CGateInfo* FirstGate()
{
    std::lock_guard<std::mutex> lock(g_gate_list_mutex);
    return g_gate_list.empty() ? nullptr : g_gate_list.front();
}

// 等待条件成立，最多 timeoutMs 毫秒
bool WaitFor(const std::function<bool()>& pred, int timeoutMs)
{
    for (int waited = 0; waited < timeoutMs; waited += 50) {
        if (pred()) return true;
        Sleep(50);
    }
    return pred();
}

void PushRecv(CGateInfo* gate, int type, const char* data)
{
    _LPTRECVBUFF msg = new _TRECVBUFF;
    msg->type        = type;
    msg->len         = data ? static_cast<int>(strlen(data)) + 1 : 0;
    msg->szData[0]   = 0;
    if (data) strcpy(msg->szData, data);
    gate->m_recv_buff_q.push_q(reinterpret_cast<uint8_t*>(msg));
}

class ServerSockTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 延迟销毁会清空容器，Clean_Up 需要房间工厂（Create_Room 解析 Lua
        // 脚本）
        ensure_lua_env();

        WSADATA wsa;
        ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);

        // 上一个用例不得残留连接或监听 socket
        ASSERT_EQ(GateCount(), 0u) << "上一个用例残留了网关连接";
        ASSERT_EQ(g_ssock, INVALID_SOCKET) << "上一个用例残留了监听 socket";
    }

    void TearDown() override
    {
        // 无论断言是否失败，都回收 worker 线程、监听 socket 与客户端 socket
        if (m_serverStarted) {
            g_terminated = true;
            signal_iocp_threads_exit();
            wait_for_all_worker_threads(5000);
            g_terminated = false;
            clear_socket(g_ssock);
            m_serverStarted = false;
        }
        if (m_client != INVALID_SOCKET) {
            closesocket(m_client);
            m_client = INVALID_SOCKET;
        }

        std::list<CGateInfo*> leftovers;
        {
            std::lock_guard<std::mutex> lock(g_gate_list_mutex);
            leftovers.splice(leftovers.end(), g_gate_list);
        }
        for (CGateInfo* gate : leftovers) {
            gate->close_connection();
            gate->release();
        }

        WSACleanup();
    }

    bool m_serverStarted = false;
    SOCKET m_client      = INVALID_SOCKET;
};

}  // namespace

// --------------------------------------------------------- ProcessUserMessage

TEST_F(ServerSockTest, DrainsDestructList)
{
    CContainer* victim = new CContainer();
    victim->set("name", "victim");
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.push_back(victim);
    }

    process_user_message();

    std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
    EXPECT_TRUE(g_destruct_list.empty());
}

TEST_F(ServerSockTest, DispatchesCommandThenRemovesGateOnExit)
{
    CGateInfo* gate = new CGateInfo();
    gate->m_login   = GATE_LOGIN_NORMAL;
    PushRecv(gate, RECV_MSG_COMMAND, "look");
    PushRecv(gate, RECV_MSG_EXIT, nullptr);
    {
        std::lock_guard<std::mutex> lock(g_gate_list_mutex);
        g_gate_list.push_back(gate);
    }

    process_user_message();

    // EXIT 让 gate 出列表并释放最后一个引用（析构发生在这里）
    std::lock_guard<std::mutex> lock(g_gate_list_mutex);
    EXPECT_TRUE(g_gate_list.empty());
}

// ---------------------------------------------------------------- loopback

TEST_F(ServerSockTest, LoopbackClientIsAcceptedAndDrained)
{
    ASSERT_TRUE(init_server_socket(g_ssock, &g_saddr, 0, 0, 1))
        << "InitServerSocket failed, wsa=" << WSAGetLastError();
    m_serverStarted = true;

    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    ASSERT_EQ(
        getsockname(g_ssock, reinterpret_cast<sockaddr*>(&bound), &boundLen),
        0);

    m_client = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(m_client, INVALID_SOCKET);

    sockaddr_in target     = bound;
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(
        connect(m_client, reinterpret_cast<sockaddr*>(&target), sizeof(target)),
        0)
        << "connect failed, wsa=" << WSAGetLastError();

    // worker 完成 accept，并为新连接预投首次接收
    ASSERT_TRUE(WaitFor([] { return GateCount() > 0; }, 5000))
        << "accept 未完成";

    // 客户端发来登录前数据 → worker 压登录消息并转入登录中状态
    const char payload[] = "hello\n";
    ASSERT_EQ(send(m_client, payload, static_cast<int>(sizeof(payload) - 1), 0),
              static_cast<int>(sizeof(payload) - 1));
    ASSERT_TRUE(WaitFor(
        [] {
            CGateInfo* gate = FirstGate();
            if (!gate) return false;
            std::lock_guard<std::recursive_mutex> lock(gate->m_cs);
            return gate->m_login == GATE_LOGIN_LOGGING_IN;
        },
        5000))
        << "接收分支未执行, gates=" << GateCount();

    // 服务器回发 → 触发 worker 的发送完成分支
    {
        CGateInfo* gate = FirstGate();
        ASSERT_NE(gate, nullptr);
        ASSERT_EQ(gate->send("welcome"), 0);
    }

    DWORD recvTimeoutMs = 5000;
    setsockopt(m_client, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&recvTimeoutMs),
               sizeof(recvTimeoutMs));
    char recvBuf[64] = {};
    const int got    = recv(m_client, recvBuf, sizeof(recvBuf) - 1, 0);
    ASSERT_GT(got, 0);
    recvBuf[got] = '\0';
    EXPECT_STREQ(recvBuf, "welcome");

    // 游戏线程：登录消息不是 login 指令 → 拒绝并关闭连接，
    // worker 随后压入 EXIT，下一轮把 gate 移出列表
    for (int i = 0; i < 50 && GateCount() > 0; i++) {
        process_user_message();
        Sleep(50);
    }
    EXPECT_EQ(GateCount(), 0u);
}

// ---------------- init_server_socket：s 已非 INVALID → else 拒绝(143)
// ----------------
TEST_F(ServerSockTest, InitServerSocketRejectsExistingSocket)
{
    SOCKET s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(s2, INVALID_SOCKET);
    EXPECT_FALSE(init_server_socket(s2, &g_saddr, 0, 0, 1))
        << "socket 已存在 → else 支直接 false(143)";
    closesocket(s2);
}

// ---------------- IOCP
// 生命周期：零线程创建/重复拒绝/空列表关闭(165,209,321-327) ----------------
TEST_F(ServerSockTest, IocpInvalidHandleLifecycle)
{
    // thread=0 → 创建 g_iocp 但零 worker（不 spawn 线程）→ 返回 0
    EXPECT_EQ(0, create_iocp_worker_thread(0)) << "零并发 → 0 线程";
    ASSERT_TRUE(g_iocp != nullptr) << "IOCP 已创建";

    // g_iocp 已存在 → 拒绝重复创建，落到函数收尾(208→209)
    EXPECT_EQ(-1, create_iocp_worker_thread(1)) << "重复创建拒绝(209)";

    // 非空列表但条目全为无效句柄 → 过滤后 validHandles 空 →
    // 走"清列表 + 关闭 IOCP"支(321-327)
    g_worker_threads.push_back(INVALID_HANDLE_VALUE);
    wait_for_all_worker_threads(0);
    EXPECT_TRUE(g_iocp == nullptr) << "IOCP 已关闭并复位(327)";
    EXPECT_TRUE(g_worker_threads.empty()) << "worker 列表已清空(322)";

    // 空列表 → 早退路径(290)，g_iocp 保持空不崩
    wait_for_all_worker_threads(0);

    // INVALID socket → clear_socket 返回 false(165)
    SOCKET s = INVALID_SOCKET;
    EXPECT_FALSE(clear_socket(s)) << "无效 socket 清理返回 false(165)";
}
