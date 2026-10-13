/**
 * @file Money.cpp
 * @brief implementation of the CMoney class. */
#include <windows.h>

#include "Char.h"
#include "Money.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CMoney::CMoney() {}

CMoney::~CMoney() {}

// 重载Move，到非Room中可自动合并
int CMoney::move(CContainer* me)
{
    DTItemList search;
    CContainer *env, *t;
    int32_t total;
    const char* id;

    if (!CItem::move(me)) return 0;  // 未移动成功。

    if ((env = environment())) {
        if (env->query_attr(IS_ROOM)) return 1;

        copy_list(&search, env->get_item_list());
        id    = querystr("base_name");
        total = query_amount();
        for (auto val : search) {
            t = val;
            if (t == this) continue;
            if (!t->query_attr(IS_ITEM)) continue;
            if (EQUALSTR(t->querystr("base_name"), id)) {
                total += static_cast<CItem*>(t)->query_amount();
                set_amount(total);
                DESTRUCT_OB(t, "CMoney::move");
            }
        }
    }

    return 1;
}

int32_t CMoney::query_attr(int attr)
{
    if (attr == IS_MONEY) return 1;
    if (attr == CAN_COMBINE) return 1;

    return CItem::query_attr(attr);
}

int CMoney::pay_player(CChar* me, int32_t amount)
{
    CContainer* gold;

    if (amount < 1) return 0;

    if (!(gold = g_item_mgr->load_item("coin"))) return 0;
    gold->set_amount(amount);

    if (!gold->move(me)) gold->move(me->environment());

    return 1;
}

int CMoney::player_pay(CChar* me, int32_t amount)
{
    CContainer* money;
    int32_t gc;

    if (amount <= 0) return 0;

    if (!(money = me->present_name("coin", IS_MONEY))) return 0;
    gc = money->query_amount() * money->query("base_value");

    if (gc < amount) return 0;
    money->set_amount(gc - amount);
    return 1;
}

char* CMoney::money_str(int32_t amount)
{
    static char output[255];
    snprintf(output, 255, "%d金币", amount);
    return output;
}

void CMCoin::create(int flag)
{
    set_name("金币", "coin");
    set("base_value", 1);
    set("base_weight", 0l);
    set_amount(1);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// pay_player(me, money) - 给玩家发放（扣除）江湖货币
int l_pay_player(lua_State* L)
{
    CChar* me = dynamic_cast<CChar*>(get_object(L, 1));
    if (!me) return 0;
    int32_t money = (int32_t)luaL_checkinteger(L, 2);
    CMoney::pay_player(me, money);
    return 0;
}

// player_pay(me, value) -> number - 玩家支付金额，成功返回 1，失败返回 0
int l_player_pay(lua_State* L)
{
    CChar* me = dynamic_cast<CChar*>(get_object(L, 1));
    if (!me) return 0;
    int32_t value = (int32_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, CMoney::player_pay(me, value));
    return 1;
}
