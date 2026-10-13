// test_setup_objects.cpp - Unit tests for the Create_* factories returning
// std::unique_ptr (Phase 2 of the new/delete -> smart pointer migration), plus
// the DB factory and CFightRoom::getTeamList ownership.
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include <memory>
#include <string>

#include "Condition.h"
#include "Char.h"
#include "Container.h"
#include "FB.h"
#include "FBRoom.h"
#include "FightRoom.h"
#include "Item.h"
#include "Mapping.h"
#include "Npc.h"
#include "Room.h"
#include "Skill.h"
#include "Throwing.h"
#include "IDatabase.h"
#include "test_log_hits.h"
#include "test_lua_env.h"

// Factory declarations (defined in SetupObjects.cpp / db wrappers)
std::unique_ptr<CSkill> create_skill(string skill_name);
std::unique_ptr<CCondition> create_condition(string base_name);
std::unique_ptr<CContainer> create_item(string base_name, string unique_id);
std::unique_ptr<CNpc> create_npc(string base_name);
std::unique_ptr<CRoom> create_room(string base_name);
std::unique_ptr<CFB> create_fb(string fb_name);
std::unique_ptr<CFBRoom> create_fb_room(string room_name, CFB* fb);
std::unique_ptr<IDatabase> create_sqlite_db();

// Fixture: boot the real Lua engine so ScriptExists / GetScriptType work.
class SetupObjectsTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

// ====================================================================
// Factories return a non-null unique_ptr that owns a live object
// ====================================================================
TEST_F(SetupObjectsTest, CreateItem_SpecialTypes)
{
    std::unique_ptr<CContainer> coin = create_item("coin", "");
    ASSERT_NE(coin, nullptr);
    EXPECT_EQ(coin->query_attr(CMainObject::IS_MONEY), 1);

    std::unique_ptr<CContainer> box = create_item("box", "");
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->query_attr(CMainObject::IS_BOX), 1);
}

// 无 item 脚本的 C++ 特型（coin/box/corpse_ob…）：创建时不得向脚本引擎
// 发起加载尝试。否则每次造 coin 都会写一条 "cannot read file" ERROR
// 加完整回溯，并重做一次失败的文件读取（load 失败不缓存）。
TEST_F(SetupObjectsTest, CreateItem_SpecialTypesSkipScriptLookup)
{
    const char* needle = "[Lua] coin: cannot read file";
    const int before   = count_log_hits(needle);

    std::unique_ptr<CContainer> coin = create_item("coin", "");
    ASSERT_NE(coin, nullptr);
    EXPECT_EQ(count_log_hits(needle), before)
        << "create_item 对无脚本特型发起了脚本加载并写入 ERROR 日志";

    const char* box_needle          = "[Lua] box: cannot read file";
    const int box_before            = count_log_hits(box_needle);
    std::unique_ptr<CContainer> box = create_item("box", "");
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(count_log_hits(box_needle), box_before);
}

TEST_F(SetupObjectsTest, CreateItem_FallbackIsOwned)
{
    // Unknown item still yields a valid object (base CItem path); unique_ptr
    // guarantees it is released even though we never touch it again.
    std::unique_ptr<CContainer> t = create_item("no_such_item_zzz", "");
    ASSERT_NE(t, nullptr);
    EXPECT_NE(t->object_id(), 0);
}

TEST_F(SetupObjectsTest, CreateNpc_ReturnsOwnedPointer)
{
    std::unique_ptr<CNpc> npc = create_npc("no_such_npc_zzz");
    ASSERT_NE(npc, nullptr);
    EXPECT_EQ(npc->query_attr(CNpc::IS_NPC), 1);
}

TEST_F(SetupObjectsTest, CreateRoom_ReturnsOwnedPointer)
{
    std::unique_ptr<CRoom> room = create_room("no_such_room_zzz");
    ASSERT_NE(room, nullptr);
    EXPECT_EQ(room->query_attr(CChar::IS_ROOM), 1);
}

TEST_F(SetupObjectsTest, CreateSkill_ReturnsOwnedPointer)
{
    std::unique_ptr<CSkill> sk = create_skill("no_such_skill_zzz");
    ASSERT_NE(sk, nullptr);
    EXPECT_EQ(sk->query_attr(CSkill::IS_SKILL), 1);
}

// base="throwing" → CLuaThrowing 工厂分支（区别于默认 CLuaSkill）
TEST_F(SetupObjectsTest, CreateSkill_ThrowingBaseSelectsThrowing)
{
    std::unique_ptr<CSkill> sk = create_skill("tc_throwing");
    ASSERT_NE(sk, nullptr);
    EXPECT_NE(dynamic_cast<CThrowing*>(sk.get()), nullptr);
}

TEST_F(SetupObjectsTest, CreateCondition_ReturnsOwnedPointer)
{
    std::unique_ptr<CCondition> cnd = create_condition("no_such_cnd_zzz");
    ASSERT_NE(cnd, nullptr);
}

TEST_F(SetupObjectsTest, CreateFb_UnknownReturnsNull)
{
    // No lua/room/<name>.lua for a bogus name -> factory yields null, and the
    // unique_ptr simply holds nothing (no leak, no dangling pointer).
    std::unique_ptr<CFB> fb = create_fb("no_such_fb_zzz");
    EXPECT_EQ(fb, nullptr);
}

TEST_F(SetupObjectsTest, CreateFbRoom_ReturnsOwnedPointer)
{
    std::unique_ptr<CFBRoom> r = create_fb_room("no_such_fbroom_zzz", nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->query_attr(CChar::IS_FBROOM), 1);
}

// ====================================================================
// DB factory: unique_ptr ownership
// ====================================================================
TEST(SetupObjectsDb, CreateSqliteDb_ReturnsOwnedPointer)
{
    std::unique_ptr<IDatabase> db = create_sqlite_db();
    ASSERT_NE(db, nullptr);
    // unique_ptr goes out of scope here; no manual delete required.
}

TEST(SetupObjectsDb, InterfaceCreateSqlite)
{
    std::unique_ptr<IDatabase> db = IDatabase::create("sqlite");
    ASSERT_NE(db, nullptr);
}

TEST(SetupObjectsDb, InterfaceCreateUnknownReturnsNull)
{
    std::unique_ptr<IDatabase> db = IDatabase::create("nope");
    EXPECT_EQ(db, nullptr);
}

// ====================================================================
// CFightRoom::getTeamList returns a unique_ptr<vector> (was a leaking raw new)
// ====================================================================
TEST(SetupObjectsFightRoom, GetTeamListEmpty)
{
    CFightRoom fr;
    std::unique_ptr<TeamList> list = fr.get_team_list(0);
    // No members -> empty (possibly null) list; either way it is owned.
    if (list) EXPECT_EQ((int)list->size(), 0);
}

TEST(SetupObjectsFightRoom, GetTeamListWithMembers)
{
    CFightRoom fr;
    CChar a, b;
    fr.m_Team[0].obj[0]      = &a;
    fr.m_Team[0].obj[1]      = &b;
    fr.m_Team[0].fighter_num = 2;

    std::unique_ptr<TeamList> list = fr.get_team_list(0, NULL, 10);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ((int)list->size(), 2);
}

TEST(SetupObjectsFightRoom, GetTeamListRespectsNumLimit)
{
    CFightRoom fr;
    CChar a, b;
    fr.m_Team[0].obj[0]      = &a;
    fr.m_Team[0].obj[1]      = &b;
    fr.m_Team[0].fighter_num = 2;

    // num < 1 -> null (no allocation to leak)
    EXPECT_EQ(fr.get_team_list(0, NULL, 0), nullptr);
}
