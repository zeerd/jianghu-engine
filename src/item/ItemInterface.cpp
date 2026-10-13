/** @file ItemInterface.cpp @brief CItem 实现的接口方法（IHittable / IEquippable
 * / IActionHandler / IDialogSource / IClientObject）。 */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "Item.h"
#include "User.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Room.h"
#include "Sql.h"

// ========================================================================
//  IHittable
// ========================================================================

const char* CItem::hit_ob(CChar* me, CChar* victim, int32_t damage_bonus)
{
    return "";
}

void CItem::owner_is_killed(CChar* killer) {}

// ========================================================================
//  IEquippable
// ========================================================================

int CItem::wield() { return do_action(CChar::DO_WIELD); }

int CItem::wear() { return do_action(CChar::DO_WIELD); }

// ========================================================================
//  IActionHandler
// ========================================================================

int CItem::handle_action(char* comm, CChar* me, char* arg) { return 0; }

int CItem::handle_action(char* comm, CChar* me, char* arg, char* target)
{
    return 0;
}

// ========================================================================
//  IDialogSource
// ========================================================================

void CItem::send_menu(CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&e=1&", QUESTION, object_id());
    me->send_command(msg, SEND_WAIT_BUFFERED);
}

void CItem::add_menu_item(string show, string cmd, CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&s=%s&c=%s&", QUESTION, object_id(),
             show.c_str(), cmd.c_str());
    me->send_command(msg, SEND_WAIT_BUFFERED);
}

int CItem::say(const char* arg, CChar* me, const char* tips)
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

void CItem::download(CChar* me)
{
    char str[512];

    snprintf(str, 512, "&C=%d&O=%d&", DOWNOBJECT, object_id());
    get_info(str, me);

    me->send_command(str, SEND_WAIT_BUFFERED);
}

void CItem::get_info(char* str, CChar* me)
{
    char tmp[512], msg[255];

    snprintf(tmp, 255, "N=%s&I=%s&P=%d&a=%d&U=%d&W=%d&", name(), id(),
             query("icon"), query_amount(), query_attr(CChar::UPDATE),
             weight());

    if (query_attr(CAN_GET)) strcat(tmp, "g=1&");
    if (query_attr(IS_BOX)) strcat(tmp, "x=1&");

    if (EQUALSTR(querystr("status"), "locked")) {
        strcat(tmp, snprintf(msg, 40, "l=%s&", querystr("reason")));
    }

    if (query_attr(CAN_USE)) strcat(tmp, "f=1&");

    if (query_attr(CAN_EQUIPPED)) {
        snprintf(msg, 40, "w=%d&", query("wield_position"));
        strcat(tmp, msg);
    }

    strcat(tmp, snprintf(msg, 40, "t=%s&", title()));

    strcat(str, tmp);
}

void CItem::send_command(const char* msg, int wait) {}
