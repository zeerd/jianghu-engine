

/**
 * @file CombatAnnounce.h
 * @brief 战斗事件广播（死亡/昏迷/复活通知）。
 */
#pragma once

class CChar;

/**
 * @brief 战斗事件广播器，向房间内玩家发送战斗事件消息。
 */
class CCombatAnnounce {
public:
    // Announce combat events (dead/unconcious/revive) to room viewers
    static void announce(CChar* me, const char* event);
};

extern CCombatAnnounce g_combat_announce;
