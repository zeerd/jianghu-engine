// test_lua_quest.cpp - 任务系统经通用挂点接入的契约
//
// 任务系统不再有 C++ 专用通道：数据集把 quest_mgr 装进全局 hooks 表，
// 引擎只认两个通用挂点（pre_talk / pre_accept_object）；NPC 击杀结算复用
// 引擎既有的 hooks.kill_bonus（唯一的击杀结算点），不另开挂点。
// 本文件守护挂点的调用点契约：
//   1. 缺席 = 不适用，C++ 走自身默认流程（询问菜单 / 不接收物品）；
//   2. pre_talk 的 handled / flow 两个字段：接管对话 / 归引擎标准询问流程；
//   3. pre_accept_object 的 handled 字段：物品交付已处理；
//   4. 一次对话只咨询一次钩子（CNpc 与 Lua NPC 两条路径都成立）。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>

#include "Char.h"
#include "Container.h"
#include "CReward.h"
#include "Global.h"
#include "Item.h"
#include "Npc.h"
#include "Room.h"
#include "User.h"
#include "LuaEngine.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];
extern MAP2USER g_user_list;

namespace {

// 往 hooks 表装一个钩子：body 是一段 Lua 表达式（函数体或 nil）。
void set_hook(const char* name, const char* body)
{
    lua_State* L     = g_LuaEngine->get_state();
    std::string code = std::string("hooks.") + name + " = " + body;
    luaL_dostring(L, code.c_str());
}

// 读 hooks 表里的全局计数（钩子被咨询了几次）
int hook_counter(const char* name)
{
    lua_State* L = g_LuaEngine->get_state();
    lua_getglobal(L, name);
    int value = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return value;
}

// 读 hooks 表里的全局字符串（钩子收到的实参）
std::string hook_string(const char* name)
{
    lua_State* L = g_LuaEngine->get_state();
    lua_getglobal(L, name);
    const char* text  = lua_tostring(L, -1);
    std::string value = text ? text : "";
    lua_pop(L, 1);
    return value;
}

}  // namespace

class LuaQuestHookTest : public ::testing::Test {
protected:
    CChar* player    = nullptr;
    CNpc* npc        = nullptr;
    CContainer* item = nullptr;
    CContainer* room = nullptr;

    void SetUp() override
    {
        // 每次重装 hooks 表：上一个用例装进去的钩子不会漏到下一个
        ensure_lua_env();
        // 钩子计数用的全局变量不受 hooks 表重装影响，单独复位
        luaL_dostring(g_LuaEngine->get_state(),
                      "_G.talk_count = nil _G.give_count = nil "
                      "_G.kill_count = nil");

        room = new CContainer();
        room->set("name", "test_room");

        player = new CChar();
        player->set("name", "玩家");
        player->set("id", "player1");
        player->move(room);

        npc = new CNpc();
        npc->set("name", "任务NPC");
        npc->set("id", "quest_npc");
        npc->move(room);

        item = new CContainer();
        item->set("name", "任务物品");
        item->set("id", "quest_item");
    }

    void TearDown() override
    {
        g_user_list.clear();
        item->move(nullptr);
        npc->move(nullptr);
        player->move(nullptr);
        room->move(nullptr);
        delete item;
        delete npc;
        delete player;
        delete room;
    }
};

// ====================================================================
// pre_talk：通用对话钩子
// ====================================================================

// 钩子接管：对话终止，不走询问菜单，实参按 (npc, me, ask) 传入。
TEST_F(LuaQuestHookTest, PreTalk_Handled_TerminatesDialog)
{
    set_hook("pre_talk",
             "function(npc, me, ask)\n"
             "  _G.talk_count = (_G.talk_count or 0) + 1\n"
             "  _G.talk_npc_id = npc:querystr('id')\n"
             "  _G.talk_me_id = me:querystr('id')\n"
             "  _G.talk_ask = ask\n"
             "  return { handled = true }\n"
             "end");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(1, npc->do_talk(player, (char*)"身世"));
    EXPECT_EQ(1, hook_counter("talk_count")) << "一次对话只咨询一次钩子";
    EXPECT_EQ("quest_npc", hook_string("talk_npc_id"));
    EXPECT_EQ("player1", hook_string("talk_me_id"));
    EXPECT_EQ("身世", hook_string("talk_ask"));
    EXPECT_STREQ(g_notify_fail_msg, "") << "接管后不走询问菜单";
}

// flow = "engine"：不接管，但本次对话归引擎标准询问流程。
TEST_F(LuaQuestHookTest, PreTalk_FlowEngine_UsesEngineInquiry)
{
    set_hook("pre_talk",
             "function()\n"
             "  _G.talk_count = (_G.talk_count or 0) + 1\n"
             "  return { flow = 'engine' }\n"
             "end");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, npc->do_talk(player, (char*)"身世"));
    EXPECT_EQ(1, hook_counter("talk_count")) << "钩子被咨询过";
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。")
        << "归引擎流程：无 C++ 询问项时按引擎默认提示";
}

