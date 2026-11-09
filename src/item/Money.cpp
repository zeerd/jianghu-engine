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

// pay_money(kind, me, value) -> number - 资金发放/扣取，KIND 取 give（发放
// 给玩家）/ take（从玩家扣取），成功返回 1，失败返回 0，未登记 KIND 报错。
// 全局名避开脚本侧常见变量 pay/money/charge（同名局部会遮蔽全局函数）。
int l_pay_money(lua_State* L)
{
    const char* kind = luaL_checkstring(L, 1);
    CChar* me        = dynamic_cast<CChar*>(get_object(L, 2));
    if (!me) return 0;
    int32_t value = (int32_t)luaL_checkinteger(L, 3);
    int ok;
    if (strcmp(kind, "give") == 0)
        ok = CMoney::pay_player(me, value);
    else if (strcmp(kind, "take") == 0)
        ok = CMoney::player_pay(me, value);
    else
        return luaL_error(L, "unknown pay kind: %s", kind);
    lua_pushinteger(L, ok);
    return 1;
}
