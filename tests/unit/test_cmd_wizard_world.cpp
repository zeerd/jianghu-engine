// test_cmd_wizard_world.cpp - Unit tests for CmdWizardWorld.cpp
//
// Tests wizard world state without IO-heavy operations.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Room.h"
#include "Item.h"
#include "CmdWizard.h"
#include "Global.h"               // g_npc_list / EQUALSTR
#include "mock/mock_itemmgr.h"    // CMockItemMgr
#include "test_lua_env.h"         // ensure_lua_env（load_npc 前置）
#include "test_state_recorder.h"  // 巫师播报的可观察面
#include "HeartBeat.h"            // g_destruct_list 快照回收
#include "Corpse.h"               // smash 毁灭角色的尸体工厂
#include "FightRoom.h"            // smash 战房收场

extern char g_notify_fail_msg[512];

class CmdWizardWorldTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallWhereMe(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"where", me, target, arg);
    }
    static int CallCloneNpc(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"clonenpc", me, target, arg);
    }
    static int CallCloneObject(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"clone", me, target, arg);
    }
    static int CallSmash(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"smash", me, target, arg);
    }
    static int CallData(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"data", me, target, arg);
    }

    CChar* me;
    CNpc* target_npc;
    CContainer* room;
    // SetUp 快照：TearDown 只回收本用例新增的自毁对象（smash 的战房/尸体）
    std::vector<CContainer*> m_live_at_setup;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        room = new CContainer();
        room->set("name", "test_room");
        room->set("wiz_room", (long)1);

        me = new CChar();
        me->set("name", "测试巫师");
        me->set("id", "wiz1");
        me->set("level", (long)100);
        me->set("grant", (long)200);
        me->set("hp", (long)1000);
        me->move(room);

        target_npc = new CNpc();
        target_npc->set("name", "目标NPC");
        target_npc->set("id", "target_npc");
        target_npc->move(room);
    }

    void TearDown() override
    {
        // 延迟析构：本用例内被 smash/destruct 推入列表的对象
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete ob;
            }
        }

        target_npc->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete target_npc;
        delete me;
        delete room;
    }
};

TEST_F(CmdWizardWorldTest, Data_NoTarget)
{
    int result = CallData(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdWizardWorldTest, Data_ValidTarget)
{
    int result = CallData(me, target_npc->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdWizardWorldTest, CloneNpc_NoDb)
{
    // create_npc 有 fn- 回退（SetupObjects.cpp）→ 不存在的名字也能克隆；
    // load_npc 前置需要 g_LuaEngine 已初始化（幂等）
    ASSERT_NE(ensure_lua_env(), nullptr);
    int result = CallCloneNpc(me, 0, (char*)"clone_zzz");
    EXPECT_EQ(result, 1);

    // 清理注册进 g_npc_list 的克隆物（摘心跳→出列→删），避免污染后续用例
    for (auto it = g_npc_list.begin(); it != g_npc_list.end();) {
        if (EQUALSTR(it->second->querystr("base_name"), "clone_zzz")) {
            CNpc* clone = it->second;
            it          = g_npc_list.erase(it);
            clone->set_heart_beat(0);
            clone->move(nullptr);
            delete clone;
        }
        else {
            ++it;
        }
    }
}

TEST_F(CmdWizardWorldTest, CloneObject_NoDb)
{
    // mock 无 factory → load_item 返回 null → 克隆失败返回 0
    CMockItemMgr mgr;
    // 换入前保存进程共享管理器（test_env 的 g_testItemMgr），归还而非置空
    IItemMgr* saved_mgr = g_item_mgr;
    g_item_mgr          = &mgr;
    int result          = CallCloneObject(me, 0, (char*)"clone_sword_zzz");
    EXPECT_EQ(result, 0);
    g_item_mgr = saved_mgr;
}

namespace {
// smash 毁灭角色经工厂造尸体：登记后由用例回收
std::vector<CContainer*> g_smash_corpses;

CContainer* SmashCorpseFactory(const char* base_name)
{
    if (strcmp(base_name, "corpse_ob") == 0) {
        auto* c = new CCorpse();
        c->set("base_name", base_name);
        g_smash_corpses.push_back(c);
        return c;
    }
    return nullptr;
}

// 录制巫师：tell/chat 播报的可观察面（grant 覆盖 where/clone/smash/data 门）
StateRecordingUser* MakeRecordingWiz(CContainer* room)
{
    auto* u = new StateRecordingUser();
    u->set_name("巫师", "wiz_rec");
    u->set("grant", (long)200);
    u->set("hp", (long)1000);
    u->move(room);
    u->delete_temp("client/loading");
    return u;
}
}  // namespace

// ---------------- where：物件寻踪三态 ----------------

TEST_F(CmdWizardWorldTest, Where_NotExist_TellsSpeaker)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);

    EXPECT_EQ(1, CallWhereMe(wiz, 0, (char*)"999999")) << "查无此物也要回执";
    EXPECT_TRUE(wiz->has("该物件不存在"));

    wiz->move(nullptr);
    delete wiz;
}

TEST_F(CmdWizardWorldTest, Where_EnvNull_TellsSpeaker)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    auto* orphan = new StateRecordingUser();  // 注册但从不入房 → 无环境
    orphan->set_name("游魂", "where_orphan");
    orphan->set("id", "where_orphan");
    g_user_list[orphan->object_id()] = orphan;

    EXPECT_EQ(1, CallWhereMe(wiz, orphan->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("无环境"));

    g_user_list.erase(orphan->object_id());
    delete orphan;
    wiz->move(nullptr);
    delete wiz;
}