// 钩子缺席：不咨询，C++ 走自身默认流程。
TEST_F(LuaQuestHookTest, PreTalk_Absent_FallsBackToDefault)
{
    set_hook("pre_talk", "nil");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, npc->do_talk(player, (char*)"身世"));
    EXPECT_EQ(0, hook_counter("talk_count")) << "缺席时不咨询任何钩子";
    EXPECT_STREQ(g_notify_fail_msg, "对方并不愿意理你。");
}

// query_talk_hook 直接读钩子返回表的字段。
TEST_F(LuaQuestHookTest, QueryTalkHook_ReadsHandledAndFlow)
{
    TalkHookResult result;

    set_hook("pre_talk", "function() return { handled = true } end");
    result = query_talk_hook(npc, player, (char*)"身世");
    EXPECT_TRUE(result.handled);
    EXPECT_FALSE(result.engine_flow);

    set_hook("pre_talk", "function() return { flow = 'engine' } end");
    result = query_talk_hook(npc, player, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_TRUE(result.engine_flow);

    // 未知的 flow 值 = 不接管、归对象自身流程
    set_hook("pre_talk", "function() return { flow = 'script' } end");
    result = query_talk_hook(npc, player, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);

    // 空表 = 不接管、归对象自身流程
    set_hook("pre_talk", "function() return {} end");
    result = query_talk_hook(npc, player, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);

    // 钩子缺席 = 不适用
    set_hook("pre_talk", "nil");
    result = query_talk_hook(npc, player, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);

    // 空实参：不咨询钩子
    result = query_talk_hook(nullptr, player, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);
    result = query_talk_hook(npc, nullptr, (char*)"身世");
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);
    result = query_talk_hook(npc, player, nullptr);
    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.engine_flow);
}

// ====================================================================
// pre_accept_object：通用交物钩子
// ====================================================================

// handled = true：物品交付已处理，引擎不再走默认逻辑。
TEST_F(LuaQuestHookTest, PreAcceptObject_Handled_ReturnsOne)
{
    set_hook("pre_accept_object",
             "function(npc, me, item)\n"
             "  _G.give_count = (_G.give_count or 0) + 1\n"
             "  _G.give_npc_id = npc:querystr('id')\n"
             "  _G.give_me_id = me:querystr('id')\n"
             "  _G.give_item_id = item:querystr('id')\n"
             "  return { handled = true }\n"
             "end");

    EXPECT_EQ(1, npc->accept_object(player, item));
    EXPECT_EQ(1, hook_counter("give_count"));
    EXPECT_EQ("quest_npc", hook_string("give_npc_id"));
    EXPECT_EQ("player1", hook_string("give_me_id"));
    EXPECT_EQ("quest_item", hook_string("give_item_id"));
}

// 不接管 / 钩子缺席：按引擎默认（不接收）处理。
TEST_F(LuaQuestHookTest, PreAcceptObject_NotHandled_ReturnsZero)
{
    set_hook("pre_accept_object", "function() return { handled = false } end");
    EXPECT_EQ(0, npc->accept_object(player, item));

    set_hook("pre_accept_object", "nil");
    EXPECT_EQ(0, npc->accept_object(player, item));
    EXPECT_EQ(0, hook_counter("give_count"));
}

// ====================================================================
// NPC 击杀结算：任务结算复用引擎既有的 hooks.kill_bonus，不另开挂点
// ====================================================================

// hooks.kill_bonus 是唯一的 NPC 击杀结算点：既可改写入账经验，也可承接
// 任务/悬赏结算（数据集在钩子里调任务模块）。
TEST_F(LuaQuestHookTest, KillBonus_IsTheNpcKillSettlementPoint)
{
    CUser* killer = new CUser();
    killer->set_name("猎手", "quest_killer");
    killer->set("id", "quest_killer");
    killer->set("level", (long)10);
    killer->move(room);

    set_hook("kill_bonus",
             "function(killer, victim, reward)\n"
             "  _G.kill_count = (_G.kill_count or 0) + 1\n"
             "  _G.kill_killer_id = killer:querystr('id')\n"
             "  _G.kill_victim_id = victim:querystr('id')\n"
             "  _G.kill_reward = reward\n"
             "  return { reward = 100 }\n"
             "end");

    g_reward.killer_reward(killer, npc);

    EXPECT_EQ(1, hook_counter("kill_count")) << "击杀结算只咨询一次钩子";
    EXPECT_EQ("quest_killer", hook_string("kill_killer_id"));
    EXPECT_EQ("quest_npc", hook_string("kill_victim_id"));
    EXPECT_EQ(100, killer->query("kill/nkgain")) << "钩子返回值改写入账值";

    killer->move(nullptr);
    delete killer;
}

// 钩子缺席：击杀结算照常，按引擎封顶值入账。
TEST_F(LuaQuestHookTest, KillBonus_Absent_KeepsReward)
{
    CUser* killer = new CUser();
    killer->set_name("猎手", "quest_killer2");
    killer->set("id", "quest_killer2");
    killer->set("level", (long)10);
    killer->move(room);

    set_hook("kill_bonus", "nil");

    g_reward.killer_reward(killer, npc);
    EXPECT_EQ(0, hook_counter("kill_count"));
    EXPECT_GT(killer->query("kill/nkgain"), 0L) << "经验照常入账";

    killer->move(nullptr);
    delete killer;
}
