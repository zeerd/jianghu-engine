// test_cmd_user_vendor.cpp - Unit tests for CmdUserVendor.cpp
//
// Tests CCmdUser::ListGoods, Buy.
//
// 摆摊/购买是纯房间交互：货物以 trade/value + trade/owner 温度标记，
// 买方付现（CMoney::player_pay）后货物过户、卖方收款（pay_player 经
// g_item_mgr 取钱——夹具注入 CMockItemMgr，测试进程默认 g_item_mgr
// 为空指针）。买方用录制用户接菜单与私聊文案。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>
#include <vector>

#include "Char.h"
#include "CmdUser.h"
#include "Item.h"
#include "Money.h"
#include "Room.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {

// 工厂造出的钱：登记后由夹具统一回收（可能已被合并逻辑自毁）
std::vector<CContainer*> g_created_coins;

CContainer* vendor_item_factory(const char* base_name)
{
    if (strcmp(base_name, "coin") == 0) {
        CMCoin* coin = new CMCoin();
        coin->create();
        coin->set("base_name", "coin");
        g_created_coins.push_back(coin);
        return coin;
    }
    CItem* it = new CItem();
    it->set("base_name", base_name);
    return it;
}

}  // namespace

class CmdUserVendorTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallListGoods(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"listgoods", me, target, arg);
    }
    static int CallBuy(CChar* me, int32_t target, char* arg)
    {
        CCmdUser cmd;
        return cmd.handlecommand((char*)"buy", me, target, arg);
    }

    CCmdUser cmd;
    CChar* me;      // 买家（录制用户，接菜单与私聊文案）
    CChar* seller;  // 卖家（普通角色，在场即可见）
    CContainer* room;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;
    MockDatabase* db    = nullptr;     // 玩家搬入/搬出会触发 env->save()
    std::vector<CContainer*> m_items;  // 用例造的货物，收尾销毁

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        // 玩家移动链路会触存盘（g_db 空引用即崩），mock 接住（同
        // test_vendor.cpp）
        db                = new MockDatabase();
        g_db              = db;
        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &vendor_item_factory;
        g_item_mgr        = mgr;

        room = new CContainer();
        room->set("name", "test_room");
        // 生产房间经 create_room 一律置 infinity：地面不受重量闸门限制，
        // 拿不动的货才能落回地上（move 的重量检查对 infinity 跳过）
        room->set("infinity", 1);

        auto* buyer = new StateRecordingUser();
        buyer->set_name("测试者", "tester1");
        buyer->set("level", (long)50);
        buyer->set("hp", (long)100);
        me = buyer;
        me->move(room);
        // 代答切图：玩家入房带 client/loading，present() 视其不可见
        me->delete_temp("client/loading");

        seller = new CChar();
        seller->set_name("货郎", "vendor1");
        seller->set("hp", (long)100);
        seller->move(room);
    }

    void TearDown() override
    {
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr = nullptr;
        FreeItems();
        seller->move(nullptr);
        delete seller;
        me->move(nullptr);
        delete me;  // 虚析构：StateRecordingUser 正常回收
        room->move(nullptr);
        delete room;
        for (auto* c : g_created_coins) delete c;
        g_created_coins.clear();
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    void FreeItems()
    {
        for (auto* it : m_items) {
            it->move(nullptr);
            delete it;
        }
        m_items.clear();
    }

    // 造一件挂摊货物（trade/value + trade/owner 是摆摊的可观察标记）
    CContainer* MakeGoods(int32_t value, const char* owner_id,
                          const char* name = "货物")
    {
        CItem* it = new CItem();
        it->set_name(name, "goods");
        it->set("base_name", "goods");
        it->set_temp("trade/value", value);
        it->set_temp("trade/owner", owner_id);
        it->move(seller);
        m_items.push_back(it);
        return it;
    }

    // 给买家一些现金（真实 CMCoin，走 player_pay 的计价口径）
    void GiveCash(CChar* who, int32_t amount)
    {
        CMCoin* coin = new CMCoin();
        coin->create();
        coin->set("base_name", "coin");
        coin->set_amount(amount);
        coin->move(who);
        m_items.push_back(coin);
    }
};

TEST_F(CmdUserVendorTest, ListGoods_ViaHandlecommand)
{
    int result = cmd.handlecommand((char*)"listgoods", me, 0, (char*)"");
    EXPECT_NE(result, -1);
}

TEST_F(CmdUserVendorTest, Buy_ViaHandlecommand)
{
    int result = cmd.handlecommand((char*)"buy", me, 0, (char*)"");
    EXPECT_NE(result, -1);
}

