/**
 * @file FeishuClient.h
 * @brief 飞书自定义机器人 webhook 推送：文本载荷组装与 HTTP 传输抽象。
 *
 * 分层：日志监控只依赖 IFeishuSender；载荷组装与重试在 CFeishuClient；
 * 网络收发下沉到 IHttpTransport——传输手段（WinHTTP 或其他）属部署细节，
 * 单独替换不影响投递决策；投递失败与否由 HTTP 层给出，监控侧只看结果。
 */

#pragma once

#include <memory>
#include <string>

// HTTP POST 传输：把 JSON 载荷投递到 webhook URL。
// 返回 true 表示服务端以 2xx 应答。
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual bool post_json(const std::string& url, const std::string& body) = 0;
};

// 飞书发送接口：日志监控计划产出的唯一出口。
class IFeishuSender {
public:
    virtual ~IFeishuSender() = default;
    // 发送一条文本消息；返回是否送达（传输层成功）。
    virtual bool send(const std::string& text) = 0;
};

// 自定义机器人的 text 消息载荷：{"msg_type":"text","content":{"text":"..."}}
std::string build_feishu_payload(const std::string& text);

// webhook 投递实现：载荷组装 + 传输失败时重试一次（瞬时网络抖动不丢消息）。
class CFeishuClient : public IFeishuSender {
public:
    CFeishuClient(std::unique_ptr<IHttpTransport> transport,
                  std::string webhook_url);

    bool send(const std::string& text) override;

private:
    std::unique_ptr<IHttpTransport> m_transport;
    std::string m_webhook_url;
};

// WinHTTP POST 实现：URL 拆解 → HTTPS 请求 → 2xx 判定。
// 非 2xx、超时与连接失败一律返回 false。
class CWinHttpTransport : public IHttpTransport {
public:
    bool post_json(const std::string& url, const std::string& body) override;
};
