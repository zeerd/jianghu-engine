// cli/tests/connection_test.cc — Connection 连接/收发契约
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=Win32
//   bin\cli\jianghu_cli_tests.exe --gtest_filter=Connection*

#include "connection.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "compat.h"
#include "protocol.h"

namespace {

// 轮询等待谓词成立（网络测试专用：有上界的等待，不做无限阻塞）。
template <typename Pred>
bool wait_until(Pred pred, int timeout_ms = 3000)
{
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

// 本机回环一次性 TCP 服务器：accept 一个连接，收发后关闭。
class LoopbackServer {
public:
    ~LoopbackServer() { stop(); }

    bool start()
    {
        if (!net_init()) return false;
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listener_ == kInvalidSocket) return false;

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;  // 临时端口
        if (::bind(listener_, reinterpret_cast<sockaddr*>(&addr),
                   sizeof(addr))
            != 0)
            return false;
        if (::listen(listener_, 1) != 0) return false;

        socket_len_t len = sizeof(addr);
        if (::getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &len)
            != 0)
            return false;
        port_ = ntohs(addr.sin_port);
        return true;
    }

    int port() const { return port_; }

    // 接受一个连接并收完首条请求（以 '\n' 结尾）
    bool accept_and_recv_request(std::string& out)
    {
        client_ = ::accept(listener_, nullptr, nullptr);
        if (client_ == kInvalidSocket) return false;
        char buf[512];
        int n = ::recv(client_, buf, static_cast<int>(sizeof(buf)), 0);
        if (n <= 0) return false;
        out.assign(buf, n);
        return true;
    }

    bool send_to_client(const std::string& data)
    {
        int len = static_cast<int>(data.size());
        return ::send(client_, data.data(), len, 0) == len;
    }

    // 关闭客户端连接（让客户端观察到 EOF）
    void close_client()
    {
        if (client_ != kInvalidSocket) {
            close_socket(client_);
            client_ = kInvalidSocket;
        }
    }

    void stop()
    {
        close_client();
        if (listener_ != kInvalidSocket) {
            close_socket(listener_);
            listener_ = kInvalidSocket;
        }
    }

private:
    socket_t listener_ = kInvalidSocket;
    socket_t client_   = kInvalidSocket;
    int port_          = 0;
};

}  // namespace

// ==================== 未连接状态 ====================

TEST(ConnectionTest, FreshConnectionIsNotConnected)
{
    Connection conn;
    EXPECT_FALSE(conn.is_connected());
    EXPECT_FALSE(conn.is_eof());
    EXPECT_TRUE(conn.drain_received().empty());
}

TEST(ConnectionTest, SendsFailWhenNotConnected)
{
    Connection conn;
    EXPECT_FALSE(conn.send("x"));
    EXPECT_FALSE(conn.send_login("acc", "pwd"));
    EXPECT_FALSE(conn.send_register("acc", "pwd", "M", 21, 21, 21, 21));
    EXPECT_FALSE(conn.send_command_utf8("look\t"));
}

TEST(ConnectionTest, DisconnectIsSafeWhenNeverConnected)
{
    Connection conn;
    conn.disconnect();
    conn.disconnect();  // 幂等
    EXPECT_FALSE(conn.is_connected());
}

// ==================== 回环真实收发 ====================

TEST(ConnectionTest, LoopbackSendsReceivesAndSeesEof)
{
    LoopbackServer server;
    ASSERT_TRUE(server.start());

    std::string request;
    std::thread server_thr([&] {
        if (!server.accept_and_recv_request(request)) return;
        // 下行：两条独立编码的消息一次写出（测试 \t 分帧）
        server.send_to_client(protocol::encode_str("msg1\t")
                              + protocol::encode_str("msg2\t"));
        // 下行：跨包半条消息（测试 incomplete_message_ 拼接）
        server.send_to_client(protocol::encode_str("partial"));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server.send_to_client("\t");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server.close_client();  // 客户端应观察到 EOF
    });

    Connection conn;
    ASSERT_TRUE(conn.connect("127.0.0.1", server.port()));
    EXPECT_TRUE(conn.is_connected());

    // 上行：UTF-8 → GBK → XOR 编码后送达
    EXPECT_TRUE(conn.send_command_utf8("look\t"));

    // 下行：分帧 + 半包拼接（drain 返回的是 XOR 编码的原始字节）
    std::vector<std::string> got;
    ASSERT_TRUE(wait_until([&] {
        auto batch = conn.drain_received();
        got.insert(got.end(), batch.begin(), batch.end());
        return got.size() >= 3;
    }));
    EXPECT_EQ(got[0], protocol::encode_str("msg1\t"));
    EXPECT_EQ(got[1], protocol::encode_str("msg2\t"));
    EXPECT_EQ(got[2], protocol::encode_str("partial\t"));

    // 服务端关闭 → is_eof
    EXPECT_TRUE(wait_until([&] { return conn.is_eof(); }));

    server_thr.join();

    // 上行内容可还原为原命令（send 追加的 '\n' 去掉后再解码）
    ASSERT_FALSE(request.empty());
    EXPECT_EQ(request.back(), '\n');
    std::string body = request.substr(0, request.size() - 1);
    EXPECT_EQ(protocol::decode_str(body), "look\t");
}
