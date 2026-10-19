// test_npc_cleanup.h — 测试共用：按 base_name 摘除并销毁 load_npc 装载的真实
// NPC（g_npc_list 登记）。房间先于 NPC 析构会让仆人的 environment 悬垂，
// 之后对仆人 move(nullptr) 即 UAF——因此删除房前必须先摘 NPC。
// 房屋与居民区测试都需要，按"重复 setup 合并"原则抽出共享。
#pragma once

#include "Global.h"
#include "Npc.h"
#include "NpcMgr.h"

// 从 g_npc_list 摘除并销毁本用例装载的 NPC
inline void discard_npc(CNpc* npc)
{
    if (!npc) return;
    npc->set_heart_beat(0);
    npc->move(nullptr);
    g_npc_list.erase(npc->object_id());
    delete npc;
}

// 按 base_name 找 NPC（reset 复生会换 object_id，按 id 追不住）
inline CNpc* find_npc_by_base_name(const char* base_name)
{
    for (auto& kv : g_npc_list) {
        if (EQUALSTR(kv.second->querystr("base_name"), base_name))
            return kv.second;
    }
    return nullptr;
}
