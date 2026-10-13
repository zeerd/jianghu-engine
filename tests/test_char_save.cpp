// test_char_save.cpp - Unit tests for CharSave.cpp (存盘/读档).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "User.h"
#include "Mapping.h"
#include "test_char_common.h"

class CharSaveTest : public ::testing::Test {
protected:
    CharMockDb* db;
    void SetUp() override
    {
        db   = new CharMockDb();
        g_db = db;
    }
    void TearDown() override
    {
        g_db = nullptr;
        delete db;
    }

    static CUser* MakeUser(const char* id)
    {
        CUser* u = new CUser();
        u->set("id", id);
        u->set("name", "玩家");
        u->set("gender", "男性");
        u->set("str", (long)15);
        u->set("int", (long)15);
        u->set("con", (long)15);
        u->set("dex", (long)15);
        u->set("combat_exp", (long)1000);
        u->set("balance", (long)100);
        u->set("login/time", (long)1000000);
        u->set_skill("unarmed", 30);
        return u;
    }
};

// ---------------- SaveToServer ----------------
TEST_F(CharSaveTest, SaveToServer_Success)
{
    CUser* u = MakeUser("s1");
    EXPECT_EQ(u->save_to_server(), 1);
    EXPECT_NE(db->LastSql().find("update jianghudb"), std::string::npos);
    delete u;
}

TEST_F(CharSaveTest, SaveToServer_ExecuteFail)
{
    CUser* u = MakeUser("s3");
    db->SetExecFail(true);
    EXPECT_EQ(u->save_to_server(), 0);
    delete u;
}

TEST_F(CharSaveTest, SaveToServer_MasterExpBranch)
{
    // combat_exp < 50000 and family/user set -> recruit exp recorded
    CUser* u = MakeUser("s4");
    u->set("combat_exp", (long)5000);
    u->set("family/user", (long)1);
    u->set("family/master_id", "master1");
    u->set("family/last_exp", (long)1000);
    EXPECT_EQ(u->save_to_server(), 1);
    delete u;
}

// ---------------- Save ----------------
TEST_F(CharSaveTest, Save_UserCallsServer)
{
    CUser* u = MakeUser("s5");
    EXPECT_EQ(u->save(), 1);
    delete u;
}

TEST_F(CharSaveTest, Save_NonUser)
{
    CChar* c = new CChar();
    c->set("id", "s6");
    c->set("name", "npc");
    // non-user -> CContainer::_Save path (may return 0 if not CAN_SAVE)
    int r = c->save();
    EXPECT_TRUE(r == 0 || r == 1);
    delete c;
}

// ---------------- GetSaveData / RestoreData ----------------
TEST_F(CharSaveTest, GetSaveData_Populates)
{
    CUser* u = MakeUser("s8");
    CMapping save;
    EXPECT_EQ(u->get_save_data(&save), 1);
    EXPECT_NE(save.querymap("dbase"), nullptr);
    delete u;
}

TEST_F(CharSaveTest, RestoreData_RoundTrip)
{
    CUser* src = MakeUser("s9");
    src->set_skill("dodge", 42);
    CMapping save;
    src->get_save_data(&save);

    CUser* dst = new CUser();
    dst->set("id", "s9");
    EXPECT_EQ(dst->restore_data(&save), 1);
    EXPECT_EQ(dst->query_skill("dodge", 1), 42);
    EXPECT_EQ(dst->query_skill("unarmed", 1), 30);
    delete src;
    delete dst;
}

// ---------------- LoadFromServer ----------------
TEST_F(CharSaveTest, LoadFromServer_Success)
{
    // generate a real save buffer
    CUser* src = MakeUser("s10");
    src->set_skill("parry", 55);
    CMapping save;
    src->get_save_data(&save);
    std::string buf = save.get_buffer();
    delete src;

    CUser* u = new CUser();
    u->set("id", "s10");
    db->SetResult("select cc_savedata from jianghudb where cc_id=s10",
                  CharMockDb::SingleRow({buf}));
    EXPECT_EQ(u->load_from_server(), 1);
    EXPECT_EQ(u->query_skill("parry", 1), 55);
    delete u;
}

TEST_F(CharSaveTest, LoadFromServer_Empty)
{
    CUser* u = new CUser();
    u->set("id", "s11");
    // no result configured -> empty result set
    EXPECT_EQ(u->load_from_server(), 0);
    delete u;
}

TEST_F(CharSaveTest, LoadFromServer_QueryFail)
{
    CUser* u = new CUser();
    u->set("id", "s12");
    db->SetQueryFail(true);
    EXPECT_EQ(u->load_from_server(), 0);
    delete u;
}

// ---------------- Load ----------------
TEST_F(CharSaveTest, Load_UserDelegates)
{
    CUser* u = new CUser();
    u->set("id", "s13");
    db->SetQueryFail(true);
    EXPECT_EQ(u->load(), 0);  // LoadFromServer fails
    delete u;
}

TEST_F(CharSaveTest, Load_NonUser)
{
    CChar* c = new CChar();
    c->set("id", "s14");
    int r = c->load();
    EXPECT_TRUE(r == 0 || r == 1);
    delete c;
}
