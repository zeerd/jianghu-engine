/**
 * @file HeartBeat.cpp
 * @brief 心跳系统  */

#include "HeartBeat.h"
#include "../object/Global.h"
#include "../object/Container.h"
#include "Room.h"
#include "../lua/LuaEngine.h"
#include "../util/Logger.h"

#include <Windows.h>
#include <process.h>
#include <ctime>

//---------------------------------------------------------
// 心跳链表定义
//---------------------------------------------------------
typedef map<int32_t, CContainer*, less<int32_t> > MAP2HEARTBEAT;
MAP2HEARTBEAT g_heart_beat;

// 延迟销毁列表定义
std::list<CContainer*> g_destruct_list;
std::mutex g_destruct_list_mutex;

//---------------------------------------------------------
// 冲刷延迟销毁列表：停止心跳、脱离容器并销毁
//---------------------------------------------------------
void flush_destruct_list()
{
    std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
    auto it = g_destruct_list.begin();
    while (it != g_destruct_list.end()) {
        CContainer* me = *it;
        me->set_destructed();
        me->set_heart_beat(0);  // 停止心跳
        me->move(NULL);
        me->clean_up();
        delete me;
        it = g_destruct_list.erase(it);
    }
}

//---------------------------------------------------------
// 内部函数：NPC 刷新（每 5 分钟）
//---------------------------------------------------------
static void look_for_objects_to_swap()
{
    static int next_time = g_boot_time;

    if (g_current_time < next_time) return;
    next_time = g_current_time + 900;

    iterator_room pos;

    LOG_TRACE("reset room %ld>>", g_room_list.size());
    for (pos = g_room_list.begin(); pos != g_room_list.end(); pos++) {
        (pos->second)->reset();
    }
    LOG_TRACE(">>");
}

//---------------------------------------------------------
// 心跳定时器线程
//---------------------------------------------------------
void CDECL alarm_loop(void* ignore)
{
    while (1) {
        Sleep(1000);
        g_heart_beat_flag = 1;
    }
}

//---------------------------------------------------------
// 心跳主循环
//---------------------------------------------------------
void call_heart_beat()
{
    LOG_TRACE("IN");
    g_current_time = static_cast<int>(time(NULL));

    // Lua 脚本自动重载检查（每 5 秒扫描一次）
    static time_t lastLuaCheck = 0;
    if (g_current_time - lastLuaCheck >= 5) {
        lastLuaCheck = g_current_time;
        g_LuaEngine->check_for_reload();
    }

    CContainer* obj;
    MAP2HEARTBEAT::iterator p, prep;

    for (p = g_heart_beat.begin(); p != g_heart_beat.end();) {
        prep = p;
        p++;

        obj = prep->second;

        if (obj) {
            obj->heart_beat();
        }
        else
            g_heart_beat.erase(prep);
    }

    look_for_objects_to_swap();
    LOG_TRACE("OUT");
}

//---------------------------------------------------------
// 添加/移除心跳对象
//---------------------------------------------------------
void set_heart_beat_obj(CContainer* obj, int flag)
{
    MAP2HEARTBEAT::iterator p;
    int32_t id = obj->object_id();
    p          = g_heart_beat.find(id);

    if (flag) {
        if (p != g_heart_beat.end()) return;
        g_heart_beat[id] = obj;
    }
    else {
        if (p == g_heart_beat.end()) return;
        p->second = 0;
    }
}

//---------------------------------------------------------
// 心跳释放（空实现，延迟销毁由主循环处理）
//---------------------------------------------------------
void heart_beat_release() {}
