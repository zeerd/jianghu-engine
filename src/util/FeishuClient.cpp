/**
 * @file FeishuClient.cpp
 * @brief 飞书 webhook 投递实现：JSON 载荷组装、重试与 WinHTTP 传输。 */

#include "FeishuClient.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <stdio.h>
#include <string>

// JSON 字符串转义：引号、反斜杠与控制字符；UTF-8 多字节序列原样保留
// （飞书按 UTF-8 解析载荷，中文不需要 \u 转义）。
static std::string json_escape(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size() + 8);
    for (unsigned char c : raw) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    // 其余控制字符按 \u00xx 转义（JSON 规范要求）
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

std::string build_feishu_payload(const std::string& text)
{
    return "{\"msg_type\":\"text\",\"content\":{\"text\":\"" + json_escape(text)
           + "\"}}";
}

CFeishuClient::CFeishuClient(std::unique_ptr<IHttpTransport> transport,
                             std::string webhook_url)
    : m_transport(std::move(transport)), m_webhook_url(std::move(webhook_url))
{
}

bool CFeishuClient::send(const std::string& text)
{
    // 未配置 webhook 时客户端不产生任何网络动作（调用方本不该注入它）
    if (m_webhook_url.empty() || !m_transport) return false;

    std::string payload = build_feishu_payload(text);
    if (m_transport->post_json(m_webhook_url, payload)) return true;
    // 单次重试：覆盖一次瞬时失败（DNS 抖动、连接被拒），仍失败则放弃
    // ——推送是尽力而为，失败只记 INFO 日志，不回灌告警链路。
    return m_transport->post_json(m_webhook_url, payload);
}

// ---------------------------------------------------------------------
// WinHTTP 传输
// ---------------------------------------------------------------------

// 宽字符转换：webhook URL 属 ASCII（域名与路径不含非 ASCII 字符）
static std::wstring to_wide(const std::string& s)
{
    return std::wstring(s.begin(), s.end());
}

bool CWinHttpTransport::post_json(const std::string& url,
                                  const std::string& body)
{
    if (url.empty()) return false;

    std::wstring wurl = to_wide(url);

    URL_COMPONENTS uc    = {0};
    uc.dwStructSize      = sizeof(uc);
    uc.dwSchemeLength    = (DWORD)-1;
    uc.dwHostNameLength  = (DWORD)-1;
    uc.dwUrlPathLength   = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &uc)) {
        return false;
    }

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) {
        path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }

    HINTERNET session
        = WinHttpOpen(L"JH-LogMonitor/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    // webhook 调用必须短平快：四段各 10 秒封顶
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 10000);

    bool success   = false;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    if (conn) {
        DWORD flags
            = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(
            conn, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (request) {
            const wchar_t* headers
                = L"Content-Type: application/json; charset=utf-8";
            DWORD body_len = (DWORD)body.size();
            if (WinHttpSendRequest(request, headers, (DWORD)-1,
                                   (LPVOID)body.data(), body_len, body_len, 0)
                && WinHttpReceiveResponse(request, NULL)) {
                DWORD status     = 0;
                DWORD status_len = sizeof(status);
                if (WinHttpQueryHeaders(
                        request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_len,
                        WINHTTP_NO_HEADER_INDEX)) {
                    success = (status >= 200 && status < 300);
                }
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(session);
    return success;
}
