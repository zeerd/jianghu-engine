// test_sql_cards.cpp — SqlCards.cpp 卡系统 DB 层单元测试
//
// 真实 SqlCards.cpp 链接进测试二进制，g_db 指向 MockDatabase：
// 断言各操作在 DB 边界发出的 SQL、回给玩家的 notify_fail 文案，
// 以及 DB 异常不穿透到游戏逻辑。
#include "pch.h"

#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <stdexcept>
#include <string>

#include "Char.h"
#include "Global.h"
#include "IDatabase.h"
#include "mock/mock_database.h"
#include "Sql.h"

extern char g_notify_fail_msg[512];
bool card_init(void);

namespace {

constexpr char kCard[]   = "C001";
constexpr char kPlayer[] = "7";

std::string FutureStamp()
{
    return std::to_string(::time(nullptr) + 30L * 24 * 3600);
}

class SqlCardsTest : public ::testing::Test {
protected:
    MockDatabase db;

    void SetUp() override
    {
        g_notify_fail_msg[0] = '\0';
        g_db                 = &db;
    }

    void TearDown() override { g_db = nullptr; }

    static DbResultSet Row(std::initializer_list<std::string> cols)
    {
        return MockDatabase::MakeSingleRow(cols);
    }
};

class SqlCardsThrowTest : public ::testing::Test {
protected:
    ThrowingDatabase db;

    void SetUp() override
    {
        db.Reset();
        g_db = &db;
    }

    void TearDown() override { g_db = nullptr; }
};

}  // namespace

// ---------------------------------------------------------------- card_init

TEST_F(SqlCardsTest, Init_CreatesCardTableOnDemand)
{
    EXPECT_TRUE(card_init());
    EXPECT_TRUE(db.ExecutedContaining("CREATE TABLE IF NOT EXISTS `card_jx`"));
}

TEST_F(SqlCardsTest, Init_DbError_ReturnsFalse)
{
    db.SetExecuteErrorCode(1);
    EXPECT_FALSE(card_init());
}

TEST_F(SqlCardsThrowTest, Init_SwallowsDbException)
{
    db.throwOnExecute = true;
    EXPECT_FALSE(card_init());
}

// ------------------------------------------------------------ register_card

TEST_F(SqlCardsTest, Register_DbFailurePaths_DoNotWrite)
{
    db.SetQueryErrorCode(1);
    EXPECT_FALSE(register_card(kCard, kPlayer));
    EXPECT_FALSE(db.ExecutedContaining("update card_jx"));

    db.SetQueryErrorCode(0);
    EXPECT_FALSE(register_card(kCard, kPlayer));  // 查无记录
    EXPECT_FALSE(db.ExecutedContaining("update card_jx"));
}

TEST_F(SqlCardsTest, Register_RowWithoutColumns_TellsPlayerCardUnavailable)
{
    db.SetQueryResultMatching("WHERE card = '", Row({}));
    EXPECT_FALSE(register_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "非可用卡号！");
}

TEST_F(SqlCardsTest, Register_FreshCard_WritesActivationWindow)
{
    db.SetQueryResultMatching("WHERE card = '", Row({"0", "0"}));

    EXPECT_TRUE(register_card(kCard, kPlayer));
    EXPECT_TRUE(db.ExecutedContaining("update card_jx set activetime"));
    EXPECT_TRUE(db.ExecutedContaining("where card = 'C001'"));
}

TEST_F(SqlCardsTest, Register_UnusedCard_ContinuesLastCardTimeline)
{
    const std::string lastEnd = FutureStamp();
    db.SetQueryResultMatching("WHERE card = '", Row({"0", "0"}));
    db.SetQueryResultMatching("FROM card_jx WHERE id = '", Row({lastEnd}));

    EXPECT_TRUE(register_card(kCard, kPlayer));
    EXPECT_TRUE(db.ExecutedContaining("begintime = '" + lastEnd + "'"));
}