TEST_F(CmdWizardWorldTest, Where_InRoom_TellsPosition)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    auto* peer              = new StateRecordingUser();
    peer->set_name("同门", "where_peer");
    peer->set("id", "where_peer");
    peer->move(room);
    peer->delete_temp("client/loading");
    g_user_list[peer->object_id()] = peer;

    EXPECT_EQ(1, CallWhereMe(wiz, peer->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("目前处于")) << "回执带所在容器";

    g_user_list.erase(peer->object_id());
    peer->move(nullptr);
    delete peer;
    wiz->move(nullptr);
    delete wiz;
}

// ---------------- data：选中目标与数据打印 ----------------

TEST_F(CmdWizardWorldTest, Data_BadTarget_Rejected)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallData(me, 123456, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("请选中目标"));
}

TEST_F(CmdWizardWorldTest, Data_PrintsArrayAndMapping)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    CVector arr;
    arr.append("刀剑");
    arr.append("拳脚");
    room->set("wz_arr", arr);
    CMapping sub;
    sub.set("k", (long)5);
    room->set("wz_map", sub);

    EXPECT_EQ(1, CallData(wiz, 0, (char*)""));
    EXPECT_TRUE(wiz->has("wz_arr : ({")) << "数组值起始符";
    EXPECT_TRUE(wiz->has("    })")) << "数组值收尾符";
    EXPECT_TRUE(wiz->has("wz_map : ([")) << "映射值起始符";
    EXPECT_TRUE(wiz->has("    ])")) << "映射值收尾符";
    EXPECT_TRUE(wiz->has("k : 5")) << "嵌套数值递归打印";

    wiz->move(nullptr);
    delete wiz;
}

// ---------------- smash：战房收场 / 物件毁灭 / 角色死亡 ----------------

TEST_F(CmdWizardWorldTest, Smash_FightRoom_EndsFight)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    auto* fr                = new CFightRoom();
    fr->set_name("私斗", "smash_fr");
    fr->move(room);

    EXPECT_EQ(1, CallSmash(wiz, fr->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("闪电")) << "毁灭先广播雷击";
    // end_fight 会自毁战房 → 延迟析构列表统一回收，此处不再手动销毁
}

TEST_F(CmdWizardWorldTest, Smash_Item_Destructs)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    auto* item              = new CItem();
    item->set_name("碍眼石", "smash_item");
    item->move(room);

    EXPECT_EQ(1, CallSmash(wiz, item->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("闪电"));
    EXPECT_EQ(nullptr, item->environment()) << "非容器被毁灭后离场";
    // 已推入延迟析构列表，由 TearDown 回收
}

TEST_F(CmdWizardWorldTest, Smash_Character_Dies)
{
    StateRecordingUser* wiz = MakeRecordingWiz(room);
    CChar* foe              = new CChar();
    foe->set_name("妖人", "smash_foe");
    foe->set("hp", (long)100);
    foe->move(room);

    CMockItemMgr mgr;
    IItemMgr* saved_mgr = g_item_mgr;
    g_item_mgr          = &mgr;
    mgr.load_factory    = &SmashCorpseFactory;

    EXPECT_EQ(1, CallSmash(wiz, foe->object_id(), (char*)""));
    EXPECT_TRUE(wiz->has("闪电"));
    EXPECT_FALSE(g_smash_corpses.empty()) << "死亡结算产出尸体";

    g_item_mgr = saved_mgr;
    for (CContainer* c : g_smash_corpses) {
        c->set_heart_beat(0);
        c->move(nullptr);
        delete c;
    }
    g_smash_corpses.clear();
    // 遗体本体已推入延迟析构列表，由 TearDown 回收
}

// ---------------- clone：带编号克隆（管理器工厂 + 注册表回查）
// ----------------

TEST_F(CmdWizardWorldTest, Clone_RegisteredNpc_Succeeds)
{
    CMockItemMgr mgr;
    IItemMgr* saved_mgr = g_item_mgr;
    g_item_mgr          = &mgr;
    mgr.load_factory    = [](const char* base_name) -> CContainer* {
        if (strcmp(base_name, "clone_reg") != 0) return nullptr;
        auto* n = new CNpc();
        n->set_name("克隆体", "clone_reg");
        n->set("base_name", "clone_reg");
        g_npc_list[n->object_id()] = n;  // find_object 回查注册表可命中
        return n;
    };

    char arg[64];  // analyse_string 原地切分："数量 名字"
    snprintf(arg, sizeof(arg), "5 clone_reg");
    int rc = CallCloneObject(me, 0, arg);
    EXPECT_EQ(1, rc) << "克隆成功：注册表可回查即视为已存在";
    EXPECT_NE(nullptr, me->present("clone_reg")) << "克隆体随身";

    g_item_mgr = saved_mgr;
    for (auto it = g_npc_list.begin(); it != g_npc_list.end();) {
        if (EQUALSTR(it->second->querystr("base_name"), "clone_reg")) {
            CNpc* clone = it->second;
            it          = g_npc_list.erase(it);
            clone->set_heart_beat(0);
            clone->move(nullptr);
            delete clone;
        }
        else {
            ++it;
        }
    }
}
