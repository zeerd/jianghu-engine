// test_cmd_user_card.cpp - Unit tests for CmdUserCard.cpp
//
// Tests card system game state without DB operations.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "CmdUser.h"
#include "GameInit.h"             // g_start_room
#include "test_char_common.h"     // CharMockDb
#include "mock/mock_database.h"   // 结果行脚本（register/unregister 成功支）
#include "test_scoped_room.h"     // g_room_list 兜底清理
#include "test_state_recorder.h"  // tell 回执观察面

class CmdUserCardTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallCardHelp(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"card", me, target, arg);
    }
    static int CallAddCard(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"addcard", me, target, arg);
    }
    static int CallRemoveCard(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"removecard", me, target, arg);
    }
    static int CallShowCards(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"showcards", me, target, arg);
    }

    CChar* me;
    CContainer* room;

    void SetUp() override
    {
        room = new CContainer();
        room->set("name", "test_room");
        room->set("base_name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->move(room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }
};

TEST_F(CmdUserCardTest, CardHelp_ShowsHelp)
{
    int result = CallCardHelp(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(CmdUserCardTest, AddCard_NoDb)
{
    // mock 空查询 → register_card 判失败 → 命令返回 0，但必须真的触达 db
    CharMockDb db;
    g_db       = &db;
    int result = CallAddCard(me, 0, (char*)"CARD0001");
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(db.LastSql().empty());
    g_db = nullptr;
}

TEST_F(CmdUserCardTest, RemoveCard_NoDb)
{
    // mock 空查询 → unregister_card 早退 0 → 命令返回 0，但必须真的触达 db
    CharMockDb db;
    g_db       = &db;
    int result = CallRemoveCard(me, 0, (char*)"CARD0001");
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(db.LastSql().empty());
    g_db = nullptr;
}

TEST_F(CmdUserCardTest, ShowCards_NoDb)
{
    // 成员无条件 return 1（内部查询失败被吞）；db 层被真实调用
    CharMockDb db;
    g_db       = &db;
    int result = CallShowCards(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_FALSE(db.LastSql().empty());
    g_db = nullptr;
}

TEST_F(CmdUserCardTest, PlayerHasId) { EXPECT_STREQ(me->id(1), "tester1"); }

TEST_F(CmdUserCardTest, PlayerHasName) { EXPECT_STREQ(me->name(1), "测试者"); }

// ====================================================================
// 成功支与巫师权限支（此前用例全部落 DB 失败/无参守卫）
// ====================================================================

TEST_F(CmdUserCardTest, CardHelp_WizardSeesCreateHint)
{
    auto* wiz = new StateRecordingUser();
    wiz->set_name("巫师", "card_wiz");
    wiz->set("grant", (long)CChar::GRANT_WIZARD);
    wiz->move(room);

    EXPECT_EQ(1, CallCardHelp(wiz, 0, (char*)""));
    EXPECT_TRUE(wiz->has("createcard")) << "巫师才见建卡提示(9-10)";

    wiz->move(nullptr);
    delete wiz;
}

TEST_F(CmdUserCardTest, AddCard_Success_RegistersAndLeavesRk)
{
    MockDatabase db;
    g_db = &db;
    // 卡未被注册过：endtime=0、begintime 已有值 → 走 UPDATE 激活支
    db.SetQueryResultMatching("WHERE card =",
                              MockDatabase::MakeSingleRow({"0", "1700000000"}));
    ScopedRoom start("card_start");
    ASSERT_NE(start.get(), nullptr);

    auto* rk = new CContainer();  // 新手村：base_name=rk → 注册后回起始房
    rk->set("name", "新手村");
    rk->set("base_name", "rk");
    auto* wiz = new StateRecordingUser();
    wiz->set_name("玩家", "tester1");
    wiz->set("id", "tester1");
    wiz->set("hp", (long)100);
    wiz->move(rk);

    strncpy(g_start_room, "card_start", sizeof(g_start_room) - 1);
    EXPECT_EQ(1, CallAddCard(wiz, 0, (char*)"CARDX1")) << "注册成功路(24-30)";
    EXPECT_TRUE(wiz->has("注册成功！")) << "回执(24)";
    EXPECT_EQ(start.get(), wiz->environment()) << "rk 注册后迁回起始房(26-27)";

    wiz->move(nullptr);
    delete wiz;
    rk->move(nullptr);
    delete rk;
    g_db = nullptr;
}

TEST_F(CmdUserCardTest, RemoveCard_Success_NotifiesExpiryAndReturnsRk)
{
    MockDatabase db;
    g_db = &db;
    // 名下唯一卡解绑：card 与实参一致、activetime 非 0 → UPDATE 后返回真
    db.SetQueryResultMatching(
        "SELECT card,IFNULL",
        MockDatabase::MakeSingleRow({"CARD9", "1700000000", "1700000000"}));
    ScopedRoom rk("rk");  // verify 失败 → 移回 rk，预建兜底
    ASSERT_NE(rk.get(), nullptr);

    auto* wiz = new StateRecordingUser();
    wiz->set_name("玩家", "tester1");
    wiz->set("id", "tester1");
    wiz->set("hp", (long)100);
    wiz->move(room);

    EXPECT_EQ(1, CallRemoveCard(wiz, 0, (char*)"CARD9")) << "移除成功路(39)";
    EXPECT_TRUE(wiz->has("移除成功！")) << "回执(39)";
    EXPECT_TRUE(wiz->has("到期")) << "解绑后 verify 无活跃卡 → 到期回执(42-44)";
    EXPECT_EQ(rk.get(), wiz->environment()) << "无卡者移回 rk(47)";

    wiz->move(nullptr);
    delete wiz;
    g_db = nullptr;
}
