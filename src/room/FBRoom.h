/**
 * @file FBRoom.h
 * @brief 副本房间（CFBRoom）与副本出口（CFBDoor）。副本总控见 FB.h。
 */

#pragma once

#include "Room.h"

class CFB;

/**
 * @brief 副本出口对象。
 *
 * 与普通 CDoor 的唯一差异：enter_door 不在全局房间表里找目标，而是经
 * m_parent_fb->load_fb_room 动态创建/复用副本房间。bind 复用 CDoor::bind。
 * 由 CFBRoom::new_door 工厂产出，存放于 CRoom::m_door_list（不再单列）。
 */
class CFBDoor : public CDoor {
public:
    virtual int enter_door(CChar* me);

    CFBDoor(CFB* fb) { m_parent_fb = fb; };
    ~CFBDoor() {};
    CFB* m_parent_fb;
};

/**
 * @brief 标准副本房间。
 *
 * 与 CRoom 的绝大部分行为已合并（门列表、清物 reset、change_room、valid_leave、
 * do_look、add_item、present_door、query_exits 等直接继承 CRoom）。此处只保留
 * 副本真正独有的差异：
 *   - new_door 工厂产出 CFBDoor（门经父副本按需加载房间）
 *   - reset 只清物、不复位 NPC（构造置 no_reset_npc，防 boss 无限复活）
 *   - change_room 保留 fb_nodie（CRoom 按 IS_FBROOM 分流）
 *   - valid_leave 受 no_leave 约束（CRoom 通用检查）
 *   - setup 区域名取 base_name 前 8 字符、不设 city
 *   - handle_action 一律拒绝（禁 "back" 越狱）
 *   - add_npc 的 NPC 不可拾起
 */
class CFBRoom : public CRoom {
public:
    CFBRoom();
    CFBRoom(CFB* fb);
    virtual ~CFBRoom();

    virtual void destructed();
    virtual int handle_action(char* comm, CChar* me, char* arg, char* tar);
    virtual void setup();
    virtual int32_t query_attr(int attr);

    // 增加NPC：副本房间内的 NPC 不允许拾起
    CNpc* add_npc(const char* base_name, int32_t bx = 0, int32_t by = 0,
                  int32_t ex = 0, int32_t ey = 0, int dir = 0, int active = 0,
                  int ival = 0) override;

    CFB* m_parent_fb;

protected:
    // 门工厂：副本房产出 CFBDoor（携带父副本指针，enter_door 按需加载房间）
    virtual CDoor* new_door();
};
