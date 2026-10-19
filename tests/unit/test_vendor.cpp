// test_vendor.cpp - CVendor（item/Vendor.cpp）买卖行为契约测试
//
// 覆盖改动面（Lua 化前锁定既有可观察行为，Lua 化后功能不变）：
//   1. query_attr(IS_VENDOR)；do_action(DO_LIST) 转发；
//   2. do_list 分页与 send_table 载荷（good/name/value/amount，值=进价*6/5）；
//   3. do_sell 的否决条件与收购价（value*70/100）、value<50 销毁 / >=50 上架；
//   4. do_buy 的常规定价（value*12/10）、售罄下架、支付失败文案；
//      成交前改写/否决与成交后结算走通用钩子 hooks.pre_buy /
//      hooks.post_buy；
//   5. carry_good 上架与同类合并；
//   6. handle_action 的 list/buy/sell 分发与战斗中拒答；
//   7. 贸易任务（红顶商人）在真实 dataset 钩子下的等价性（Lua 化后功能不变）。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <lua.hpp>

#include "Char.h"
#include "Container.h"
#include "Item.h"
#include "Money.h"
#include "User.h"
#include "Vendor.h"
#include "Global.h"
#include "HeartBeat.h"
#include "IDatabase.h"
#include "ItemMgr.h"
#include "Mapping.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

// notify_fail 写入的全局缓冲（CmdHandle.cpp 定义），用于断言否决文案
extern char g_notify_fail_msg[512];

namespace {

// 商品工厂：按 base_name 出货，value/index 由测试指定，
// 并把工厂产出的物件登记下来，用例结束时统一回收
// （add_good/do_sell 会用 DESTRUCT_OB 移交给延迟销毁队列）。
class VendorFactory {
public:
    explicit VendorFactory(CMockItemMgr* mgr) : m_mgr(mgr)
    {
        m_saved             = g_item_mgr;
        m_mgr->load_factory = &VendorFactory::create;
        s_self              = this;
        g_item_mgr          = m_mgr;
    }
    ~VendorFactory()
    {
        g_item_mgr          = m_saved;
        m_mgr->load_factory = nullptr;
        s_self              = nullptr;

        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        for (auto it = g_destruct_list.begin(); it != g_destruct_list.end();) {
            if (std::find(m_created.begin(), m_created.end(), *it)
                != m_created.end()) {
                it = g_destruct_list.erase(it);
            }
            else {
                ++it;
            }
        }
        for (auto* p : m_created) delete p;
        m_created.clear();
    }

    // 出货参数（工厂按 base_name 分派）
    void expect(const char* base_name, int32_t value, int index = 0,
                int32_t weight = 0)
    {
        m_plan[base_name] = std::make_tuple(value, index, weight);
    }

    // 登记非工厂产出的物件，随夹具一起回收
    void adopt(CContainer* ob) { m_created.push_back(ob); }

    static CContainer* create(const char* base_name)
    {
        // 与生产 create_item 一致：coin 走 CMCoin（IS_MONEY/CAN_COMBINE）
        CItem* it = (std::strcmp(base_name, "coin") == 0)
                        ? static_cast<CItem*>(new CMCoin())
                        : new CItem();
        it->set("base_name", base_name);
        it->set("id", base_name);
        it->set("name", base_name);
        auto self = s_self;
        if (self) {
            auto it2 = self->m_plan.find(base_name);
            if (it2 != self->m_plan.end()) {
                it->set("value", std::get<0>(it2->second));
                it->set("index", std::get<1>(it2->second));
                if (std::get<2>(it2->second))
                    it->set_weight(std::get<2>(it2->second));
            }
        }
        if (self) self->m_created.push_back(it);
        return it;
    }

private:
    static inline VendorFactory* s_self = nullptr;
    std::map<std::string, std::tuple<int32_t, int, int32_t>> m_plan;
    std::vector<CContainer*> m_created;
    CMockItemMgr* m_mgr;
    IItemMgr* m_saved = nullptr;
};

class VendorTest : public ::testing::Test {
protected:
    CMockItemMgr mgr;
    MockDatabase db;
    StateRecordingUser* me = nullptr;
    CContainer* env        = nullptr;
    lua_State* L           = nullptr;

    void SetUp() override
    {
        L = ensure_lua_env();
        // 物件移入玩家背包会触发 env->save()（g_db 解引用），用 mock 兜住
        g_db = &db;
        me   = new StateRecordingUser();
        me->set("id", "vbuyer");
        me->set("name", "买家");
        me->set("gender", "男性");
        env = new CContainer();
        env->set("name", "商店");
        env->set("base_name", "vendor_test_room");
        me->move(env);
    }

