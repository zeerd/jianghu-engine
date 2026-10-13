/**
 * @file FB.h
 * @brief 副本总控（CFB）：管理一组副本房间（CFBRoom）的生命周期与进出。
 */

#pragma once

#include "Global.h"
#include "Item.h"

class CFBRoom;

typedef std::list<CFBRoom*> DTFBROOMLIST;

/**
 * @brief 副本总控对象（本身是一件 CItem，停放在 CFBRegistry 房间内）。
 *
 * 负责：按需加载副本房间、玩家进出（move_in）、限时/无人倒计时心跳（heart_beat）、
 * 结束清场（end_fb）。房间本身（CFBRoom）见 FBRoom.h。
 */
class CFB : public CItem {
public:
    void remove_all_room();
    virtual int do_look(CChar* me);
    CFBRoom* load_fb_room(const char* room_name);
    virtual void change_room(CContainer* me);
    virtual void init(CChar* me);
    virtual void create(int flag = 0);
    virtual int32_t query_attr(int attr);
    virtual void reset();

    typedef struct USER_NODE_t {
        string id;  // 用户ID
        int party;  // 哪一方
        USER_NODE_t() : party(0) {}
    } USER_NODE;

    typedef std::list<USER_NODE*> DTUSERLIST;  // 记录NPC列表

    CFB();
    virtual ~CFB();
    virtual void destructed();
    virtual void begin_fb(const char* source_map, int limit_time = 0,
                          int flag = 0);
    virtual void heart_beat();
    void end_fb();
    int move_in(CChar* me, int party = 0);
    virtual void desc(CChar* me) {};
    virtual void reward_party(int party) {};

protected:
    DTFBROOMLIST m_room_list;  // 出口列表。

    int m_room_num;
    int m_tick;
    bool m_nodie;
    DTUSERLIST m_user_list;
};

std::unique_ptr<CFB> create_fb(string fb_name);
