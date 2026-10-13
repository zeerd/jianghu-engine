/**
 * @file CombatAnnounce.cpp
 * @brief Combat event announcements */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "CombatAnnounce.h"
#include "Char.h"
#include "Channel.h"
#include "Message.h"

CCombatAnnounce g_combat_announce;

void CCombatAnnounce::announce(CChar* me, const char* event)
{
    char msg[255];
    if (strcmp(event, "dead") == 0) {
        snprintf(msg, 255, "%s死了。", me->name());
    }
    else if (strcmp(event, "unconcious") == 0) {
        snprintf(msg, 255, "%s脚下一个不稳，跌在地上一动也不动了。",
                 me->name());
    }
    else if (strcmp(event, "revive") == 0) {
        snprintf(msg, 255, "%s慢慢睁开眼睛，清醒了过来。", me->name());
    }

    message_vision_room(me, "chat", msg);
}