TEST_F(SqlCardsTest, Register_AlreadyRegistered_TellsPlayer)
{
    db.SetQueryResultMatching("WHERE card = '", Row({FutureStamp(), "1"}));

    EXPECT_FALSE(register_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "此卡已经被注册过了。");
    EXPECT_FALSE(db.ExecutedContaining("update card_jx"));
}

TEST_F(SqlCardsTest, Register_UpdateFailure_TellsPlayer)
{
    db.SetQueryResultMatching("WHERE card = '", Row({"0", "0"}));
    db.SetExecuteErrorCode(1);

    EXPECT_FALSE(register_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "卡注册失败。");
}

// --------------------------------------------------------------- verify_id

TEST_F(SqlCardsTest, Verify_ActiveCard_ReturnsTrueAndLeavesBufferUntouched)
{
    char endtime_s[65] = "untouched";
    db.SetQueryResultMatching("FROM card_jx WHERE id = '",
                              Row({FutureStamp()}));

    EXPECT_TRUE(verify_id(kPlayer, endtime_s));
    EXPECT_STREQ(endtime_s, "untouched");
}

TEST_F(SqlCardsTest, Verify_ExpiredCard_FillsEndtimeAndTellsPlayer)
{
    char endtime_s[65] = "untouched";
    db.SetQueryResultMatching("FROM card_jx WHERE id = '", Row({"1000"}));

    EXPECT_FALSE(verify_id(kPlayer, endtime_s));
    EXPECT_STREQ(g_notify_fail_msg, "游戏卡到期。");

    // 到期时间回填为可读时间戳（epoch+1000 落在 1970/01/01）
    EXPECT_STRNE(endtime_s, "untouched");
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    EXPECT_EQ(std::sscanf(endtime_s, "%d/%d/%d %d:%d:%d", &year, &month, &day,
                          &hour, &minute, &second),
              6);
    EXPECT_EQ(year, 1970);
    EXPECT_EQ(month, 1);
    EXPECT_EQ(day, 1);
}

TEST_F(SqlCardsTest, Verify_NoBoundCard_TellsPlayer)
{
    char endtime_s[65] = "";
    db.SetQueryResultMatching("FROM card_jx WHERE id = '", Row({"0"}));

    EXPECT_FALSE(verify_id(kPlayer, endtime_s));
    EXPECT_STREQ(g_notify_fail_msg, "该账号没有绑定任何游戏卡3。");
}

TEST_F(SqlCardsTest, Verify_RowWithoutColumns_TellsPlayer)
{
    char endtime_s[65] = "";
    db.SetQueryResultMatching("FROM card_jx WHERE id = '", Row({}));

    EXPECT_FALSE(verify_id(kPlayer, endtime_s));
    EXPECT_STREQ(g_notify_fail_msg, "该账号没有绑定任何游戏卡2。");
}

TEST_F(SqlCardsTest, Verify_DbFailurePaths_ReturnFalse)
{
    char endtime_s[65] = "";

    db.SetQueryErrorCode(1);
    EXPECT_FALSE(verify_id(kPlayer, endtime_s));

    db.SetQueryErrorCode(0);
    EXPECT_FALSE(verify_id(kPlayer, endtime_s));  // 查无记录

    EXPECT_EQ(g_notify_fail_msg[0], '\0');
}

// --------------------------------------------------------- unregister_card

TEST_F(SqlCardsTest, Unregister_DbFailurePaths_ReturnFalse)
{
    db.SetQueryErrorCode(1);
    EXPECT_FALSE(unregister_card(kCard, kPlayer));

    db.SetQueryErrorCode(0);
    EXPECT_FALSE(unregister_card(kCard, kPlayer));  // 查无记录

    EXPECT_EQ(g_notify_fail_msg[0], '\0');
}

TEST_F(SqlCardsTest, Unregister_RowWithoutColumns_TellsPlayer)
{
    db.SetQueryResultMatching("ORDER BY endtime DESC", Row({}));
    EXPECT_FALSE(unregister_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "该账号没有绑定任何游戏卡2。");
}

