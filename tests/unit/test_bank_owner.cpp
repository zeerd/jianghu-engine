// test_bank_owner.cpp - Unit tests for BankOwner.cpp
//
// Tests CBankOwner::Do_Deposit, Do_Withdraw, Do_Loan, Do_Balance, Query.
//
#include "pch.h"
#include "Char.h"
#include "Npc.h"
#include "BankOwner.h"
#include "Mapping.h"
#include "Money.h"      // CMCoin / CMoney（存款取现的现款）
#include "HeartBeat.h"  // g_destruct_list 快照回收
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"  // 存单/播报的观察面

extern char g_notify_fail_msg[512];

namespace {
// 现款工厂：经 g_item_mgr->load_item("coin") 供货（pay_player 的入口）
std::vector<CContainer*> g_bank_coins;

CContainer* BankCoinFactory(const char* base_name)
{
    auto* c = new CMCoin();
    c->create();  // 出厂值：base_value=1、amount=1
    c->set("base_name", base_name);
    g_bank_coins.push_back(c);
    return c;
}
}  // namespace

class BankOwnerTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallBalance(CBankOwner* bank, CChar* me)
    {
        return bank->do_balance(me);
    }

    CBankOwner* bank;
    CChar* player;
    CContainer* room = nullptr;
    CMockItemMgr mgr;
    IItemMgr* saved_mgr = nullptr;
    std::vector<CContainer*> m_live_at_setup;  // 延迟析构快照

    void SetUp() override
    {
        ensure_lua_env();
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        // pay_player 经 g_item_mgr 造币 → 注入现款工厂，收尾归还共享管理器
        saved_mgr        = g_item_mgr;
        mgr.load_factory = &BankCoinFactory;
        g_item_mgr       = &mgr;

        bank = new CBankOwner();
        bank->set("name", "钱庄掌柜");
        bank->set("id", "banker1");

        player = new CChar();
        player->set("name", "玩家");
        player->set("id", "player1");
        player->set("level", (long)100);
        player->set("hp", (long)1000);
        player->set("combat_exp", (long)5000000);
        player->set("balance", (long)0);

        room = new CContainer();
        room->set("name", "bank_room");
        player->move(room);
    }

    void TearDown() override
    {
        // 1) 存活币先出列表（被毁灭的币 env 已空，跳过）
        for (auto* c : g_bank_coins) {
            if (c->environment()) {
                c->set_heart_beat(0);
                c->move(nullptr);
            }
        }
        player->move(nullptr);
        room->move(nullptr);
        delete bank;
        delete player;
        delete room;

        // 2) 延迟析构：本用例扣到 0 自毁的币统一回收
        std::vector<CContainer*> reclaimed;
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
                reclaimed.push_back(ob);
                delete ob;
            }
        }

        // 3) 未被回收的存活币
        for (auto* c : g_bank_coins) {
            bool done = false;
            for (auto* r : reclaimed) {
                if (r == c) done = true;
            }
            if (!done) delete c;
        }
        g_bank_coins.clear();

        g_item_mgr = saved_mgr;
    }
};

TEST_F(BankOwnerTest, Query_IS_BANKER)
{
    EXPECT_EQ(bank->query_attr(CNpc::IS_BANKER), 1);
}

TEST_F(BankOwnerTest, Query_OtherAttr_Delegates)
{
    // Non-banker attr should delegate to CNpc::Query
    EXPECT_EQ(bank->query_attr(CNpc::IS_NPC), 1);
}

TEST_F(BankOwnerTest, Do_Deposit_Zero_ReturnsZero)
{
    EXPECT_EQ(bank->do_deposit(player, 0), 0);
}

TEST_F(BankOwnerTest, Do_Deposit_Negative_ReturnsZero)
{
    EXPECT_EQ(bank->do_deposit(player, -100), 0);
}

TEST_F(BankOwnerTest, Do_Deposit_Positive_IncreasesBalance)
{
    // Player has no money, so Player_Pay fails → returns 0 (notify_fail)
    // This tests the path where payment fails
    int result = bank->do_deposit(player, 100);
    // Without money in inventory, payment fails
    EXPECT_EQ(result, 0);
}

TEST_F(BankOwnerTest, Do_Withdraw_Zero_ReturnsZero)
{
    EXPECT_EQ(bank->do_withdraw(player, 0), 0);
}

TEST_F(BankOwnerTest, Do_Withdraw_Negative_ReturnsZero)
{
    EXPECT_EQ(bank->do_withdraw(player, -100), 0);
}

