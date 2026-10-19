// test_register_fb.h — 测试共用：注册大厅副本实例的回收
//
// 进注册大厅走 g_item_mgr->load_fb 造出的 CFB 副本总控：它带心跳、副本房
// （base_name=rk）与房内的注册专员，析构又统一走延迟销毁队列 g_destruct_list
// （测试进程没有主循环冲刷它）。因此夹具必须按固定顺序收尾，否则残留会污染
// 后续用例：副本房不在 g_room_list，删了副本本体也不等于清干净。
//
// 固定顺序：副本 → 本用例新增 NPC → 本用例新增房间。人要更早退出。
//
// Build+Run: .\run-coverage.ps1
#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include "FB.h"
#include "Global.h"
#include "HeartBeat.h"  // g_destruct_list：延迟销毁队列
#include "Npc.h"
#include "RegisterHall.h"
#include "Room.h"

// 还活着的注册副本总控（登记在 g_item_list 的 CFB）
inline std::vector<CFB*> live_register_fbs()
{
    std::vector<CFB*> fbs;
    for (auto& kv : g_item_list) {
        CFB* fb = dynamic_cast<CFB*>(kv.second);
        if (!fb || fb->is_destructed()) continue;
        if (EQUALSTR(fb->querystr("base_name"), REGISTER_FB_NAME))
            fbs.push_back(fb);
    }
    return fbs;
}

// 结束所有存活的注册副本并立即回收本轮入队的延迟销毁对象。
// 快照先于 end_fb 取：用例主体自己入队的对象留给夹具的延迟队列回收，
// 本函数只负责副本生命周期自己产生的（副本房、房内专员、副本本体）。
inline void recycle_fbs()
{
    std::vector<CContainer*> queued_before;
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        queued_before.assign(g_destruct_list.begin(), g_destruct_list.end());
    }

    for (CFB* fb : live_register_fbs()) fb->end_fb();

    std::vector<CContainer*> fresh;
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        for (auto it = g_destruct_list.begin(); it != g_destruct_list.end();) {
            CContainer* ob = *it;
            bool queued_early
                = std::find(queued_before.begin(), queued_before.end(), ob)
                  != queued_before.end();
            if (queued_early) {
                ++it;
                continue;
            }
            it = g_destruct_list.erase(it);
            fresh.push_back(ob);
        }
    }
    for (CContainer* ob : fresh) delete ob;
}

// 摘除并销毁本用例新增的 NPC（全局大厅的注册专员等）。
// 摘除必须早于房间删除：~CRoom 不清理房内内容物，先删房会留下悬空环境指针。
inline void discard_new_npcs(const std::vector<int32_t>& live_ids)
{
    std::vector<CNpc*> new_npcs;
    for (auto& kv : g_npc_list) {
        bool known = std::find(live_ids.begin(), live_ids.end(), kv.first)
                     != live_ids.end();
        if (!known) new_npcs.push_back(kv.second);
    }
    for (CNpc* npc : new_npcs) {
        g_npc_list.erase(npc->object_id());
        npc->set_heart_beat(0);
        npc->move(nullptr);
        delete npc;
    }
}

// 删除本用例新增的房间（全局大厅 / fb_registry / 起始房兜底等）
inline void discard_new_rooms(const std::vector<std::string>& live_keys)
{
    std::vector<std::string> new_rooms;
    for (auto& kv : g_room_list) {
        bool known = std::find(live_keys.begin(), live_keys.end(), kv.first)
                     != live_keys.end();
        if (!known) new_rooms.push_back(kv.first);
    }
    for (const std::string& key : new_rooms) {
        auto it = g_room_list.find(key);
        if (it == g_room_list.end()) continue;
        it->second->set_heart_beat(0);
        delete it->second;
        g_room_list.erase(it);
    }
}
