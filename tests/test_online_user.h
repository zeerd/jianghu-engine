// test_online_user.h — 测试共用：在线用户登记（find_online 走 g_user_list），
// 析构即摘除。原内联于 test_house.cpp 的匿名 namespace，因 CResident
// （Resident.h）测试同样需要，按"重复 setup 合并"原则抽出共享。
#pragma once

#include "User.h"

class ScopedOnlineUser {
public:
    ScopedOnlineUser(const char* id, const char* name)
    {
        user = new CUser();
        user->set("id", id);
        user->set("name", name);
        g_user_list[user->object_id()] = user;
    }
    ~ScopedOnlineUser()
    {
        g_user_list.erase(user->object_id());
        delete user;
    }
    ScopedOnlineUser(const ScopedOnlineUser&)            = delete;
    ScopedOnlineUser& operator=(const ScopedOnlineUser&) = delete;
    CUser* get() const { return user; }

private:
    CUser* user;
};
