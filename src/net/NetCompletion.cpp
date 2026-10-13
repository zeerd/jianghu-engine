/** @file NetCompletion.cpp @brief IOCP 完成包的分类判定实现。 */

#include "NetCompletion.h"

WorkerAction classify_completion(bool queued, bool hasOverlapped, DWORD bytes,
                                 ULONG_PTR key, int ovFlag)
{
    // 关停唤醒：GQCS 失败且无重叠结构、0 字节、0 key
    if (!queued && !hasOverlapped) {
        if (bytes == 0 && key == 0) return WorkerAction::Exit;
        return WorkerAction::Skip;
    }

    // accept 完成以 completion key 区分
    if (key == IOCP_ACCEPT_KEY) return WorkerAction::Accept;

    // 数据完成的 key 是 CGateInfo*，无效即跳过
    if (!hasOverlapped || key == 0) return WorkerAction::Skip;

    if (ovFlag == OVERLAPPED_SEND) return WorkerAction::SendDone;
    if (ovFlag == OVERLAPPED_RECV) return WorkerAction::RecvDone;

    return WorkerAction::Skip;
}
