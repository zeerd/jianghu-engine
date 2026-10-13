/** @file LoginReward.cpp @brief 登录后校验 所有游戏逻辑已迁移到
 * lua-sample/data/login.lua， C++ 仅负责加载脚本并调用唯一入口点。 */

#include "Global.h"
#include "User.h"
#include "Char.h"
#include "../lua/LuaEngine.h"
#include "../lua/LuaBind.h"

int check_player(CChar* me, int newenter)
{
    int ref = g_LuaEngine->load_script("login", LUA_SCRIPT_DATA);
    if (ref == LUA_NOREF) {
        LOG_ERROR("CheckPlayer: failed to load login.lua");
        return -1;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_getfield(L, -1, "on_login");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        LOG_ERROR("CheckPlayer: login.on_login not found");
        return -1;
    }

    lua_bind_push_object(L, me);
    lua_pushinteger(L, newenter);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("CheckPlayer: login.on_login failed: %s",
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return -1;
    }

    int result = 0;
    if (lua_isinteger(L, -1)) {
        result = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 2);  // pop return value + script table
    return result;
}
