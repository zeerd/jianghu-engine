/**
 * @file ConnectionHandleManager.h
 * @brief 连接句柄表：把「连接标识」与「对象地址」分开，对外只流转不透明句柄。
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

class CGateInfo;

/**
 * @brief 连接句柄表（handle ↔ CGateInfo*）。
 *
 * 为什么存在：游戏层曾经把 CGateInfo* 截断成 32 位整数当连接标识流动，
 * 64 位平台上指针被压断即失效，对象销毁后旧值仍会被当指针解引用。句柄把
 * 标识与地址分开：指针只存在于本表内，跨线程只流转 64 位不透明序号，查表
 * 未命中一律视为「连接不存在」，不存在野指针解引用。
 *
 * 生命周期契约（查表结果为何可信）：
 * - 分配发生在 accept 完成路径（IOCP worker 线程），存入 CGateInfo::m_handle；
 * - 释放在 CGateInfo::close_connection 进入关闭流程的那一刻——早于对象被
 *   游戏线程移出 g_gate_list，因此「查表命中 ⇒ 对象尚未进入关闭流程」；
 * - 句柄单调递增、永不复用：旧句柄释放后永久失效，不存在复用撞号（ABA），
 *   回绕需 2^64 次 accept，进程生命周期内不可达。
 *
 * 与 IOCP 完成路径的分工：完成键与 OVERLAPPEDSEND 直接携带 CGateInfo*，
 * 不查本表——连接关闭后仍会到达的完成包若查表必然落空，未完成 IO 持有的
 * 引用与发送缓冲就无人归还；那条路径靠「每个未完成 IO 各持一份引用计数」
 * 保证对象存活（见 CGateInfo::add_ref/release），完成键只需不被截断
 * （ULONG_PTR），不承担连接标识职责。
 *
 * 线程安全：allocate/lookup/release 全程持表锁，临界区只有一次哈希表操作；
 * 对象自身的引用计数独立于本表，两者锁不互涉。
 */
class CConnectionHandleManager {
public:
    using Handle = uint64_t;

    // 0 保留为「无连接」：未登录/已断线一律持有 0，查表必然落空
    static constexpr Handle INVALID_CONNECTION_HANDLE = 0;

    // 分配句柄并登记对象；gate 为空时返回 INVALID_CONNECTION_HANDLE
    static Handle allocate(CGateInfo* gate);
    // 查表；未命中（含句柄为 0）返回 nullptr
    static CGateInfo* lookup(Handle handle);
    // 释放句柄；幂等，重复释放与释放 0 均为空操作
    static void release(Handle handle);

private:
    static std::mutex s_mutex;
    static std::unordered_map<Handle, CGateInfo*> s_table;
    static uint64_t s_sequence;  // 表锁内递增，句柄永不复用
};
