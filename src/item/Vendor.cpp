/**
 * @file Vendor.cpp
 * @brief implementation of the CVendor class. */
#include <windows.h>

#include <algorithm>
#include <string>

#include "LuaHook.h"
#include "Money.h"
#include "Vendor.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

// 标准类：商人
CVendor::CVendor()
{
    m_good_code   = 1;
    m_vendor_type = 0;
}

CVendor::~CVendor() { m_good_list.clear(); }

int32_t CVendor::query_attr(int attr)
{
    switch (attr) {
        case IS_VENDOR:
            return 1;
    }
    return CNpc::query_attr(attr);
}

int CVendor::do_action(int action, CChar* me)
{
    if (action == DO_LIST) return do_list(me);

    return CNpc::do_action(action, me);
}

// 显示商品清单 一页10个商品 页号从0开始
int CVendor::do_list(CChar* me, int page)
{
    char version[40];

    strncpy(version, me->querystr_temp("client/version"), 40);

    DTGood* good;
    int i = 0, begin = page * 10, end = page * 10 + 10;
    CMapping m, line;

    int total = m_good_list.size() / 10;
    for (auto& val : m_good_list) {
        good = val.get();
        if (i >= begin) {
            char cmd[20];

            line.set("good", snprintf(cmd, 20, "%d", good->m_code));
            line.set("name", good->m_name);
            line.set("value", snprintf(cmd, 20, "%d", good->m_value * 6 / 5));
            line.set("amount", snprintf(cmd, 20, "%d", good->m_amount));

            sprintf(cmd, "%03d", i);
            m.set(cmd, line);
        }
        i++;
        if (i >= end) break;
    }

    // page and total -> total * 10 + page
    // the second param for vendor_type
    me->send_table("vendor", &m, total * 10 + page, m_vendor_type);
    return 1;
}

// 卖物品
int CVendor::do_sell(CChar* me, CContainer* obj)
{
    char msg[255];

    if (!obj) return 0;

    if (obj->query("no_sell")) return notify_fail("这样东西不能卖。");

    if (obj->query_attr(IS_MONEY)) {
        tell_object(me, "你想卖「钱」？？");
        return 1;
    }

    if (obj->query_attr(CChar::IS_ITEM) && obj->query_amount() > 1) {
        obj = static_cast<CItem*>(obj)->split(1);
    }

    CMapping* m = obj->query_entire_dbase();
    if (!m->undefinedp("no_drop")) return notify_fail("这样东西不能卖。");

    if (!m->undefinedp("owner")) return notify_fail("这样东西不能卖。");

    if (obj->query("food_supply"))
        return notify_fail("剩菜剩饭留给您自己用吧。");

    int32_t value = obj->query("value");
    if (value < 10) return notify_fail("%s一文不值！", obj->name());

    message_vision(snprintf(msg, 255, "$N卖掉了数量1的%s给$n。", obj->name()),
                   me, this);

    CMoney::pay_player(me, value * 70 / 100);
    if (value < 50)
        DESTRUCT_OB(obj, "CVendor::Do_Sell");
    else {
        add_good(obj);
    }

    return 1;
}

// 买物品
int CVendor::do_buy(CChar* me, int32_t good_id)
{
    CContainer* obj;
    DTGood* good;
    char msg[255];
    int val_factor = 12;
    int32_t money;

    if (!good_id) return 0;
    if (!(good = get_good(good_id))) return 0;

    // 商品字段快照：售罄下架会 erase 掉 good（连同对象一起销毁），
    // 而 pre_buy/post_buy 都要把商品交给脚本，故一次性捕获到局部。
    std::string basename = good->m_basename;
    std::string good_name
        = good->m_name;  // char[40]，此处收拢为 string 供脚本引用
    int index      = good->m_index;
    int32_t code   = good->m_code;
    int32_t value  = good->m_value;
    int32_t amount = good->m_amount;  // 成交前库存

    money = good->m_value * val_factor / 10;

    // 成交前询问数据集：商品以快照 table 整体交给脚本，规则可按商品任意属性
    // 与买卖双方身份表达（任务优惠、对特定人群禁售、限购…）；日后新增商品
    // 字段只在此处多 add 一项，钩子签名不变。
    // 返回 {price=N} 改写成交价；{reason=文案} 否决本次成交。
    // hooks.pre_buy(vendor, buyer, goods)
    {
        CLuaHookCall hook("pre_buy");
        hook.add_object(this)
            .add_object(me)
            .begin_table()
            .table_set_str("name", good_name.c_str())
            .table_set_int("code", code)
            .table_set_str("basename", basename.c_str())
            .table_set_int("index", index)
            .table_set_int("amount", amount)
            .table_set_int("value", value)
            .table_set_int("price", money)
            .end_table();
        if (hook.call() && !hook.result_is_nil()) {
            std::string reason;
            if (hook.table_field_str("reason", reason))
                return notify_fail("%s", reason.c_str());
            int64_t price;
            if (hook.table_field_int("price", price)) money = (int32_t)price;
        }
    }

    if (!CMoney::player_pay(me, money)) {
        tell_object(me, "穷光蛋，一边呆着去！");
        return 1;
    }

    if (!(obj = g_item_mgr->load_item(basename.c_str()))) return 0;
    if (index) obj->create(index);

    // 此物品已卖光
    good->m_amount--;
    if (good->m_amount <= 0) {
        auto it = std::find_if(m_good_list.begin(), m_good_list.end(),
                               [good](const std::unique_ptr<DTGood>& g) {
                                   return g.get() == good;
                               });
        if (it != m_good_list.end()) m_good_list.erase(it);
    }

    // 成交后结算（数据集侧标记任务完成、播报专属文案）。
    // goods.price 为本次实付价、goods.amount 为成交后剩余库存（下架则 0）。
    // hooks.post_buy(vendor, buyer, goods)
    {
        CLuaHookCall hook("post_buy");
        hook.add_object(this)
            .add_object(me)
            .begin_table()
            .table_set_str("name", good_name.c_str())
            .table_set_int("code", code)
            .table_set_str("basename", basename.c_str())
            .table_set_int("index", index)
            .table_set_int("amount", amount - 1 > 0 ? amount - 1 : 0)
            .table_set_int("value", value)
            .table_set_int("price", money)
            .end_table();
        hook.call();
    }

    message_vision(
        snprintf(msg, 255, "$N从$n那里买下了数量1的%s。", obj->name()), me,
        this);
    if (!obj->move(me)) {
        obj->move(me->environment());
        tell_object(me, snprintf(msg, 255, "由于你负重不起，%s掉到了地上。",
                                 obj->name()));
    }

    return 1;
}

