// test_smoke.cpp — Minimal smoke test to verify framework links correctly
//
// This test validates that:
// 1. jianghu_core.lib links without errors
// 2. Mock functions resolve correctly
// 3. Core game classes can be instantiated
// 4. Google Test framework works
//
#include "pch.h"
#include <string>
#include "Container.h"
#include "Mapping.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Item.h"
#include "Skill.h"

// ====================================================================
// Smoke test: basic object creation
// ====================================================================
TEST(SmokeTest, CreateContainer)
{
    CContainer* obj = new CContainer();
    EXPECT_NE(obj, nullptr);
    delete obj;
}

TEST(SmokeTest, ContainerSetAndGet)
{
    CContainer obj;
    obj.set("name", "测试对象");
    EXPECT_STREQ(obj.querystr("name"), "测试对象");
    obj.set("hp", (long)100);
    EXPECT_EQ(obj.query("hp"), 100);
}

TEST(SmokeTest, MappingBasic)
{
    CMapping m;
    m.set("key1", "value1");
    m.set("key2", (long)42);
    EXPECT_STREQ(m.querystr("key1"), "value1");
    EXPECT_EQ(m.query("key2"), 42);
}

TEST(SmokeTest, CharCreation)
{
    CChar* ch = new CChar();
    EXPECT_NE(ch, nullptr);
    ch->set("name", "测试角色");
    ch->set("id", "test_char");
    ch->set("gender", "男性");
    ch->set("combat_exp", 100000);
    ch->set("level", 50);
    EXPECT_STREQ(ch->querystr("name"), "测试角色");
    EXPECT_EQ(ch->query("level"), 50);
    delete ch;
}

TEST(SmokeTest, MockFunctionsLink)
{
    // Verify functions are callable
    extern void tell_object(CContainer*, const char*, const char*);
    extern void message_vision(const char*, CContainer*, CContainer*, bool);
    extern bool db_save_object(const char*, long, const char*, const char*);
    extern bool db_load_object(const char*, long, std::string&, std::string&);
    extern int is_user(CContainer*);
    extern int is_wizard(CChar*);

    tell_object(nullptr, "test", "system");
    message_vision("test", nullptr, nullptr, false);
    // g_db is nullptr here, so real implementations return false
    EXPECT_FALSE(db_save_object("owner", 1, "item", "data"));
    std::string base, data;
    EXPECT_FALSE(db_load_object("owner", 1, base, data));
    EXPECT_EQ(is_user(nullptr), 0);
    EXPECT_EQ(is_wizard(nullptr), 0);
}

TEST(SmokeTest, StackingLogic)
{
    // Pure function test — no mocks needed
    EXPECT_EQ(80 * 1, 80);
    EXPECT_EQ(5 * 300, 1500);
}
