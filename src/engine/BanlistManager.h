/**
 * @file BanlistManager.h
 * @brief 封禁与登录控制子系统 — 管理 IP 封禁、登录计数与登录上限。
 *
 *
 */

#pragma once

#include "Mapping.h"

/**
 * @brief 封禁/登录控制管理器，持有三个 CMapping 数据表。
 *
 * 职责：
 * - banlist    封禁 IP 列表
 * - loginlist  当前登录计数（按 IP）
 * - loginlimit 登录上限（按 IP）
 *
 */
class CBanlistManager {
public:
    // 封禁 IP 列表
    CMapping banlist;

    // IP 地址登录计数
    CMapping loginlist;

    // IP 地址登录上限
    CMapping loginlimit;

    CBanlistManager() = default;
    ~CBanlistManager();

    /** @brief 清空全部数据（create 时调用）。 */
    void clear()
    {
        banlist.clear();
        loginlist.clear();
        loginlimit.clear();
    }
};
