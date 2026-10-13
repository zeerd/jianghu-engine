/**
 * @file GateRecvDispatch.h
 * @brief 按登录状态把接收完成的数据分类入队（与 socket 投递无关）。
 */
#pragma once

#include "ServerSockHandler.h"

class CGateInfo;

// 一次接收数据处理完后，worker 应当采取的动作
enum class RecvAction {
    Rearm,    // 继续投递接收
    release,  // 连接已进入删除流程：释放本次 recv 引用，不再 re-arm
};

/**
 * @brief 处理一次接收完成的数据。
 *
 * 登录前的数据压成登录消息（附带对端 IP），登录中的数据丢弃，
 * 正常状态的数据按 \t 拆成命令入队；待删除状态返回 RecvAction::Release。
 *
 * @param gate         网关连接
 * @param bytesReceived 本次完成的字节数
 * @param peerIp       对端 IP，可为空
 */
RecvAction dispatch_gate_recv(CGateInfo* gate, DWORD bytesReceived,
                              const char* peerIp);
