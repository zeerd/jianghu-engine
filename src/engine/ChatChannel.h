/**
 * @file ChatChannel.h
 * @brief 聊天频道子系统 — 管道配置、消息广播、频率限制与禁词过滤。
 *
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Global.h"
#include "Mapping.h"

class CChar;
class CContainer;
class CUser;
class CEmoteSystem;

/**
 * @brief 聊天频道系统，负责频道配置与消息广播。
 *
 * 职责：
 * - do_channel()         核心广播（解析频道/表情/权限/格式化/发送）
 * - check_channel_msg()  频率限制与禁词过滤
 * - create()             初始化频道配置表
 *
 * filter_list / filter_listener / group_listener 为内部辅助，
 * 仅由 do_channel 调用，因此保持 private。
 */
class CChatChannel {
public:
    CChatChannel() = default;
    ~CChatChannel();

    // 禁止拷贝（持有 CMapping 与 vector）
    CChatChannel(const CChatChannel&)            = delete;
    CChatChannel& operator=(const CChatChannel&) = delete;

    /**
     * @brief 设置 EmoteSystem 引用（do_channel 内部调用 do_emote）。
     * @param emote  不得为 nullptr，生命周期须覆盖本对象
     */
    void set_emote_system(CEmoteSystem* emote) { m_emote = emote; }

    /**
     * @brief 核心广播方法。
     * @param ob   消息发布者（角色或频道精灵）
     * @param who  表情目标（可为 NULL）
     * @param arg1 频道动词（带 * 后缀表示表情）
     * @param arg2 消息内容
     * @return 非零表示已处理
     */
    int do_channel(CContainer* ob, CChar* who, const char* arg1,
                   const char* arg2);

    /**
     * @brief 频率限制与禁词过滤。
     * @param me         发言者
     * @param verb       频道名
     * @param chat       消息内容
     * @param block_key  封禁 key（NULL 时不封禁）
     * @return 1 通过；0 被拒绝
     */
    int check_channel_msg(CChar* me, const char* verb, const char* msg,
                          const char* block_key);

    /** @brief 初始化频道配置表并加载禁词。 */
    void create();

    /** @brief 清空频道配置与禁词。 */
    void clear()
    {
        channels.clear();
        m_chanbanword.clear();
    }

private:
    /** @brief 过滤接收该频道消息的用户列表（内部）。
     *  分组频道判定经 hooks.channel_group 下放脚本；
     *  返回 0 表示发言者未入组（已 notify_fail）。 */
    int filter_list(CMapping* ch, CChar* me, const char* verb, MAP2USER& list);

    /** @brief 频道权限检查（内部）。 */
    int filter_listener(CUser* ppl, CMapping* ch);

    /** @brief 分组成员判定：同 key 同值（巫师恒通过）（内部）。 */
    int group_listener(CUser* ppl, const char* value, const char* key);

    /** @brief 从 Lua 配置加载聊天禁词。 */
    void load_ban_words_from_lua();

    // 频道配置表
    CMapping channels;

    // 聊天禁词列表（从 Lua 配置加载）
    std::vector<std::string> m_chanbanword;

    // 表情系统引用（用于 do_channel 内的表情渲染）
    CEmoteSystem* m_emote = nullptr;
};
