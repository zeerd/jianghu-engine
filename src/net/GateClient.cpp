/**
 * @file GateClient.cpp
 * @brief 客户端网关通信  */

#include "GateClient.h"
#include "../object/Global.h"
#include "GameSvr.h"
#include "../util/Logger.h"

#include <WinSock2.h>

void close_client(uint32_t serial)
{
    CGateInfo* gate_info = (CGateInfo*)serial;
    if (gate_info) {
        std::lock_guard<std::recursive_mutex> lock(gate_info->m_cs);
        LOG_DEBUG("%d", gate_info->m_player);
        gate_info->m_login  = GATE_LOGIN_CLOSING;
        gate_info->m_player = 0;
        gate_info->x_send();
        closesocket(gate_info->m_sock);
    }
}

void open_client(uint32_t serial, int32_t user)
{
    CGateInfo* gate_info = (CGateInfo*)serial;
    if (gate_info) {
        std::lock_guard<std::recursive_mutex> lock(gate_info->m_cs);
        gate_info->m_player = user;
        gate_info->m_login  = GATE_LOGIN_NORMAL;
        LOG_DEBUG("%d", gate_info->m_player);
    }
}

int send_to_client(int object_id, uint32_t serial, const char* msg)
{
    CGateInfo* gate_info = (CGateInfo*)serial;
    if (gate_info) {
        gate_info->send(msg);
    }
    return 1;
}

int send_to_client_direct(int sockfd, uint32_t serial, const char* msg)
{
    CGateInfo* gate_info = (CGateInfo*)serial;
    if (gate_info) {
        gate_info->send(msg);
    }
    return 1;
}
