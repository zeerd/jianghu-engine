// test_feishu_client.cpp — 飞书 webhook 投递：载荷组装、重试、真实回环传输
//
// 构建/运行：仓库根执行 .\run-unit-tests.ps1（唯一认可方式）
#include "pch.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "FeishuClient.h"

namespace {

// 记录每次 POST 的 URL 与载荷；前 fail_first 次返回失败
class FakeTransport : public IHttpTransport {
public:
    bool post_json(const std::string& url, const std::string& body) override
    {
        urls.push_back(url);
        bodies.push_back(body);
        const bool ok = call_count >= fail_first;
        ++call_count;
        return ok;
    }

    std::vector<std::string> urls;
    std::vector<std::string> bodies;
    int call_count = 0;
    int fail_first = 0;
};

}  // namespace

// ---------------------------------------------------------------------
// 载荷组装
// ---------------------------------------------------------------------

TEST(FeishuPayload, EscapesControlAndQuoteCharacters)
{
    const std::string payload
        = build_feishu_payload("he said \"hi\"\nline2\\end\ttab");
    EXPECT_EQ(payload,
              "{\"msg_type\":\"text\",\"content\":{\"text\":"
              "\"he said \\\"hi\\\"\\nline2\\\\end\\ttab\"}}");
}

TEST(FeishuPayload, KeepsUtf8TextRaw)
{
    // 中文按 UTF-8 原样入载荷（飞书按 UTF-8 解析，不做 \u 转义）
    const std::string payload = build_feishu_payload("缓冲越界");
    EXPECT_NE(payload.find("缓冲越界"), std::string::npos);
}

TEST(FeishuPayload, EscapesLowControlCharacters)
{
    const std::string payload
        = build_feishu_payload(std::string("a\x01") + "b");
    EXPECT_NE(payload.find("\\u0001"), std::string::npos);
}

// ---------------------------------------------------------------------
// 投递与重试
// ---------------------------------------------------------------------

TEST(FeishuClient, SendsOnceOnSuccess)
{
    auto transport = std::make_unique<FakeTransport>();
    auto* probe    = transport.get();
    CFeishuClient client(std::move(transport), "https://hook.example/x");

    EXPECT_TRUE(client.send("hello"));
    EXPECT_EQ(probe->call_count, 1);
    EXPECT_EQ(probe->urls[0], "https://hook.example/x");
    EXPECT_NE(probe->bodies[0].find("\"msg_type\":\"text\""),
              std::string::npos);
}

TEST(FeishuClient, RetriesOnceAfterFailure)
{
    auto transport    = std::make_unique<FakeTransport>();
    auto* probe       = transport.get();
    probe->fail_first = 1;  // 首次失败
    CFeishuClient client(std::move(transport), "https://hook.example/x");

    EXPECT_TRUE(client.send("hello"));
    EXPECT_EQ(probe->call_count, 2);                // 首试 + 一次重试
    EXPECT_EQ(probe->bodies[0], probe->bodies[1]);  // 重试投递同一载荷
}

TEST(FeishuClient, FailsWhenRetryExhausted)
{
    auto transport    = std::make_unique<FakeTransport>();
    auto* probe       = transport.get();
    probe->fail_first = 100;  // 全部失败
    CFeishuClient client(std::move(transport), "https://hook.example/x");

    EXPECT_FALSE(client.send("hello"));
    EXPECT_EQ(probe->call_count, 2);  // 重试一次即止，不无限重投
}

TEST(FeishuClient, EmptyWebhookMakesNoTransportCall)
{
    auto transport = std::make_unique<FakeTransport>();
    auto* probe    = transport.get();
    CFeishuClient client(std::move(transport), "");

    EXPECT_FALSE(client.send("hello"));
    EXPECT_EQ(probe->call_count, 0);
}

TEST(FeishuClient, NullTransportYieldsFalse)
{
    // 装配缺传输（防御分支）：有地址也发不出去
    CFeishuClient client(nullptr, "https://hook.example/x");
    EXPECT_FALSE(client.send("hello"));
}

TEST(FeishuPayload, EscapesBackspaceAndFormFeed)
{
    const std::string payload
        = build_feishu_payload(std::string("a\b") + std::string("c\fd"));
    EXPECT_NE(payload.find("\\b"), std::string::npos);
    EXPECT_NE(payload.find("\\f"), std::string::npos);
}

// ---------------------------------------------------------------------
// 真实 WinHTTP 传输（回环服务器：线程与 socket 在用例内回收）
// ---------------------------------------------------------------------

namespace {

// 单次应答的回环 HTTP 服务器：收一个请求、按状态码回包、关闭
class LoopbackHttpServer {
public:
    bool start(unsigned short status_code)
    {
        m_status   = status_code;
        m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listener == INVALID_SOCKET) return false;

        sockaddr_in addr     = {0};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;  // 临时端口，避免与他人争抢
        if (bind(m_listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
            != 0) {
            return false;
        }
        if (listen(m_listener, 1) != 0) return false;

        sockaddr_in bound = {0};
        int len           = sizeof(bound);
        if (getsockname(m_listener, reinterpret_cast<sockaddr*>(&bound), &len)
            != 0) {
            return false;
        }
        m_port   = ntohs(bound.sin_port);
        m_thread = std::thread([this] { serve_once(); });
        return true;
    }