    void TearDown() override
    {
        // 清理本用例可能安装的数据集钩子，避免泄漏到其他用例
        if (L)
            luaL_dostring(L,
                          "if hooks then hooks.pre_buy=nil "
                          "hooks.post_buy=nil end");
        me->move(nullptr);
        delete me;
        delete env;
        g_db = nullptr;
    }

    // 给买家塞钱（player_pay 按 base_name="coin" 且 IS_MONEY 找钱；
    // 物件由工厂登记回收，move 触发的合并销毁也落在回收网内）
    void GiveMoney(int32_t amount)
    {
        CItem* coin = static_cast<CItem*>(VendorFactory::create("coin"));
        // create 补 base_value=1（player_pay 按 amount*base_value 计价）
        static_cast<CMCoin*>(coin)->create();
        coin->set("base_name", "coin");
        coin->set_amount(amount);
        coin->move(me);
    }
};

// 可堆叠且非钱的测试物品（覆盖 do_sell 的 split 分支：
// IS_ITEM && amount>1 时先拆出 1 支再卖）
class StackableThing : public CItem {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == CAN_COMBINE) return 1;
        return CItem::query_attr(attr);
    }
};

}  // namespace

// ---------------- query_attr / do_action ----------------
TEST_F(VendorTest, QueryAttrIsVendor)
{
    CVendor v;
    EXPECT_EQ(v.query_attr(CMainObject::IS_VENDOR), 1);
    EXPECT_EQ(v.query_attr(CNpc::IS_NPC), 1);
}

TEST_F(VendorTest, DoActionListForwards)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("bread", 10);
    v.carry_good("bread", 2);

    me->clear();
    ASSERT_EQ(v.do_action(CChar::DO_LIST, me), 1);
    EXPECT_EQ(me->sent, 1);
    EXPECT_TRUE(me->has("T=vendor&"));
}

// ---------------- carry_good / do_list ----------------
TEST_F(VendorTest, CarryGoodMergesSameGoodAndListShowsMarkup)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("bread", 10);

    v.carry_good("bread", 3);
    v.carry_good("bread", 4);  // 同 base_name/index 合并为一件商品
    v.carry_good("rice", 0);   // 数量 0 不上架

    me->clear();
    ASSERT_EQ(v.do_list(me, 0), 1);
    ASSERT_EQ(me->sent, 1);
    // 单件商品 → st=3 单包；标价 = value*6/5 = 12
    std::string pkt = me->msgs[0];
    EXPECT_NE(pkt.find("st=3&"), std::string::npos) << pkt;
    EXPECT_NE(pkt.find("T=vendor&"), std::string::npos) << pkt;
    EXPECT_NE(pkt.find("name=bread&"), std::string::npos) << pkt;
    EXPECT_NE(pkt.find("value=12&"), std::string::npos) << pkt;
    EXPECT_NE(pkt.find("amount=7&"), std::string::npos) << pkt;
}

TEST_F(VendorTest, DoListSecondPageEmpty)
{
    VendorFactory f(&mgr);
    CVendor v;
    f.expect("bread", 10);
    v.carry_good("bread", 1);

    // 页号越界：无行可发（空表 0 包），仍返回 1
    me->clear();
    EXPECT_EQ(v.do_list(me, 5), 1);
    EXPECT_EQ(me->sent, 0);
}

// ---------------- do_buy ----------------
TEST_F(VendorTest, DoBuyZeroIdAndUnknownGoodReturnZero)
{
    CVendor v;
    EXPECT_EQ(v.do_buy(me, 0), 0);
    EXPECT_EQ(v.do_buy(me, 999), 0);
}

TEST_F(VendorTest, DoBuyNoMoneyTellsPoor)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("bread", 100);
    v.carry_good("bread", 1);

    // 买家身无分文 → 支付失败
    ASSERT_EQ(v.do_buy(me, 1), 1);
    EXPECT_TRUE(me->has("穷光蛋，一边呆着去！"));
}

TEST_F(VendorTest, DoBuyRegularPrice)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("bread", 100);
    v.carry_good("bread", 2);
    GiveMoney(1000);

    // 常规价 = value * 12 / 10 = 120
    ASSERT_EQ(v.do_buy(me, 1), 1);

    CContainer* got = me->present("bread");
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->query_amount(), 1);
    EXPECT_TRUE(me->has("买下了数量1的bread"));

    // 剩余 1 件，仍在架
    me->clear();
    ASSERT_EQ(v.do_list(me, 0), 1);
    EXPECT_NE(me->msgs[0].find("amount=1&"), std::string::npos);
}