TEST_F(BankOwnerTest, Do_Withdraw_OverBalance_ReturnsFail)
{
    // Player has 0 balance, trying to withdraw 100
    int result = bank->do_withdraw(player, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(BankOwnerTest, Do_Loan_LowLevel_ReturnsFail)
{
    player->set("level", (long)50);  // below 80
    int result = bank->do_loan(player, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(BankOwnerTest, Do_Loan_HighLevel_ReturnsFail)
{
    player->set("level", (long)180);  // above 170
    int result = bank->do_loan(player, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(BankOwnerTest, Do_Loan_Zero_ReturnsZero)
{
    player->set("level", (long)100);
    EXPECT_EQ(bank->do_loan(player, 0), 0);
}

TEST_F(BankOwnerTest, HandleAction_Balance_Zero)
{
    player->set("balance", (long)0);
    int result = bank->handle_action((char*)"balance", player, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(BankOwnerTest, HandleAction_Balance_Positive)
{
    player->set("balance", (long)5000);
    int result = bank->handle_action((char*)"balance", player, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(BankOwnerTest, HandleAction_Balance_Negative)
{
    player->set("balance", (long)-3000);
    int result = bank->handle_action((char*)"balance", player, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(BankOwnerTest, HandleAction_Deposit)
{
    // handle_action with "deposit" command
    int result = bank->handle_action((char*)"deposit", player, (char*)"100");
    // Player has no money, so deposit fails
    EXPECT_EQ(result, 0);
}

TEST_F(BankOwnerTest, HandleAction_Withdraw)
{
    int result = bank->handle_action((char*)"withdraw", player, (char*)"100");
    EXPECT_EQ(result, 0);  // no balance
}

TEST_F(BankOwnerTest, HandleAction_Loan)
{
    player->set("level", (long)50);
    int result = bank->handle_action((char*)"loan", player, (char*)"100");
    EXPECT_EQ(result, 0);  // level too low
}

TEST_F(BankOwnerTest, HandleAction_Balance)
{
    int result = bank->handle_action((char*)"balance", player, (char*)"");
    EXPECT_EQ(result, 1);
}

TEST_F(BankOwnerTest, HandleAction_Unknown)
{
    int result = bank->handle_action((char*)"unknown", player, (char*)"");
    EXPECT_EQ(result, 0);
}

// ====================================================================
// 成功路与溢出门：存款入账 / 取现付现 / 贷款欠账 / 三道数值闸
// （此前用例全部落在守卫支，成功支与溢出支整段未覆盖）
// ====================================================================

TEST_F(BankOwnerTest, Do_Deposit_Positive_CreditsBalanceAndAnnounces)
{
    ASSERT_EQ(1, CMoney::pay_player(player, 200)) << "先备现款(经现款工厂)";

    StateRecordingUser* ear = new StateRecordingUser();
    ear->set_name("旁听", "bank_ear");
    ear->set("hp", (long)100);
    ear->move(room);
    ear->delete_temp("client/loading");

    EXPECT_EQ(1, bank->do_deposit(player, 100));
    EXPECT_EQ(100, player->query("balance")) << "扣现款入账(32-33)";
    EXPECT_TRUE(ear->has("存入现款")) << "存单对房广播(34-35)";

    ear->move(nullptr);
    delete ear;
}

TEST_F(BankOwnerTest, Do_Deposit_NearLongMax_GuardRejects)
{
    const long near_cap = 2147483647L - 5;  // Win32 LONGMAX 逼近值
    player->set("balance", near_cap);
    ASSERT_EQ(1, CMoney::pay_player(player, 20));

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, bank->do_deposit(player, 10)) << "余额将满 → 拒存(28-29)";
    EXPECT_EQ(near_cap, player->query("balance")) << "被拒不动账";
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("没办法帮你保存"));
}

TEST_F(BankOwnerTest, Do_Withdraw_Positive_PaysCashAndReducesBalance)
{
    player->set("balance", (long)1000);

    EXPECT_EQ(1, bank->do_withdraw(player, 100));
    EXPECT_EQ(900, player->query("balance")) << "扣账(54-55)";
    CContainer* coin = player->present_name("coin", CChar::IS_MONEY);
    ASSERT_NE(nullptr, coin) << "现款到手(57)";
    EXPECT_EQ(100, coin->query_amount());
}

TEST_F(BankOwnerTest, Do_Loan_Success_CreditsDebtAndPaysCash)
{
    // 夹具 level=100 ∈ [80,170]，余额 0 → 贷 10 成功
    EXPECT_EQ(1, bank->do_loan(player, 10));
    EXPECT_EQ(-10, player->query("balance")) << "欠账入册(88-90)";
    CContainer* coin = player->present_name("coin", CChar::IS_MONEY);
    ASSERT_NE(nullptr, coin) << "贷款到账(92)";
    EXPECT_EQ(10, coin->query_amount());
}

TEST_F(BankOwnerTest, Do_Loan_ExceedsStanding_GateRejects)
{
    player->set("level", (long)80);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, bank->do_loan(player, 2147483647))
        << "贷额超出江湖地位对应上限 → 拒(85-86)";
    EXPECT_EQ(0, player->query("balance")) << "未动账";
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你借不到这么多钱"));
}

TEST_F(BankOwnerTest, Do_Loan_WithExistingDebt_OverflowGuardRejects)
{
    player->set("balance", (long)-10);  // 既有欠款 → hamount<0
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, bank->do_loan(player, 2147483647))
        << "欠款叠加将溢出 long → 拒(83-84)";
    EXPECT_EQ(-10, player->query("balance")) << "被拒不动账";
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("别想刷钱"));
}
