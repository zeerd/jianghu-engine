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

void CDoor::bind(const char* next_room, const char* NextDoor)
{
    if (next_room != NULL) {
        set("next_room", next_room);
        if (NextDoor != NULL) set("next_door", NextDoor);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:bind(next_room, next_door) - 重新绑定门的目标房间（CDoor）
int l_door_bind(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CDoor* door = dynamic_cast<CDoor*>(obj);
    if (!door) return 0;
    const char* next_room = luaL_checkstring(L, 2);
    const char* next_door = luaL_optstring(L, 3, "");
    char nextBuf[256], nextDoorBuf[256];
    strncpy(nextBuf, next_room, 255);
    nextBuf[255] = 0;
    strncpy(nextDoorBuf, next_door, 255);
    nextDoorBuf[255] = 0;
    door->bind(nextBuf, nextDoorBuf);
    return 0;
}
