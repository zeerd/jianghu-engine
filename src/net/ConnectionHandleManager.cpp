/**
 * @file ConnectionHandleManager.cpp
 * @brief 连接句柄表实现。
 */

#include "ConnectionHandleManager.h"

std::mutex CConnectionHandleManager::s_mutex;
std::unordered_map<CConnectionHandleManager::Handle, CGateInfo*>
    CConnectionHandleManager::s_table;
uint64_t CConnectionHandleManager::s_sequence = 0;

CConnectionHandleManager::Handle CConnectionHandleManager::allocate(
    CGateInfo* gate)
{
    if (!gate) return INVALID_CONNECTION_HANDLE;

    std::lock_guard<std::mutex> lock(s_mutex);
    Handle handle = ++s_sequence;
    // 回绕跳过 0：0 是「无连接」的保留值（回绕本身在进程内不可达）
    if (handle == INVALID_CONNECTION_HANDLE) handle = ++s_sequence;
    s_table.emplace(handle, gate);
    return handle;
}

CGateInfo* CConnectionHandleManager::lookup(Handle handle)
{
    if (handle == INVALID_CONNECTION_HANDLE) return nullptr;

    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_table.find(handle);
    return it == s_table.end() ? nullptr : it->second;
}

void CConnectionHandleManager::release(Handle handle)
{
    if (handle == INVALID_CONNECTION_HANDLE) return;

    std::lock_guard<std::mutex> lock(s_mutex);
    s_table.erase(handle);
}
