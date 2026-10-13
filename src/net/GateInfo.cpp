#include "gamesvr.h"
#include "Logger.h"

CGateInfo::CGateInfo()
{
    m_do_sending = 0;
    m_player     = 0;
    m_login      = GATE_LOGIN_NOT_LOGGED_IN;
    m_ref_count  = 1;  // 列表持有 1 个引用
    m_closing    = false;
    memset(&OverlappedEx, 0, sizeof(OverlappedEx));
}

CGateInfo::~CGateInfo() = default;

void CGateInfo::add_ref() { InterlockedIncrement(&m_ref_count); }

void CGateInfo::release()
{
    if (InterlockedDecrement(&m_ref_count) == 0) delete this;
}

void CGateInfo::close_connection()
{
    std::lock_guard<std::recursive_mutex> lock(m_cs);
    if (m_closing) {
        return;
    }
    m_closing = true;

    LOG_INFO("连接关闭: player=%d", m_player);

    // 压断开消息，由游戏线程统一清理
    _LPTRECVBUFF lpRecvBuff = new _TRECVBUFF;
    lpRecvBuff->len         = 0;
    lpRecvBuff->type        = RECV_MSG_EXIT;
    lpRecvBuff->szData[0]   = 0L;
    m_recv_buff_q.push_q(reinterpret_cast<uint8_t*>(lpRecvBuff));

    // 清空发送队列，释放未发送的缓冲
    _LPTSENDITEM lpItem;
    while ((lpItem = (_LPTSENDITEM)m_send_buff_q.pop_q()) != NULL) {
        delete[] lpItem->data;
        delete lpItem;
    }

    closesocket(m_sock);
    m_sock = INVALID_SOCKET;
}

// 从发送队列取数据并发起单个 WSASend（同一时刻仅一个未完成发送，保证顺序）
void CGateInfo::try_send_next()
{
    if (InterlockedCompareExchange(&m_do_sending, 1, 0) != 0)
        return;  // 已有未完成发送

    if (m_sock == INVALID_SOCKET) {
        InterlockedExchange(&m_do_sending, 0);
        return;
    }

    _LPTSENDITEM lpItem = (_LPTSENDITEM)m_send_buff_q.pop_q();
    if (!lpItem) {
        InterlockedExchange(&m_do_sending, 0);
        return;
    }

    LPOVERLAPPEDSEND sendBuf = new OVERLAPPEDSEND;
    memset(&sendBuf->Overlapped, 0, sizeof(OVERLAPPED));
    sendBuf->nOvFlag     = OVERLAPPED_SEND;
    sendBuf->DataBuf.len = lpItem->len;
    sendBuf->DataBuf.buf = lpItem->data;  // 所有权转移给发送缓冲
    sendBuf->bufLen      = lpItem->len;
    sendBuf->gate        = this;
    delete lpItem;

    // 发送完成前持有引用，防止连接被释放
    add_ref();

    DWORD dwBytesSends = 0;
    int ret            = WSASend(m_sock, &sendBuf->DataBuf, 1, &dwBytesSends, 0,
                                 reinterpret_cast<OVERLAPPED*>(sendBuf), NULL);

    if (ret == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        // 同步失败：释放缓冲与引用，恢复发送通道
        LOG_INFO("发送失败断开: player=%d", m_player);
        delete[] sendBuf->DataBuf.buf;
        delete sendBuf;
        release();
        InterlockedExchange(&m_do_sending, 0);
        close_connection();
    }
}

void CGateInfo::x_send() { try_send_next(); }

int CGateInfo::send(const char* msg)
{
    if (!msg || m_sock == INVALID_SOCKET) return SOCKET_ERROR;

    int pos = (int)strlen(msg);

    _LPTSENDITEM lpItem = new _TSENDITEM;
    lpItem->len         = pos;
    lpItem->data        = new char[pos ? pos : 1];
    if (pos) memcpy(lpItem->data, msg, pos);

    m_send_buff_q.push_q(reinterpret_cast<uint8_t*>(lpItem));

    // 尝试立即发送（若通道空闲）
    try_send_next();

    return 0;
}

int CGateInfo::recv()
{
    DWORD nRecvBytes = 0, nFlags = 0;

    OverlappedEx[0].nOvFlag     = OVERLAPPED_RECV;
    OverlappedEx[0].DataBuf.len = DATA_BUFSIZE - OverlappedEx[0].bufLen;
    OverlappedEx[0].DataBuf.buf
        = OverlappedEx[0].Buffer + OverlappedEx[0].bufLen;

    memset(&OverlappedEx[0].Overlapped, 0, sizeof(OverlappedEx[0].Overlapped));

    return WSARecv(m_sock, &OverlappedEx[0].DataBuf, 1, &nRecvBytes, &nFlags,
                   &OverlappedEx[0].Overlapped, 0);
}

int CGateInfo::has_completion_packet()
{
    int analyse_ptr      = 0;
    char* command_string = OverlappedEx[0].Buffer;

    while (analyse_ptr < OverlappedEx[0].bufLen
           && command_string[analyse_ptr]) {
        if (command_string[analyse_ptr] == '\n')  // Command Line End  吃掉
        {
            memmove(OverlappedEx[0].Buffer + analyse_ptr,
                    OverlappedEx[0].Buffer + analyse_ptr + 1,
                    DATA_BUFSIZE - analyse_ptr - 1);
            OverlappedEx[0].bufLen--;
            continue;
        }
        if (command_string[analyse_ptr] == '\t')  // Command Line End
        {
            return analyse_ptr + 1;
        }

        analyse_ptr++;
    }

    if (OverlappedEx[0].bufLen >= 1024) return 1024;
    return 0;
}

char* CGateInfo::extract_packet(char* packet, int len)
{
    int packetLen = len;

    if (packetLen > OverlappedEx[0].bufLen) packetLen = OverlappedEx[0].bufLen;

    memcpy(packet, OverlappedEx[0].Buffer, packetLen);

    memmove(OverlappedEx[0].Buffer, OverlappedEx[0].Buffer + packetLen,
            DATA_BUFSIZE - packetLen);
    OverlappedEx[0].bufLen -= packetLen;

    return packet + packetLen;
}
