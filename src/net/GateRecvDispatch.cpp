/**
 * @file GateRecvDispatch.cpp
 * @brief 接收数据按登录状态分类入队的实现。 */

#include "GateRecvDispatch.h"

#include <cstring>

#include "GameSvr.h"

RecvAction dispatch_gate_recv(CGateInfo* gate, DWORD bytesReceived,
                              const char* peerIp)
{
    gate->OverlappedEx[0].bufLen += static_cast<int>(bytesReceived);
    int buflen = gate->OverlappedEx[0].bufLen;

    int32_t login;
    {
        std::lock_guard<std::recursive_mutex> lock(gate->m_cs);
        login = gate->m_login;
    }

    char completionPacket[DATA_BUFSIZE];

    // 初次登录：压登录消息，IP 固定写在包体 255 字节处
    if (login == GATE_LOGIN_NOT_LOGGED_IN) {
        std::lock_guard<std::recursive_mutex> lock(gate->m_cs);
        gate->m_login = GATE_LOGIN_LOGGING_IN;

        *(gate->extract_packet(completionPacket, buflen)) = '\0';
        strcpy(completionPacket + 255, peerIp ? peerIp : "");

        char* ptr = strstr(completionPacket, "\n");
        if (ptr) (*ptr) = 0;

        _LPTRECVBUFF lpRecvBuff = new _TRECVBUFF;
        lpRecvBuff->len         = 512;
        lpRecvBuff->type        = RECV_MSG_LOGIN;
        memcpy(lpRecvBuff->szData, completionPacket, 512);
        gate->m_recv_buff_q.push_q(reinterpret_cast<uint8_t*>(lpRecvBuff));
    }

    buflen = gate->OverlappedEx[0].bufLen;

    {
        std::lock_guard<std::recursive_mutex> lock(gate->m_cs);
        login = gate->m_login;
    }

    // 断开/待删除状态：停止接收，由 worker 释放本次引用
    if (login < 0) return RecvAction::release;

    // 正在登录：丢弃后继消息
    if (login == GATE_LOGIN_LOGGING_IN) {
        *(gate->extract_packet(completionPacket, buflen)) = '\0';
    }
    else {
        // 正常命令：按 \t 拆包压入接收队列
        while (int packetlen = gate->has_completion_packet()) {
            gate->extract_packet(completionPacket, packetlen);
            if (completionPacket[packetlen - 1] != '\t') packetlen++;
            completionPacket[packetlen - 1] = 0;
            _LPTRECVBUFF lpRecvBuff         = new _TRECVBUFF;
            lpRecvBuff->len                 = packetlen;
            lpRecvBuff->type                = RECV_MSG_COMMAND;
            memcpy(lpRecvBuff->szData, completionPacket, packetlen);
            gate->m_recv_buff_q.push_q(reinterpret_cast<uint8_t*>(lpRecvBuff));
        }
    }

    return RecvAction::Rearm;
}