// =====================================================================
// list_goods：货源清点（无目标/看自己/有货/无货四态）
// =====================================================================
TEST_F(CmdUserVendorTest, ListGoods_NoTarget_Rejected)
{
    EXPECT_EQ(0, CallListGoods(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("查看谁的货物"));
}

TEST_F(CmdUserVendorTest, ListGoods_SelfTarget_Rejected)
{
    EXPECT_EQ(0, CallListGoods(me, me->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("红顶商人"));
}

TEST_F(CmdUserVendorTest, ListGoods_ListsTradeItemsOnly)
{
    MakeGoods(100, "vendor1", "匕首");
    MakeGoods(200, "vendor1", "短剑");
    // 非卖品（无摊位标记）与他人代卖（owner 不符）不入单
    CItem* plain = new CItem();
    plain->set_name("私物", "private");
    plain->move(seller);
    m_items.push_back(plain);
    CItem* others = new CItem();
    others->set_name("别家货", "foreign");
    others->set_temp("trade/value", 999);
    others->set_temp("trade/owner", "someone_else");
    others->move(seller);
    m_items.push_back(others);

    EXPECT_EQ(1, CallListGoods(me, seller->object_id(), (char*)""));
    EXPECT_TRUE(static_cast<StateRecordingUser*>(me)->has("匕首 ：100"));
    EXPECT_TRUE(static_cast<StateRecordingUser*>(me)->has("短剑 ：200"));
    EXPECT_FALSE(static_cast<StateRecordingUser*>(me)->has("私物"));
    EXPECT_FALSE(static_cast<StateRecordingUser*>(me)->has("别家货"));
}

TEST_F(CmdUserVendorTest, ListGoods_NoGoods_SaysUninteresting)
{
    EXPECT_EQ(1, CallListGoods(me, seller->object_id(), (char*)""));
    EXPECT_TRUE(static_cast<StateRecordingUser*>(me)->has("似乎不感兴趣"));
}

// =====================================================================
// buy：验货、付款、过户、找零路径
// =====================================================================
TEST_F(CmdUserVendorTest, Buy_NoTarget_Rejected)
{
    EXPECT_EQ(0, CallBuy(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("买看谁的货物"));
}

TEST_F(CmdUserVendorTest, Buy_SelfTarget_Rejected)
{
    EXPECT_EQ(0, CallBuy(me, me->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("那还用买吗"));
}

TEST_F(CmdUserVendorTest, Buy_NotForSale_Rejected)
{
    // 目标物件不存在
    EXPECT_EQ(0, CallBuy(me, seller->object_id(), (char*)"999999"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不打算卖"));

    // 绑定物件（owner 非空）拒卖
    CItem* bound = new CItem();
    bound->set_name("绑定品", "bound");
    bound->set("owner", "someone");
    bound->set_temp("trade/value", 50);
    bound->set_temp("trade/owner", "vendor1");
    bound->move(seller);
    m_items.push_back(bound);
    char bound_arg[24];
    snprintf(bound_arg, sizeof(bound_arg), "%ld", bound->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallBuy(me, seller->object_id(), bound_arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不打算卖"));

    // 摊位主人不符（别人挂的货）
    CItem* foreign = new CItem();
    foreign->set_name("别家货", "foreign2");
    foreign->set_temp("trade/value", 50);
    foreign->set_temp("trade/owner", "someone_else");
    foreign->move(seller);
    m_items.push_back(foreign);
    char foreign_arg[24];
    snprintf(foreign_arg, sizeof(foreign_arg), "%ld", foreign->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallBuy(me, seller->object_id(), foreign_arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不打算卖"));
}

TEST_F(CmdUserVendorTest, Buy_PoorBuyer_Rejected)
{
    CContainer* goods = MakeGoods(300, "vendor1", "铜甲");
    char arg[24];
    snprintf(arg, sizeof(arg), "%ld", goods->object_id());

    // 身无分文：付不出去
    EXPECT_EQ(0, CallBuy(me, seller->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("摸摸口袋"));
    EXPECT_EQ(static_cast<CContainer*>(seller), goods->environment())
        << "没付钱货物不得过户";
}

TEST_F(CmdUserVendorTest, Buy_TransfersGoodsAndPaysSeller)
{
    CContainer* goods = MakeGoods(300, "vendor1", "铜甲");
    GiveCash(me, 500);
    char arg[24];
    snprintf(arg, sizeof(arg), "%ld", goods->object_id());

    EXPECT_EQ(1, CallBuy(me, seller->object_id(), arg));
    EXPECT_EQ(static_cast<CContainer*>(me), goods->environment())
        << "付款后货物过户给买家";
    // 卖家收到货款（pay_player 经工厂造币）
    EXPECT_EQ(1, mgr->load_item_call_count) << "卖家收款取币一次";
    CContainer* paid = seller->present("coin");
    ASSERT_NE(nullptr, paid);
    EXPECT_EQ(300, paid->query_amount());
    // 买家付了 300：兜里只剩 200
    CContainer* wallet = me->present("coin");
    ASSERT_NE(nullptr, wallet);
    EXPECT_EQ(200, wallet->query_amount());
}

TEST_F(CmdUserVendorTest, Buy_HeavyGoods_DroppedToGround)
{
    // 摊主可存货不受限（生产 NPC 摆摊者带 infinity），买家负重有限：
    // 过户时被重量闸门拦下 -> 货落回地面并告知
    seller->set("infinity", 1);
    CItem* heavy = new CItem();
    heavy->set_name("巨石", "heavy");
    heavy->set("base_name", "heavy");
    heavy->set_temp("trade/value", 10);
    heavy->set_temp("trade/owner", "vendor1");
    heavy->set_weight(1000);  // 买家负重 0：拿不动
    heavy->move(seller);
    m_items.push_back(heavy);
    GiveCash(me, 50);

    char arg[24];
    snprintf(arg, sizeof(arg), "%ld", heavy->object_id());
    EXPECT_EQ(1, CallBuy(me, seller->object_id(), arg));
    EXPECT_EQ(static_cast<CContainer*>(room), heavy->environment())
        << "拿不动的货落回地面";
    EXPECT_TRUE(static_cast<StateRecordingUser*>(me)->has("拿不动"));
}
