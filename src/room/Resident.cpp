// Lua binds for CResident house helpers (declared in Resident.h).

#include "LuaBindIncludes.h"

// get_my_house(who) -> object - 获取玩家的住宅房间（CResident::get_my_house）

int l_get_my_house(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) {
        lua_pushnil(L);

        return 1;
    }

    CChar* ch = dynamic_cast<CChar*>(obj);

    if (!ch) {
        lua_pushnil(L);

        return 1;
    }

    lua_bind_push_object(L, CResident::get_my_house(ch));

    return 1;
}