// C++ 不再认识任何贸易任务键；成交价改由通用钩子 hooks.pre_buy 改写。
// 本用例锁定 seam：商品以快照 table 送达（goods.price 为 C++ 常规价），
// 脚本返回 {price=…} 时按返回值收款（此处按 goods.price 的十分之一）。
TEST_F(VendorTest, DoBuyPreBuyHookOverridesPrice)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("shield", 1000);  // 默认价会是 1000*12/10=1200
    v.carry_good("shield", 1);
    GiveMoney(2000);

    ASSERT_NE(L, nullptr);
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.pre_buy = function(vendor, buyer, goods)\n"
                            "  if goods.basename == 'shield' then\n"
                            "    return {price = goods.price // 10}\n"
                            "  end\n"
                            "  return nil\n"
                            "end"),
              LUA_OK);

    ASSERT_EQ(v.do_buy(me, 1), 1);
    EXPECT_NE(me->present("shield"), nullptr);
    // 收款 120（1200 的十分之一）而非 1200：余额 2000-120=1880
    CContainer* coin = me->present_name("coin", CMainObject::IS_MONEY);
    ASSERT_NE(coin, nullptr);
    EXPECT_EQ(coin->query_amount(), 1880);
}

// hooks.pre_buy 返回 {reason=文案} = 拒绝成交（notify_fail）：不扣钱、不交货。
TEST_F(VendorTest, DoBuyPreBuyHookCanReject)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("shield", 1000);
    v.carry_good("shield", 1);
    GiveMoney(2000);

    ASSERT_NE(L, nullptr);
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.pre_buy = function(vendor, buyer, goods)\n"
                            "  return {reason = '此货非卖品。'}\n"
                            "end"),
              LUA_OK);

    EXPECT_EQ(v.do_buy(me, 1), 0);
    EXPECT_EQ(me->present("shield"), nullptr) << "否决后不应交货";
    EXPECT_NE(std::string(g_notify_fail_msg).find("非卖品"), std::string::npos);

    CContainer* coin = me->present_name("coin", CMainObject::IS_MONEY);
    ASSERT_NE(coin, nullptr);
    EXPECT_EQ(coin->query_amount(), 2000) << "否决后不应扣款";
}

// 成交后 C++ 调用 hooks.post_buy（数据集侧在此标记任务完成、播报专属文案）。
// 本用例锁定 seam：同一份商品快照送达，goods.price 为实付价、
// goods.amount 为成交后剩余库存，脚本无需重算即可引用。
TEST_F(VendorTest, DoBuyPostBuyHookReceivesPaidGoods)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("shield", 100);  // 常规价 100*12/10=120
    v.carry_good("shield", 3);
    GiveMoney(2000);

    ASSERT_NE(L, nullptr);
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.post_buy = function(vendor, buyer, goods)\n"
                            "  buyer:set('test/paid', goods.price)\n"
                            "  buyer:set('test/left', goods.amount)\n"
                            "end"),
              LUA_OK);

    ASSERT_EQ(v.do_buy(me, 1), 1);
    EXPECT_EQ(me->query("test/paid"), 120) << "post_buy 应收到实付价";
    EXPECT_EQ(me->query("test/left"), 2) << "post_buy 应收到成交后库存";
}

// 对特定人群禁售：规则只读 buyer 身份 + goods 属性，C++ 零改动。
// （接口通用性的验收场景：不必为"谁能买什么"新增任何 C++ 参数）
TEST_F(VendorTest, DoBuyPreBuyHookCanBanByIdentity)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("shield", 1000);
    f.expect("bread", 10);
    v.carry_good("shield", 1);
    v.carry_good("bread", 1);
    GiveMoney(2000);

    me->set("family/family_name", "明教");

    ASSERT_NE(L, nullptr);
    ASSERT_EQ(luaL_dostring(L,
                            "hooks.pre_buy = function(vendor, buyer, goods)\n"
                            "  if goods.basename == 'shield'\n"
                            "     and buyer:querystr('family/family_name') == "
                            "'明教' then\n"
                            "    return {reason = '本号不卖给明教弟子。'}\n"
                            "  end\n"
                            "  return nil\n"
                            "end"),
              LUA_OK);

    // 被禁人群 + 受限商品 → 否决（不扣钱不交货）
    EXPECT_EQ(v.do_buy(me, 1), 0);
    EXPECT_NE(std::string(g_notify_fail_msg).find("不卖给明教"),
              std::string::npos);
    EXPECT_EQ(me->present("shield"), nullptr);

    // 同一买家买不受限商品 → 照常成交
    EXPECT_EQ(v.do_buy(me, 2), 1);
    EXPECT_NE(me->present("bread"), nullptr);

    // 非受限人群买同一商品 → 成交
    me->set("family/family_name", "武当");
    EXPECT_EQ(v.do_buy(me, 1), 1);
    EXPECT_NE(me->present("shield"), nullptr);
}

