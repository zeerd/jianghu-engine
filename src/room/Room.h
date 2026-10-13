/**
 * @file Room.h
 * @brief 房间基类，管理出口/箱子/查看/移动。
 */

#pragma once

#include "Box.h"
#include "Door.h"
#include "Global.h"
#include "IActionHandler.h"
#include "IDialogSource.h"
#include "IClientObject.h"

#define ROOM_BEGIN(class_name)        \
    class class_name : public CRoom { \
    public:                           \
        class_name() {};              \
        virtual ~class_name() {};
#define ROOM_END }

// 标准房间

class CRoom : public CContainer,
              public IActionHandler,
              public IDialogSource,
              public IClientObject {
public:
    // === IActionHandler ===
    int do_action(int act, CChar* me = NULL) override;
    int handle_action(char* comm, CChar* me, char* arg) override;
    int handle_action(char* comm, CChar* me, char* arg, char* tar) override;

    // === IDialogSource ===
    void send_menu(CChar* me) override;
    void add_menu_item(string show, string cmd, CChar* me) override;
    int say(const char* msg, CChar* me = NULL, const char* tips = 0) override;

    // === IClientObject ===
    void download(CChar* me) override;
    void get_info(char* str, CChar* me) override;
    int do_look(CChar* me) override;
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override;

    // === CRoom-specific ===
    void remove_all_box();
    virtual void change_room(CContainer* me);
    virtual int valid_leave(CChar* me, const char* dir);
    virtual void setup();
    virtual void remove_all_doors();
    virtual void remove_doors(const char* arg);
    virtual CContainer* add_item(const char* item_name);
    virtual DTEXITLIST* query_exits();

    typedef struct OBJECT_NODE_t {
        string base_name;  // 基本名
        int32_t object;    // 实例指针
        int is_npc;        // 是否npc
        int interval;
        int base_interval;
        int32_t bx, by, ex, ey;
        int dir, active;
        OBJECT_NODE_t()
            : object(0),
              is_npc(0),
              interval(0),
              base_interval(0),
              bx(0),
              by(0),
              ex(0),
              ey(0),
              dir(0),
              active(0)
        {
        }
    } OBJECT_NODE;

    typedef std::list<std::unique_ptr<OBJECT_NODE>>
        DTOBJECTLIST;  // 记录NPC列表

    virtual void init(CChar* me);
    virtual CDoor* present_door(const char* id);

    CBox* add_box(const char* box_type = "box");
    virtual CDoor* add_door(const char* door, const char* next_room,
                            const char* nNextDoor, const char* dir = "");
    virtual void create();
    virtual int32_t query_attr(int attr);
    virtual void reset();

    // 增加NPC（由 Lua 绑定层调用，故为 public）
    virtual CNpc* add_npc(const char* base_name, int32_t bx = 0, int32_t by = 0,
                          int32_t ex = 0, int32_t ey = 0, int dir = 0,
                          int active = 0, int ival = 0);

    CRoom();
    virtual ~CRoom();

protected:
    static void setup_city(CRoom* city);

    // 门工厂：创建本房间类型的出口对象。普通房产 CDoor，
    // 副本房（CFBRoom）重写为产 CFBDoor。add_door 经此创建后统一入
    // m_door_list。
    virtual CDoor* new_door();

    // reset 的物品清除块（跨两遍 wait_clean 标记清除非角色物件）。
    // CRoom::reset 与 CFBRoom::reset 共用——副本房只清物、不做 NPC 复位。
    void clean_up_items();

    DTEXITLIST m_door_list;      // 出口列表。
    DTOBJECTLIST m_object_list;  // 物件列表

    int m_box_num;
};
