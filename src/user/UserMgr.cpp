/**
 * @file UserMgr.cpp
 * @brief 用户管理  */

#include "UserMgr.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "User.h"
#include "Channel.h"
#include "Room.h"
#include "../util/Logger.h"

//---------------------------------------------------------
// 用户列表访问
//---------------------------------------------------------
MAP2USER* users() { return &g_user_list; }

//---------------------------------------------------------
// 用户查找
//---------------------------------------------------------
CUser* find_player(int32_t id)
{
    iterator_user p;

    p = g_user_list.find(id);
    if (p == g_user_list.end()) return 0;

    return p->second;
}

//---------------------------------------------------------
// 用户释放
//---------------------------------------------------------
void drop_all(CChar* me);

void user_release()
{
    LOG_TRACE("IN");
    iterator_user p;
    CUser* user;

    for (p = g_user_list.begin(); p != g_user_list.end(); p++) {
        user = p->second;

        CRoom* env = load_room("pk");
        if (env && env->query("reboot")) drop_all(user);
        user->save_to_server();
        user->set_online_flag(0);
        user->set_destructed();
        LOG_INFO("服务器重启下线: %s(id:%s)", user->name(1), user->id(1));
        user->offline();
        user->set_heart_beat(0);
        user->move(NULL);
        user->clean_up();
    }

    for (p = g_user_list.begin(); p != g_user_list.end(); p++) {
        user = p->second;
        delete user;
    }

    g_user_list.clear();
    LOG_TRACE("OUT");
}

//---------------------------------------------------------
// 用户删除
//---------------------------------------------------------
void remove_user(int32_t id)
{
    LOG_TRACE("IN");
    g_user_list.erase(id);
    LOG_TRACE("OUT");
}

//---------------------------------------------------------
// 增加玩家
//---------------------------------------------------------
CUser* add_user()
{
    CUser* t = new CUser();
    if (t) g_user_list[t->object_id()] = t;
    return t;
}

//---------------------------------------------------------
// 用户指令循环
//---------------------------------------------------------
void user_command_loop()
{
    MAP2USER* list = users();
    iterator_user p;
    CUser* obj;

    for (p = list->begin(); p != list->end();) {
        obj = p->second;
        p++;

        obj->handle_command();
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// find_player(id) -> object - 按对象ID查找玩家
int l_find_player(lua_State* L)
{
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    lua_bind_push_object(L, find_player(id));
    return 1;
}

// add_user() -> obj - 创建（或获取）一个用户对象
int l_add_user(lua_State* L)
{
    lua_bind_push_object(L, add_user());
    return 1;
}
