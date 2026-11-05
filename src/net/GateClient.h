/**
 * @file GateClient.h
 * @brief 客户端网关通信接口，提供消息发送与连接管理。
 *
 * 入参 handle 是 CConnectionHandleManager 分配的连接句柄（64 位不透明序号，
 * 0 = 无连接），不是指针——实现经句柄表取 CGateInfo*，未命中即视为连接不存在。
 */
#pragma once

#include <cstdint>

#include "ConnectionHandleManager.h"

void close_client(CConnectionHandleManager::Handle handle);
void open_client(CConnectionHandleManager::Handle handle, int32_t user);
int send_to_client(int object_id, CConnectionHandleManager::Handle handle,
                   const char* msg);
int send_to_client_direct(int sockfd, CConnectionHandleManager::Handle handle,
                          const char* msg);