    void stop()
    {
        if (m_listener != INVALID_SOCKET) {
            // 关监听套接字令 accept 返回，服务线程随之收口
            closesocket(m_listener);
            m_listener = INVALID_SOCKET;
        }
        if (m_thread.joinable()) m_thread.join();
    }

    ~LoopbackHttpServer() { stop(); }

    unsigned short port() const { return m_port; }
    std::string request() const { return m_request; }

private:
    void serve_once()
    {
        SOCKET client = accept(m_listener, NULL, NULL);
        if (client == INVALID_SOCKET) return;

        // 读到头部结束，再按 Content-Length 收齐载荷即应答——
        // 不等客户端断开（WinHTTP 走 keep-alive，等断开会挂住 join）
        char buf[8192];
        std::string req;
        size_t body_begin  = std::string::npos;
        size_t content_len = 0;
        while (true) {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            req.append(buf, n);

            if (body_begin == std::string::npos) {
                const size_t head_end = req.find("\r\n\r\n");
                if (head_end == std::string::npos) continue;
                body_begin = head_end + 4;

                const size_t cl = req.find("Content-Length: ");
                if (cl != std::string::npos) {
                    content_len = static_cast<size_t>(
                        strtoul(req.c_str() + cl + 16, NULL, 10));
                }
            }
            if (body_begin != std::string::npos
                && req.size() >= body_begin + content_len) {
                break;
            }
        }

        const std::string status_line
            = m_status == 200 ? "200 OK"
                              : (m_status == 500 ? "500 Internal Server Error"
                                                 : "404 Not Found");
        const std::string resp = "HTTP/1.1 " + status_line
                                 + "\r\nContent-Length: 2\r\n"
                                   "Connection: close\r\n\r\n{}";
        send(client, resp.data(), static_cast<int>(resp.size()), 0);
        closesocket(client);
        m_request = req;
    }

    SOCKET m_listener       = INVALID_SOCKET;
    unsigned short m_port   = 0;
    unsigned short m_status = 0;
    std::thread m_thread;
    std::string m_request;  // join 后主线程读取
};

// 已绑定但不监听的回环端口：连接必然被拒绝；套接字持有到用例结束，
// 端口不外借、无「被他人接管」的竞态
class RefusedPort {
public:
    RefusedPort()
    {
        m_socket             = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr     = {0};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;
        if (m_socket == INVALID_SOCKET
            || bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
                   != 0) {
            return;
        }
        sockaddr_in bound = {0};
        int len           = sizeof(bound);
        if (getsockname(m_socket, reinterpret_cast<sockaddr*>(&bound), &len)
            == 0) {
            m_port = ntohs(bound.sin_port);
        }
    }

    ~RefusedPort()
    {
        if (m_socket != INVALID_SOCKET) closesocket(m_socket);
    }

    unsigned short port() const { return m_port; }

private:
    SOCKET m_socket       = INVALID_SOCKET;
    unsigned short m_port = 0;
};

class FeishuTransportTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        WSADATA wsa;
        ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
    }

    void TearDown() override { WSACleanup(); }
};

}  // namespace

TEST_F(FeishuTransportTest, PostsPayloadOn2xx)
{
    LoopbackHttpServer server;
    ASSERT_TRUE(server.start(200));

    CWinHttpTransport transport;
    const std::string url
        = "http://127.0.0.1:" + std::to_string(server.port()) + "/hook?x=1";
    const std::string body = build_feishu_payload("缓冲越界");

    EXPECT_TRUE(transport.post_json(url, body));
    server.stop();

    const std::string request = server.request();
    EXPECT_EQ(request.find("POST /hook?x=1 HTTP/1.1"), 0u);
    EXPECT_NE(request.find("Content-Type: application/json"),
              std::string::npos);
    EXPECT_NE(request.find(body), std::string::npos);
}

TEST_F(FeishuTransportTest, ReturnsFalseOnNon2xx)
{
    LoopbackHttpServer server;
    ASSERT_TRUE(server.start(500));

    CWinHttpTransport transport;
    const std::string url
        = "http://127.0.0.1:" + std::to_string(server.port()) + "/hook";
    EXPECT_FALSE(transport.post_json(url, "{}"));
    server.stop();
}

TEST_F(FeishuTransportTest, ReturnsFalseWhenConnectionRefused)
{
    const RefusedPort refused;
    ASSERT_NE(refused.port(), 0);

    CWinHttpTransport transport;
    EXPECT_FALSE(transport.post_json(
        "http://127.0.0.1:" + std::to_string(refused.port()) + "/hook", "{}"));
}

TEST_F(FeishuTransportTest, ReturnsFalseOnHttpsToClosedPort)
{
    // HTTPS 分支：安全标志 + TLS 前的连接即被拒绝
    const RefusedPort refused;
    ASSERT_NE(refused.port(), 0);

    CWinHttpTransport transport;
    EXPECT_FALSE(transport.post_json(
        "https://127.0.0.1:" + std::to_string(refused.port()) + "/hook", "{}"));
}

TEST_F(FeishuTransportTest, ReturnsFalseOnMalformedUrl)
{
    CWinHttpTransport transport;
    EXPECT_FALSE(transport.post_json("://missing-scheme", "{}"));
}

TEST_F(FeishuTransportTest, ReturnsFalseOnEmptyUrl)
{
    CWinHttpTransport transport;
    EXPECT_FALSE(transport.post_json("", "{}"));
}
