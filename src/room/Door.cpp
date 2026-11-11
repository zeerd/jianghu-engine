/** @file Door.cpp @brief implementation of the CDoor class. */

#include <windows.h>

#include "Door.h"
#include "Global.h"
#include "Room.h"

CDoor::CDoor() {}
CDoor::~CDoor() {}

int CDoor::enter_door(CChar* me)
{
    CRoom* r = load_room(querystr("next_room"));
    if (!r) return 0;

    // 在me身上做一个下一房间门的标识
    me->set_temp("init_door", querystr("next_door"));
    if (!me->move(r)) {
        me->delete_temp("init_door");
        return 0;
    }
    me->delete_temp("init_door");

    return 1;
}
