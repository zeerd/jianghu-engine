/**
 * @file Door.h
 * @brief 出口对象，连接两个房间并支持锁定/解锁。
 */

#pragma once

#include <list>

#include "CObject.h"

class CChar;

/**
 * @brief 出口对象，连接两个房间并支持锁定/解锁。
 */
class CDoor : public CMainObject {
public:
    virtual void bind(const char* next_room, const char* NextDoor);
    virtual int enter_door(CChar* me);

    // 锁死
    void locked() { set("status", "locked"); }

    // 开锁
    void un_locked() { set("status", "open"); }

    CDoor();
    ~CDoor();
};

// 出口列表
typedef std::list<CDoor*> DTEXITLIST;
