/**
 * @file RoomInterface.cpp
 * @brief CRoom 实现的接口方法（IActionHandler /
 * IDialogSource / IClientObject）。 */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "Room.h"
#include "User.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Sql.h"

// ========================================================================
//  IActionHandler
// ========================================================================

int CRoom::do_action(int act, CChar* me) { return 0; }

int CRoom::handle_action(char* comm, CChar* me, char* arg) { return 0; }

// ========================================================================
//  IDialogSource
// ========================================================================

void CRoom::send_menu(CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&e=1&", QUESTION, object_id());
    me->send_command(msg, SEND_WAIT_BUFFERED);
}

void CRoom::add_menu_item(string show, string cmd, CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&s=%s&c=%s&", QUESTION, object_id(),
             show.c_str(), cmd.c_str());
    me->send_command(msg, SEND_WAIT_BUFFERED);
}

int CRoom::say(const char* arg, CChar* me, const char* tips)
{
    char chat[402];
    char msg[512];

    if (!me) {
        strncpy(chat, arg, 400)[400] = 0;
        snprintf(msg, 512, "%s说：%s", name(), chat);
        message_vision_room(this, "chat", msg);
    }
    else {
        me->send_command(snprintf(msg, 512, "&C=%d&N=%d&n=%s&m=%s&", QUESTION,
                                  object_id(), tips ? tips : name(), arg),
                         SEND_WAIT_BUFFERED);
    }

    return 1;
}

// ========================================================================
//  IClientObject
// ========================================================================

void CRoom::download(CChar* me)
{
    char str[512];

    snprintf(str, 512, "&C=%d&O=%d&", DOWNOBJECT, object_id());
    get_info(str, me);

    me->send_command(str, SEND_WAIT_BUFFERED);
}

void CRoom::get_info(char* str, CChar* me)
{
    char tmp[512], msg[255];

    snprintf(tmp, 255, "N=%s&I=%s&P=%d&a=%d&U=%d&W=%d&", name(), id(),
             query("icon"), query_amount(), query_attr(CChar::UPDATE),
             weight());

    if (EQUALSTR(querystr("status"), "locked")) {
        strcat(tmp, snprintf(msg, 40, "l=%s&", querystr("reason")));
    }

    strcat(tmp, snprintf(msg, 40, "t=%s&", title()));

    strcat(str, tmp);
}

void CRoom::send_command(const char* msg, int wait) {}
