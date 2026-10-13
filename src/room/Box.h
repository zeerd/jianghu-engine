/**
 * @file Box.h
 * @brief 箱子物件，可上锁/开锁，内容物可存盘。
 */

#pragma once

#include <cstdint>

#include "Item.h"

/**
 * @brief 箱子物件，可上锁/开锁，内容物可存盘。
 */
class CBox : public CItem {
public:
    void remove_objects();
    int load_object();
    int save_object();

    virtual int handle_action(char* comm, CChar* me, char* arg);
    int save_local();
    virtual void download(CChar* me);
    virtual void on_leave(CContainer* me);
    virtual void on_enter(CContainer* me);
    void locked(char* season, int32_t duration, CChar* me = NULL);
    int un_locked(int32_t resume = 0)
    {
        int dur = 0;

        if (!resume)
            del("duration");
        else
            dur = add("duration", -resume);

        if (dur <= 0) {
            del("owner");
            del("duration");
            set("status", "open");
            update_me();
            return 0;
        }

        return dur;
    };

    // 设定钥匙
    void set_key(int32_t passwd) { set("password", passwd); }

    // 往箱子里放东西
    CContainer* add_object(const char* obj_name, int32_t amount = 1);

    virtual int32_t query_attr(int attr)
    {
        switch (attr) {
            case CAN_GET:
                return 0;
            case IS_BOX:
                return 1;
        }
        return CItem::query_attr(attr);
    };

    CBox()
    {
        set_name("箱子");
        set("status", "open");
        set("infinity", 1);  // 不受容量限制
        set("no_clean_up", 1);
        set("max_items", 10);

        set("no_fight", 1);  // 我晕,竟然有人在箱子里战斗
        set("no_cast", 1);
    };

    ~CBox();
};
