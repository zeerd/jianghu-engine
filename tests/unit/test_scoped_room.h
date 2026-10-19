// test_scoped_room.h — 测试专用：登记房间并在用例结束摘除销毁
//
// 构造即 load_room（g_room_list 按 base_name 缓存，重复调用幂等），
// 析构从 g_room_list 摘除并销毁；覆盖 ASSERT 提前返回的路径，
// 避免房间残留污染后续用例。
// 注意：test_box_door.cpp 把栈上 CRoom 塞进 g_room_list 且只 erase 不
// delete，属另一套语义，不能改用本类（会 delete 栈对象）。
#pragma once

#include <string>

#include "Global.h"
#include "Room.h"
#include "RoomMgr.h"

class ScopedRoom {
public:
    explicit ScopedRoom(const char* base_name) : m_key(base_name)
    {
        m_room = load_room(base_name);
    }
    ~ScopedRoom()
    {
        auto it = g_room_list.find(m_key);
        if (it == g_room_list.end()) return;
        delete it->second;
        g_room_list.erase(it);
    }
    ScopedRoom(const ScopedRoom&)            = delete;
    ScopedRoom& operator=(const ScopedRoom&) = delete;

    CRoom* get() const { return m_room; }

private:
    std::string m_key;
    CRoom* m_room;
};