// 增加一个商品
void CVendor::add_good(CContainer* item, int32_t amount)
{
    DTGood* good;
    int32_t index;
    string basename;

    basename = item->querystr("base_name");
    index    = item->query("index");
    if (!(good = find_good(basename, index))) {
        auto ng = std::make_unique<DTGood>();
        good    = ng.get();
        m_good_list.push_back(std::move(ng));
        strncpy(good->m_name, item->name(), 40)[39] = 0;
        good->m_basename                            = basename;
        good->m_index                               = index;
        good->m_amount                              = 0;
        // 统一定价:商店基准价=物品当前总价query("value")。
        // 单件商品=单件价(base_value×1);整束商品(如80支箭)=束总价。
        good->m_value = item->query("value");
        good->m_code  = m_good_code++;
    }

    good->m_amount += amount;
    DESTRUCT_OB(item, "CVendor::Add_Good");
}

DTGood* CVendor::find_good(string basename, int index)
{
    for (auto& good : m_good_list) {
        if (good->m_basename == basename && good->m_index == index)
            return good.get();
    }

    return 0;
}

DTGood* CVendor::get_good(int32_t good_id)
{
    for (auto& good : m_good_list) {
        if (good->m_code == good_id) return good.get();
    }

    return 0;
}

// 出售商品
void CVendor::carry_good(const char* good, int32_t amount, int index)
{
    if (amount > 0) {
        CContainer* item = g_item_mgr->load_item(good);

        if (index) item->create(index);
        if (item->query("value") == 0) {
            item->set("value", 1);
        }
        add_good(item, amount);
    }
}

int CVendor::handle_action(char* comm, CChar* me, char* arg)
{
    char szGood[80], szPage[50];

    if (is_fighting()) return 0;
    strncpy(szGood, arg, 80);
    analyse_string(szGood, szPage, 50);

    if (strcmp(comm, "list") == 0) {
        return do_list(me, atol(szGood));  // 商品清单
    }
    else if (strcmp(comm, "buy") == 0) {
        if (do_buy(me, atol(szGood))) {
            snprintf(szGood, 80, "list %ld %s", object_id(), szPage);
            me->command(szGood);
            return 1;
        }
    }
    else if (strcmp(comm, "sell") == 0) {
        int32_t good_id;
        CContainer* obj;

        good_id = atol(arg);
        obj     = me->present(good_id);
        if (do_sell(me, obj)) {
            snprintf(szGood, 80, "list %ld %s", object_id(), szPage);
            me->command(szGood);
            //			me->SendOff();
            return 1;
        }
    }

    return 0;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:carry_good(name[, nAmount[, index]]) - 商家携带待售货物
int l_carry_good(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CVendor* vendor = dynamic_cast<CVendor*>(obj);
    if (!vendor) return 0;

    const char* name = luaL_checkstring(L, 2);
    int32_t amount   = (int32_t)luaL_optinteger(L, 3, 1);
    int index        = (int)luaL_optinteger(L, 4, 0);
    vendor->carry_good(name, amount, index);
    return 0;
}
int l_random_vendor(lua_State* L)
{
    CVendor* v = g_item_mgr->random_vendor();
    if (!v) {
        lua_pushnil(L);
        return 1;
    }
    lua_bind_push_object(L, v);
    return 1;
}