TEST_F(VendorTest, DoBuyLastOneRemovesGoodFromShelf)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("bread", 100);
    v.carry_good("bread", 1);
    GiveMoney(1000);

    EXPECT_EQ(v.do_buy(me, 1), 1);
    EXPECT_NE(me->present("bread"), nullptr);

    // 卖光后商品下架：同一 good_id 再买直接失败
    EXPECT_EQ(v.do_buy(me, 1), 0);
}

// 负重不起：买入失败 → 掉到玩家所在环境（环境的容量足够接住）
TEST_F(VendorTest, DoBuyOverWeightDropsToEnvironment)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("anvil", 100, 0, 100);  // 重 100
    v.carry_good("anvil", 1);
    GiveMoney(1000);

    me->set_max_encumbrance(10);     // 背不动
    env->set_max_encumbrance(1000);  // 地上接得住

    ASSERT_EQ(v.do_buy(me, 1), 1);
    EXPECT_EQ(me->present("anvil"), nullptr) << "不应进入背包";
    CContainer* dropped = env->present("anvil");
    EXPECT_NE(dropped, nullptr) << "应掉落在环境中";
    EXPECT_TRUE(me->has("由于你负重不起"));

    // 掉地物件仍挂在 env 列表；先脱离环境，避免工厂（用例体内局部）
    // 先于 TearDown 析构后 env 残留悬垂指针
    dropped->move(nullptr);
}

// ---------------- do_sell ----------------
TEST_F(VendorTest, DoSellNullNoop)
{
    CVendor v;
    EXPECT_EQ(v.do_sell(me, nullptr), 0);
}

TEST_F(VendorTest, DoSellRefusesNoSellFlag)
{
    VendorFactory f(&mgr);
    CVendor v;
    CItem* item = static_cast<CItem*>(VendorFactory::create("charm"));
    item->set("no_sell", 1);
    EXPECT_EQ(v.do_sell(me, item), 0);
    EXPECT_NE(std::string(g_notify_fail_msg).find("不能卖"), std::string::npos);
}

TEST_F(VendorTest, DoSellRefusesMoney)
{
    VendorFactory f(&mgr);
    CVendor v;
    CItem* coin = static_cast<CItem*>(VendorFactory::create("coin"));
    ASSERT_EQ(v.do_sell(me, coin), 1);
    EXPECT_TRUE(me->has("你想卖「钱」？？"));
}

TEST_F(VendorTest, DoSellRefusesBoundOrOwnerItem)
{
    VendorFactory f(&mgr);
    CVendor v;
    CItem* item = static_cast<CItem*>(VendorFactory::create("bound"));
    item->set("owner", "someone");
    EXPECT_EQ(v.do_sell(me, item), 0);
    EXPECT_NE(std::string(g_notify_fail_msg).find("不能卖"), std::string::npos);
}

TEST_F(VendorTest, DoSellRefusesCheapItem)
{
    VendorFactory f(&mgr);
    CVendor v;
    CItem* item = static_cast<CItem*>(VendorFactory::create("junk"));
    item->set("value", 5);
    item->move(me);
    EXPECT_EQ(v.do_sell(me, item), 0);
    EXPECT_NE(std::string(g_notify_fail_msg).find("一文不值"),
              std::string::npos);
}

TEST_F(VendorTest, DoSellCheapItemPaysAndDestructs)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    CItem* item = static_cast<CItem*>(VendorFactory::create("trash"));
    item->set("value", 20);  // <50：收购后直接销毁，不上架
    item->move(me);

    ASSERT_EQ(v.do_sell(me, item), 1);
    // 收购价 = value * 70 / 100 = 14 金币
    EXPECT_TRUE(me->has("卖掉了数量1的trash"));

    me->clear();
    ASSERT_EQ(v.do_list(me, 0), 1);
    EXPECT_EQ(me->sent, 0) << "低价物品不应进入商品清单";
}

TEST_F(VendorTest, DoSellPricyItemJoinsShelf)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    f.expect("sword", 300);
    CItem* item = static_cast<CItem*>(VendorFactory::create("sword"));
    item->set("value", 300);
    item->move(me);

    ASSERT_EQ(v.do_sell(me, item), 1);
    me->clear();
    ASSERT_EQ(v.do_list(me, 0), 1);
    EXPECT_NE(me->msgs[0].find("name=sword&"), std::string::npos);
    EXPECT_NE(me->msgs[0].find("value=360&"), std::string::npos);
}

