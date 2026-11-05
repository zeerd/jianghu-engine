/**
 * @file GateClient.cpp
 * @brief 客户端网关通信  */

#include "GateClient.h"
#include "../object/Global.h"
#include "GameSvr.h"
#include "../util/Logger.h"

#include <WinSock2.h>

void close_client(CConnectionHandleManager::Handle handle)
{
    CGateInfo* gate_info = CConnectionHandleManager::lookup(handle);
    if (gate_info) {
        std::lock_guard<std::recursive_mutex> lock(gate_info->m_cs);
        LOG_DEBUG("%d", gate_info->m_player);
        gate_info->m_login  = GATE_LOGIN_CLOSING;
        gate_info->m_player = 0;
        // 统一走关闭路径：入队 RECV_MSG_EXIT（游戏线程据此调度删除）、
        // 关闭并失效 socket，并在其中释放连接句柄。
        gate_info->close_connection();
    }
}

void open_client(CConnectionHandleManager::Handle handle, int32_t user)
{
    CGateInfo* gate_info = CConnectionHandleManager::lookup(handle);
    if (gate_info) {
        std::lock_guard<std::recursive_mutex> lock(gate_info->m_cs);
        gate_info->m_player = user;
        gate_info->m_login  = GATE_LOGIN_NORMAL;
        LOG_DEBUG("%d", gate_info->m_player);
    }
}

int send_to_client(int object_id, CConnectionHandleManager::Handle handle,
                   const char* msg)
{
    CGateInfo* gate_info = CConnectionHandleManager::lookup(handle);
    if (gate_info) {
        gate_info->send(msg);
    }
    return 1;
}

int send_to_client_direct(int sockfd, CConnectionHandleManager::Handle handle,
                          const char* msg)
{
    CGateInfo* gate_info = CConnectionHandleManager::lookup(handle);
    if (gate_info) {
        gate_info->send(msg);
    }
    return 1;
}
