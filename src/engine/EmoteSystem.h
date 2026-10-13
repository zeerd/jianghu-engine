/**
 * @file EmoteSystem.h
 * @brief 表情子系统 — 管理表情定义、文本处理与客户端下载。
 *
 *
 */

#pragma once

#include <cstdint>

#include "Mapping.h"

class CChar;
class CUser;

/**
 * @brief 表情系统，负责 emote 的加载、渲染与下发。
 *
 * 职责：
 * - do_emote()       渲染表情文本（$N/$P/$R 等占位符替换）
 * - DownLoadEmote()  将表情动词列表下发到客户端
 * - LoadEmotesFromLua() 从 Lua 配置加载表情表
 */
class CEmoteSystem {
public:
    CEmoteSystem() = default;
    ~CEmoteSystem();

    // 禁止拷贝（持有 CMapping）
    CEmoteSystem(const CEmoteSystem&)            = delete;
    CEmoteSystem& operator=(const CEmoteSystem&) = delete;

    /**
     * @brief 渲染表情文本。
     * @param me            发出表情的角色
     * @param target        目标角色（NULL 为无目标）
     * @param verb          表情动词
     * @param channel_emote 非零时为频道表情模式（返回渲染结果而非直接输出）
     * @param who           覆盖默认发送者名字
     * @return 渲染后的文本；失败返回 NULL 或空串
     */
    char* do_emote(CChar* me, CChar* target, const char* verb,
                   int channel_emote, const char* who = nullptr);

    /** @brief 将 emote 动词下载到客户端。 */
    void download_emote(CUser* me);

    /** @brief 从 Lua 配置加载 emote 表情表。 */
    void load_emotes_from_lua();

    /** @brief 清空表情表（create 时调用）。 */
    void clear() { semote.clear(); }

private:
    // verb -> field -> 文案
    CMapping semote;
};