TEST_F(VendorTest, DoSellStackedItemSplitsOne)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("老板", "boss");
    // 拆分件由 load_item 出货，其 value 取自工厂计划表（>=50 才会上架）
    f.expect("arrow", 100);
    StackableThing* stack = new StackableThing();
    stack->set("base_name", "arrow");
    stack->set("id", "arrow");
    stack->set("name", "arrow");
    stack->set("value", 100);
    stack->set_amount(5);
    f.adopt(stack);
    stack->move(me);

    ASSERT_EQ(v.do_sell(me, stack), 1);
    EXPECT_EQ(stack->query_amount(), 4) << "只卖掉一支，其余留在背包";
    me->clear();
    ASSERT_EQ(v.do_list(me, 0), 1);
    EXPECT_NE(me->msgs[0].find("name=arrow&"), std::string::npos);
}

// ---------------- handle_action ----------------
TEST_F(VendorTest, HandleActionRefusesWhileFighting)
{
    CVendor v;
    // 战斗判定落在商人自身（CVendor::handle_action 的 is_fighting）
    v.set_temp("fight/is_fighting", 1);
    char comm[] = "list";
    char arg[]  = "0";
    EXPECT_EQ(v.handle_action(comm, me, arg), 0);
}

TEST_F(VendorTest, HandleActionList)
{
    VendorFactory f(&mgr);
    CVendor v;
    f.expect("bread", 10);
    v.carry_good("bread", 1);

    char comm[] = "list";
    char arg[]  = "0";
    me->clear();
    EXPECT_EQ(v.handle_action(comm, me, arg), 1);
    EXPECT_TRUE(me->has("T=vendor&"));
}

TEST_F(VendorTest, HandleActionUnknownCommand)
{
    CVendor v;
    char comm[] = "talk";
    char arg[]  = "";
    EXPECT_EQ(v.handle_action(comm, me, arg), 0);
}

TEST_F(VendorTest, HandleActionBuyUnknownGoodReturnsZero)
{
    CVendor v;
    char comm[] = "buy";
    char arg[]  = "999";
    EXPECT_EQ(v.handle_action(comm, me, arg), 0);
}

TEST_F(VendorTest, HandleActionSellWithoutTargetReturnsZero)
{
    CVendor v;
    char comm[] = "sell";
    char arg[]  = "12345";  // 背包里没有该 object_id
    EXPECT_EQ(v.handle_action(comm, me, arg), 0);
}

// ====================================================================
// 红顶商人贸易任务在真实 dataset 钩子下的等价性（Lua 化后功能不变）
// 数据源头：npc/pub2/pub_xiaoer4.lua 派发 trade/obj|idx|val；
// 规则实现：data/hooks.lua 的 pre_buy / post_buy。
// ====================================================================
class VendorTradeTaskTest : public ::testing::Test {
protected:
    CMockItemMgr mgr;
    MockDatabase db;
    StateRecordingUser* me = nullptr;
    CContainer* env        = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        g_db = &db;
        me   = new StateRecordingUser();
        me->set("id", "vtrader");
        me->set("name", "商人");
        me->set("gender", "男性");
        env = new CContainer();
        env->set("name", "商店");
        env->set("base_name", "trade_test_room");
        me->move(env);
    }

    void TearDown() override
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        if (L)
            luaL_dostring(L,
                          "if hooks then hooks.pre_buy=nil "
                          "hooks.post_buy=nil end");
        me->move(nullptr);
        delete me;
        delete env;
        g_db = nullptr;
    }

    void GiveMoney(int32_t amount)
    {
        CItem* coin = static_cast<CItem*>(VendorFactory::create("coin"));
        static_cast<CMCoin*>(coin)->create();
        coin->set("base_name", "coin");
        coin->set_amount(amount);
        coin->move(me);
    }
};

TEST_F(VendorTradeTaskTest, TradeTaskIgnoredWhenAlreadyDone)
{
    VendorFactory f(&mgr);
    CVendor v;
    v.set_name("萧半和", "xiao");
    f.expect("shield", 1000);
    v.carry_good("shield", 1);
    GiveMoney(2000);

    me->set("trade/obj", "shield");
    me->set("trade/idx", 0);
    me->set("trade/val", 120);
    me->set("trade/done", 1);  // 已完成 → 走常规价 1200

    ASSERT_EQ(v.do_buy(me, 1), 1);
    CContainer* coin = me->present_name("coin", CMainObject::IS_MONEY);
    ASSERT_NE(coin, nullptr);
    EXPECT_EQ(coin->query_amount(), 800);
}
