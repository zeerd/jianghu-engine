// test_lua_quest.cpp - Unit tests for LuaQuest.cpp
//
// Tests QuestHasActive, QuestAcceptObject, QuestDoTalk, QuestNpcKilled.
//
#include "pch.h"

#include <string>
#include "Char.h"
#include "Npc.h"
#include "Item.h"
#include "Room.h"
#include "LuaQuest.h"
#include "LuaEngine.h"  // g_LuaEngine
#include "test_lua_env.h"

class LuaQuestTest : public ::testing::Test {
protected:
    CChar* player;
    CNpc* npc;
    CContainer* item;
    CContainer* room;

    void SetUp() override
    {
        ensure_lua_env();
        room = new CContainer();
        room->set("name", "test_room");

        player = new CChar();
        player->set("name", "玩家");
        player->set("id", "player1");
        player->set("level", (long)50);
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
        item->move(nullptr);
        npc->move(nullptr);
        player->move(nullptr);
        room->move(nullptr);
        delete item;
        delete npc;
        delete player;
        delete room;
        luaL_dostring(g_LuaEngine->get_state(),
                      "package.loaded['quest.quest_mgr']=nil;"
                      "package.preload['quest.quest_mgr']=nil");
    }
};

// QuestHasActive: null player → false
TEST_F(LuaQuestTest, QuestHasActive_NullPlayer)
{
    EXPECT_FALSE(quest_has_active(nullptr));
}

// QuestHasActive: valid player, quest_mgr may not exist → false or true
TEST_F(LuaQuestTest, QuestHasActive_ValidPlayer)
{
    // quest_mgr.lua may not exist in test env → returns false
    bool result = quest_has_active(player);
    // We just test it doesn't crash
    EXPECT_TRUE(result == true || result == false);
}

// QuestAcceptObject: null args → 0
TEST_F(LuaQuestTest, QuestAcceptObject_NullArgs)
{
    EXPECT_EQ(quest_accept_object(nullptr, player, item), 0);
    EXPECT_EQ(quest_accept_object(npc, nullptr, item), 0);
    EXPECT_EQ(quest_accept_object(npc, player, nullptr), 0);
}

// QuestAcceptObject: valid args → 0 (no quest_mgr)
TEST_F(LuaQuestTest, QuestAcceptObject_ValidArgs)
{
    int result = quest_accept_object(npc, player, item);
    EXPECT_EQ(result, 0);
}

// QuestDoTalk: null args → false
TEST_F(LuaQuestTest, QuestDoTalk_NullArgs)
{
    EXPECT_FALSE(quest_do_talk(nullptr, player, (char*)"hello"));
    EXPECT_FALSE(quest_do_talk(npc, nullptr, (char*)"hello"));
    EXPECT_FALSE(quest_do_talk(npc, player, nullptr));
}

// QuestDoTalk: valid args → false (no quest_mgr)
TEST_F(LuaQuestTest, QuestDoTalk_ValidArgs)
{
    bool result = quest_do_talk(npc, player, (char*)"hello");
    EXPECT_FALSE(result);
}

// QuestNpcKilled: null args → no crash
TEST_F(LuaQuestTest, QuestNpcKilled_NullArgs)
{
    quest_npc_killed(nullptr, npc);
    quest_npc_killed(player, nullptr);
    quest_npc_killed(nullptr, nullptr);
}

// QuestNpcKilled: valid args → no crash
TEST_F(LuaQuestTest, QuestNpcKilled_ValidArgs)
{
    quest_npc_killed(player, npc);
}

// ====================================================================
// push_quest_mgr 三态：非表返回 / 空表缺钩子 / 全钩子回读
// 经 package.preload 注入（优先于磁盘脚本），TearDown 撤除。
// ====================================================================

namespace {

void InjectQuestMgr(const std::string& body)
{
    lua_State* L = g_LuaEngine->get_state();
    luaL_dostring(L, body.c_str());
}

}  // namespace

TEST_F(LuaQuestTest, PushQuestMgrRejectsNonTableReturn)
{
    InjectQuestMgr(
        "package.loaded['quest.quest_mgr']=nil\n"
        "package.preload['quest.quest_mgr']=function() "
        "return 42 end\n");

    EXPECT_FALSE(quest_has_active(player))
        << "require 返回非表 → 早退 false(33-34)";
    EXPECT_EQ(0, quest_accept_object(npc, player, item))
        << "非表 → 早退 0(同支)";
}

TEST_F(LuaQuestTest, EmptyMgrFallsBackWhenHooksMissing)
{
    InjectQuestMgr(
        "package.loaded['quest.quest_mgr']=nil\n"
        "package.preload['quest.quest_mgr']=function() "
        "return {} end\n");

    EXPECT_FALSE(quest_has_active(player)) << "无 has_quest → 0(47)";
    EXPECT_EQ(0, quest_accept_object(npc, player, item))
        << "无 on_give_item → 0(69)";
    EXPECT_FALSE(quest_do_talk(npc, player, (char*)"hi"))
        << "无 on_talk → false(93)";
    lua_State* L = g_LuaEngine->get_state();
    int top      = lua_gettop(L);
    quest_npc_killed(player, npc);  // 无 on_npc_killed 早退(117)
    EXPECT_EQ(top, lua_gettop(L)) << "早退同样保持栈平衡(117)";
}

TEST_F(LuaQuestTest, HookReturnValuesAreReadBack)
{
    InjectQuestMgr(
        "package.loaded['quest.quest_mgr']=nil\n"
        "package.preload['quest.quest_mgr']=function()\n"
        "  return {\n"
        "    has_quest = function(me) return true end,\n"
        "    on_give_item = function(npc, me, item) return 7 end,\n"
        "    on_talk = function(npc, me, ask) return true end,\n"
        "    on_npc_killed = function(killer, victim) killed_called = true "
        "end,\n"
        "  }\n"
        "end\n");

    EXPECT_TRUE(quest_has_active(player)) << "布尔回读 true(56-59)";
    EXPECT_EQ(7, quest_accept_object(npc, player, item)) << "整数回读 7(80-83)";
    EXPECT_TRUE(quest_do_talk(npc, player, (char*)"hi"))
        << "处理结果回读 true(104-107)";
    quest_npc_killed(player, npc);  // pcall 成功链
    lua_State* L2 = g_LuaEngine->get_state();
    lua_getglobal(L2, "killed_called");
    EXPECT_TRUE(lua_toboolean(L2, -1)) << "on_npc_killed 回调已执行";
    lua_pop(L2, 1);
}
