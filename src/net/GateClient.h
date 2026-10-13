/**
 * @file GateClient.h
 * @brief 客户端网关通信接口，提供消息发送与连接管理。
 */
#pragma once

#include <cstdint>

void close_client(uint32_t serial);
void open_client(uint32_t serial, int32_t user);
int send_to_client(int object_id, uint32_t serial, const char* msg);
int send_to_client_direct(int sockfd, uint32_t serial, const char* msg);
