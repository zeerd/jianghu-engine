/**
 * @file NetCompletion.h
 * @brief IOCP 完成包的分类判定（与 socket 调用无关的纯逻辑）。
 */
#pragma once

#include "ServerSockHandler.h"

// worker 线程根据一次完成包的要素决定下一步动作
enum class WorkerAction {
    Exit,      // 关停信号：worker 退出
    Skip,      // 无效或未知完成包：跳过
    Accept,    // accept 完成
    SendDone,  // 发送完成
    RecvDone,  // 接收完成
};

/**
 * @brief 分类一次 GetQueuedCompletionStatus 的返回要素。
 * @param queued       GQCS 是否成功
 * @param hasOverlapped lpOverlapped 是否非空
 * @param bytes        传输字节数
 * @param key          completion key（accept 为 IOCP_ACCEPT_KEY，数据为
 * CGateInfo*）
 * @param ovFlag       OVERLAPPEDEX::nOvFlag，仅当 hasOverlapped 时有意义
 */
WorkerAction classify_completion(bool queued, bool hasOverlapped, DWORD bytes,
                                 ULONG_PTR key, int ovFlag);
