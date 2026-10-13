// Lua binds for CNpcMirror (declared in NpcMirror.h).

#include "LuaBindIncludes.h"

// self:catch_ob(me) -> number  （CNpcMirror 抓捕逻辑）

int l_catch_ob(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CNpcMirror* npc = dynamic_cast<CNpcMirror*>(obj);

    if (!npc) return 0;

    CMainObject* o2 = get_object(L, 2);

    if (!o2) return 0;

    CChar* me = dynamic_cast<CChar*>(o2);

    if (!me) return 0;

    npc->catch_ob(me);

    lua_pushinteger(L, 0);

    return 1;
}

// self:make_mirror(me, diff) - 制造玩家镜象（CNpcMirror 专用）

int l_make_mirror(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CNpcMirror* mirror = dynamic_cast<CNpcMirror*>(obj);

    if (!mirror) return 0;

    CMainObject* meObj = get_object(L, 2);

    CChar* me = meObj ? dynamic_cast<CChar*>(meObj) : NULL;

    if (!me) return 0;

    int diff = (int)luaL_checkinteger(L, 3);

    mirror->make_mirror(me, diff);

    return 0;
}
