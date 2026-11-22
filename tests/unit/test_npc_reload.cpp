// test_npc_reload.cpp — NPC 脚本热重载：随身物配发必须幂等
//
// 契约：
//  - 脚本热重载（手动 reload 命令 / 自动重载广播）到达已有 NPC 实例时，
//    默认走 purge() + create()（CLuaNpcT::reload_script，脚本定义 reload
//    可覆盖）
//  - CContainer::purge() 只删存档行、不清背包，因此 create() 里的
//    carry_object 不得在旧随身物之上叠加配发：重载后仍是一刀一衣
//  - 随身物不叠 = 死亡掉落只落一份（CCorpse::make_corpse 把随身物整体倾倒，
//    身上有几份就掉几份）
//
// 构建/运行：.\run-unit-tests.ps1
//
#include "pch.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Container.h"
#include "Corpse.h"
#include "Global.h"
#include "HeartBeat.h"  // g_destruct_list：die() 自毁后的回收口径
#include "Item.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "test_lua_env.h"
#include "test_npc_cleanup.h"
#include "mock/mock_itemmgr.h"

extern bool lua_reload_script(const char* name);

namespace {

// 随身物工厂：与 corpse_factory 同序——按基名造新物件；corpse_ob 造真尸体，
// 否则 make_corpse 把 CItem 当 CCorpse 用会踩未定义行为
CContainer* reload_gear_factory(const char* base_name)
{
    if (std::strcmp(base_name, "corpse_ob") == 0) {
        CCorpse* corpse = new CCorpse();
        corpse->set("base_name", base_name);
        corpse->create();
        return corpse;
    }

    CItem* item = new CItem();
    item->set("base_name", base_name);
    return item;
}

class NpcReloadTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom env;

    // reload 前的延迟销毁队列快照：本用例触发的条目（重载清空的旧随身物）
    // 在 TearDown 就地回收，不让它们流到别的用例
    std::vector<CContainer*> live_at_setup;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr             = g_item_mgr;
        item_mgr.load_factory = reload_gear_factory;
        g_item_mgr            = &item_mgr;
        env.set("infinity", 1);

        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        live_at_setup.assign(g_destruct_list.begin(), g_destruct_list.end());
    }

    void TearDown() override
    {
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* pending = *it;
                bool known          = std::find(live_at_setup.begin(),
                                                live_at_setup.end(), pending)
                                      != live_at_setup.end();
                if (known) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete pending;
            }
        }
        g_item_mgr = saved_mgr;
    }

    CNpc* load_gear_npc()
    {
        CNpc* npc = load_npc("t_gear_npc");
        if (npc) npc->move(&env);
        return npc;
    }

    // 随身物不归 g_npc_list 管：先摘出来单独销毁，再摘 NPC
    void discard_gear_npc(CNpc* npc)
    {
        if (!npc) return;
        DTItemList snapshot;
        copy_list(&snapshot, npc->get_item_list());
        for (auto* ob : snapshot) {
            ob->set_heart_beat(0);
            ob->move(nullptr);
            delete ob;
        }
        discard_npc(npc);
    }

    // 统计房间里某基名的物件数量
    int count_in_room(const char* base_name)
    {
        int n = 0;
        for (auto* ob : *env.get_item_list()) {
            if (std::strcmp(ob->querystr("base_name"), base_name) == 0) n++;
        }
        return n;
    }
};

}  // namespace

// 热重载后随身物仍是原有一套：脚本 create() 重跑不得叠加配发
TEST_F(NpcReloadTest, ReloadKeepsSingleGearSet)
{
    CNpc* npc = load_gear_npc();
    ASSERT_NE(npc, nullptr);
    ASSERT_EQ(2, (int)npc->get_item_list()->size()) << "首载只配一刀一衣";

    ASSERT_TRUE(lua_reload_script("t_gear_npc")) << "夹具脚本重载失败";

    EXPECT_EQ(2, (int)npc->get_item_list()->size())
        << "重载后仍是一刀一衣：create() 重跑不得在旧随身物上叠加配发";

    discard_gear_npc(npc);
}

// 重载后的 NPC 被杀：房间只应落出一刀一衣（掉落数 = 生前随身物数）
TEST_F(NpcReloadTest, ReloadThenDieDropsSingleGearSet)
{
    CNpc* npc = load_gear_npc();
    ASSERT_NE(npc, nullptr);
    ASSERT_TRUE(lua_reload_script("t_gear_npc"));

    npc->die();  // 无脚本 die → CChar::die：造尸体 + 倾倒随身物 + DESTRUCT_OB

    EXPECT_EQ(1, count_in_room("t_gear_sword")) << "武器只应掉落一份";
    EXPECT_EQ(1, count_in_room("t_gear_cloth")) << "衣服只应掉落一份";

    // die() 末尾已 DESTRUCT_OB：从延迟销毁队列摘除，避免 double-free
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.remove(npc);
    }

    DTItemList snapshot;
    copy_list(&snapshot, env.get_item_list());
    for (auto* ob : snapshot) {
        ob->set_heart_beat(0);
        ob->move(nullptr);
        delete ob;
    }
    discard_npc(npc);
}
