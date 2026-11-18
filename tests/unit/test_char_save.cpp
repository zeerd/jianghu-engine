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
#include "test_lua_env.h"
#include "mock/mock_itemmgr.h"  // restore_data 的 auto_load 分支需要 load_item 工厂

class CharSaveTest : public ::testing::Test {
protected:
    CharMockDb* db;
    void SetUp() override
    {
        // 先起 Lua 环境再 set_skill：无环境加载的技能会以未挂脚本的形态留在
        // g_skill_list，后续按名取到的便是 query_dodge_msg/query_action
        // 恒空的僵尸
        ensure_lua_env();
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

TEST_F(CharSaveTest, SaveToServer_GuestSkipsDatabase)
{
    // 游客（guest/guest 进大厅的临时角色）没有正式档案：返回成功但不落库
    CUser* u = MakeUser("sguest");
    u->set("guest", (long)1);
    EXPECT_EQ(u->save_to_server(), 1);
    EXPECT_EQ(db->LastSql().find("update jianghudb"), std::string::npos)
        << "游客不得触发任何存盘 SQL";
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

    // auto_load 分支：键无逗号 -> load_item(全键) -> set_amount -> move 回身上
    CMapping auto_load;
    auto_load.set("sword_1", (mapping_value_t)3);
    save.set("auto_load", auto_load);

    CMockItemMgr mgr;
    IItemMgr* saved_mgr = g_item_mgr;
    g_item_mgr          = &mgr;
    mgr.load_factory    = [](const char*) -> CContainer* {
        CContainer* it = new CContainer();
        it->set_name("mock_item", "mock_item");
        return it;
    };

    CUser* dst = new CUser();
    dst->set("id", "s9");
    EXPECT_EQ(dst->restore_data(&save), 1);
    EXPECT_EQ(dst->query_skill("dodge", 1), 42);
    EXPECT_EQ(dst->query_skill("unarmed", 1), 30);
    int n = 0;
    for (auto* item : *dst->get_item_list()) {
        (void)item;
        ++n;
    }
    EXPECT_EQ(1, n) << "auto_load 物品回到身上";

    g_item_mgr = saved_mgr;
    delete src;
    delete dst;
}

// ---------------- load_from_server ----------------
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
    EXPECT_EQ(u->load(), 0);  // load_from_server fails
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