TEST_F(SqlCardsTest, Unregister_WrongCard_TellsPlayer)
{
    db.SetQueryResultMatching("ORDER BY endtime DESC",
                              Row({"OTHER", "1", "2"}));

    EXPECT_FALSE(unregister_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "只能移除时间线上的最后一张卡。");
}

TEST_F(SqlCardsTest, Unregister_ClampsBegintimeToActivetime)
{
    db.SetQueryResultMatching("ORDER BY endtime DESC",
                              Row({kCard, "5000", "3000"}));

    EXPECT_TRUE(unregister_card(kCard, kPlayer));
    EXPECT_TRUE(db.ExecutedContaining(
        "update card_jx set endtime = 0, begintime=3000"));
    EXPECT_TRUE(db.ExecutedContaining("where card = 'C001'"));
}

TEST_F(SqlCardsTest, Unregister_UpdateFailure_TellsPlayer)
{
    db.SetQueryResultMatching("ORDER BY endtime DESC",
                              Row({kCard, "5000", "3000"}));
    db.SetExecuteErrorCode(1);

    EXPECT_FALSE(unregister_card(kCard, kPlayer));
    EXPECT_STREQ(g_notify_fail_msg, "卡注册失败。");
}

TEST_F(SqlCardsTest, Unregister_WithoutActivetime_SucceedsWithoutUpdate)
{
    db.SetQueryResultMatching("ORDER BY endtime DESC",
                              Row({kCard, "5000", ""}));

    EXPECT_TRUE(unregister_card(kCard, kPlayer));
    EXPECT_FALSE(db.ExecutedContaining("update card_jx"));
}

// ------------------------------------------------------------- create_card

TEST_F(SqlCardsTest, Create_InsertsNewCard)
{
    CChar me;
    me.set("id", kPlayer);

    EXPECT_TRUE(create_card(&me));
    EXPECT_TRUE(db.ExecutedContaining("INSERT INTO `card_jx`"));
}

TEST_F(SqlCardsTest, Create_DbFailure_ReturnsFalse)
{
    CChar me;
    me.set("id", kPlayer);
    db.SetExecuteErrorCode(1);

    EXPECT_FALSE(create_card(&me));
    EXPECT_TRUE(db.ExecutedContaining("INSERT INTO `card_jx`"));
}

// -------------------------------------------------------------- show_cards

TEST_F(SqlCardsTest, ShowCards_NoRows_TellsPlayer)
{
    CChar me;
    me.set("id", kPlayer);

    EXPECT_FALSE(show_cards(&me));
    EXPECT_STREQ(g_notify_fail_msg, "该账号没有绑定任何游戏卡。");
}

TEST_F(SqlCardsTest, ShowCards_ListsCards)
{
    CChar me;
    me.set("id", kPlayer);
    db.SetQueryResultMatching("SELECT card, begintime",
                              Row({kCard, "1000000000", "2000000000"}));

    EXPECT_TRUE(show_cards(&me));
}

TEST_F(SqlCardsTest, ShowCards_QueryFailure_TellsPlayer)
{
    CChar me;
    me.set("id", kPlayer);
    db.SetQueryErrorCode(1);

    EXPECT_FALSE(show_cards(&me));
    EXPECT_STREQ(g_notify_fail_msg, "该账号没有绑定任何游戏卡。");
}

// ------------------------------------------------ DB 异常不穿透到业务层

TEST_F(SqlCardsThrowTest, QueryFunctions_SwallowDbExceptions)
{
    db.throwOnQuery = true;

    EXPECT_FALSE(register_card(kCard, kPlayer));

    char endtime_s[65] = "";
    EXPECT_FALSE(verify_id(kPlayer, endtime_s));
    EXPECT_FALSE(unregister_card(kCard, kPlayer));

    CChar me;
    me.set("id", kPlayer);
    EXPECT_FALSE(show_cards(&me));
}

TEST_F(SqlCardsThrowTest, CreateCard_SwallowsDbException)
{
    CChar me;
    me.set("id", kPlayer);

    db.throwOnExecute     = true;
    db.executeBeforeThrow = 1;  // card_init 的建表语句正常，发卡语句抛异常

    EXPECT_FALSE(create_card(&me));
}
