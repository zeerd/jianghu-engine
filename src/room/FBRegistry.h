/**
 * @file FBRegistry.h
 * @brief 副本注册处，CFB 对象的停放容器。
 */
#include "Room.h"

// 副本注册处 —— CFB 对象的停放容器（内置 C++ 组件，不依赖 Lua 脚本）
//
// load_fb() 创建的 CFB 对象需要一个 parent 容器，
// fb_registry 提供这个容器，并阻止 get/drop/give/use 等操作。

ROOM_BEGIN(CFBRegistry);

void create() { set_name("副本注册处"); }

int handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (strcmp(comm, "get") == 0 || strcmp(comm, "drop") == 0
        || strcmp(comm, "give") == 0 || strcmp(comm, "use") == 0)
        return 1;

    return CRoom::handle_action(comm, me, arg, tar);
}

ROOM_END;
