// test_char_family.cpp - Unit tests for CharFamily.cpp (师徒/关系/跟随).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "IniParser.h"
#include "ServerConfig.h"
#include "test_char_common.h"

class CharFamilyTest : public ::testing::Test {
protected:
    CharMockDb* db;
    void SetUp() override
    {
        db   = new CharMockDb();
        g_db = db;
        ResetConfig();
    }
    void TearDown() override
    {
        ResetConfig();
        g_db = nullptr;
        delete db;
    }

    // 还原 config.ini 的玩法开关默认值，避免影响其它测试
    static void ResetConfig()
    {
        ini_load("tests/test_config.ini");
        CServerConfig::init();
    }

    static CChar* MakeMaster()
    {
        CChar* m = new CChar();
        m->set("name", "master");
        m->set("id", "m1");
        CMapping fam;
        fam.set("family_name", "少林派");
        fam.set("generation", (long)1);
        m->set("family", fam);
        m->set_skill("unarmed", 100);
        return m;
    }
};

// ---------------- is_apprentice_of ----------------
TEST_F(CharFamilyTest, ApprenticeOf_NoFamily)
{
    CChar* a = new CChar();
    a->set("id", "a1");
    CChar* m = MakeMaster();
    EXPECT_EQ(a->is_apprentice_of(m), 0);
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, ApprenticeOf_True)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    a->set("id", "a1");
    CMapping fam;
    fam.set("family_name", "少林派");
    fam.set("master_id", "m1");
    a->set("family", fam);
    EXPECT_EQ(a->is_apprentice_of(m), 1);
    delete a;
    delete m;
}

// ---------------- create_family / assign_apprentice / set_title
// ----------------
TEST_F(CharFamilyTest, CreateFamilyAndTitle)
{
    CChar* m = new CChar();
    m->set("id", "z1");
    char name[]  = "武当派";
    char title[] = "掌门";
    m->create_family(name, 1, title);
    EXPECT_STREQ(m->querystr("family/family_name"), "武当派");
    EXPECT_NE(m->querystr("title")[0], '\0');
    delete m;
}

TEST_F(CharFamilyTest, AssignApprentice_NoFamily)
{
    CChar* m = new CChar();
    m->assign_apprentice("弟子", 0);  // no family -> early return
    EXPECT_EQ(m->querymap("family"), nullptr);
    delete m;
}

// ---------------- recruit_apprentice ----------------
TEST_F(CharFamilyTest, RecruitApprentice)
{
    CChar* m = MakeMaster();
    CUser* a = new CUser();
    a->set("id", "a2");
    a->set("name", "弟子");
    EXPECT_EQ(m->recruit_apprentice(a), 1);
    EXPECT_STREQ(a->querystr("family/master_id"), "m1");
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, RecruitApprentice_AlreadyApprentice)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    a->set("id", "a3");
    CMapping fam;
    fam.set("family_name", "少林派");
    fam.set("master_id", "m1");
    a->set("family", fam);
    EXPECT_EQ(m->recruit_apprentice(a), 0);
    delete a;
    delete m;
}

// ---------------- prevent_learn ----------------
TEST_F(CharFamilyTest, PreventLearn_NotDirectDisciple)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    a->set("id", "a4");
    // a not apprentice of m, and m skill <= a skill*3 -> refuse (return 1)
    a->set_skill("unarmed", 100);
    EXPECT_EQ(m->prevent_learn(a, "unarmed"), 1);
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, PreventLearn_DirectDiscipleOk)
{
    CChar* m = MakeMaster();
    m->set_skill("unarmed", 100);
    CChar* a = new CChar();
    a->set("id", "a5");
    CMapping fam;
    fam.set("family_name", "少林派");
    fam.set("master_id", "m1");
    a->set("family", fam);
    // is apprentice -> first refuse branch skipped; second requires !apprentice
    EXPECT_EQ(m->prevent_learn(a, "unarmed"), 0);
    delete a;
    delete m;
}

// 背叛师门校验关闭时（no_betrayer_valid=0）：按技能差放行
TEST_F(CharFamilyTest, PreventLearn_BetrayerPassesWhenSkillRuleAllows)
{
    CChar* m = MakeMaster();  // unarmed 100
    CChar* a = new CChar();
    a->set("id", "b1");
    a->set("betrayer", (long)1);
    // a.unarmed(0) < 100 - 1*20 -> 不命中背叛拒绝，且未到非嫡传拒绝条件
    EXPECT_EQ(m->prevent_learn(a, "unarmed"), 0);
    delete a;
    delete m;
}

// 背叛师门校验开启时（no_betrayer_valid=1）：一律拒绝，不再看技能差
TEST_F(CharFamilyTest, PreventLearn_BetrayerAlwaysRefusedWhenCheckOn)
{
    ini_load("tests/test_config.ini");
    g_ini_data["server"]["no_betrayer_valid"] = "1";
    CServerConfig::init();

    CChar* m = MakeMaster();
    CChar* a = new CChar();
    a->set("id", "b2");
    a->set("betrayer", (long)1);
    EXPECT_EQ(m->prevent_learn(a, "unarmed"), 1);
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, RecognizeApprentice)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    EXPECT_EQ(m->recognize_apprentice(a), 0);
    delete a;
    delete m;
}

// ---------------- leader / follow ----------------
TEST_F(CharFamilyTest, LeaderGetSet)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    m->set_leader(a);
    EXPECT_EQ(m->query_leader(), a);
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, FollowMe_NotLiving)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    // m is living by default; a==this check
    char dir[] = "north";
    EXPECT_EQ(m->follow_me(m, dir), 0);  // me == this -> 0
    delete a;
    delete m;
}

TEST_F(CharFamilyTest, RelayEmptyStubs)
{
    CChar* m = MakeMaster();
    CChar* a = new CChar();
    m->relay_emote(a, "smile");
    char msg[] = "hi";
    m->relay_say(a, msg);
    m->attempt_apprentice(a);
    m->finish_kill(a);
    m->send_off();
    delete a;
    delete m;
}

// ---------------- release_master / recruit_num ----------------
TEST_F(CharFamilyTest, ReleaseMaster_NotPlayer)
{
    CChar* a = new CChar();
    a->set("id", "a6");
    a->release_master();  // family/user not set -> early return
    EXPECT_EQ(a->query("family/user"), 0);
    delete a;
}

TEST_F(CharFamilyTest, RecruitNum_Empty)
{
    CChar* m = MakeMaster();
    EXPECT_EQ(m->recruit_num(), 0);
    delete m;
}
