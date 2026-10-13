/**
 * @file Item.cpp
 * @brief implementation of the CItem class. */

#include <windows.h>

#include "Global.h"
#include "Item.h"
#include "Container.h"
#include "Room.h"
#include "Channel.h"
#include "StackingLogic.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CItem::CItem()
{
    m_amount     = 1;
    m_max_amount = 0;
}

CItem::~CItem() {}

// 移动入口:成功移动后统一触发自动合并检查
int CItem::move(CContainer* me)
{
    if (!CContainer::move(me)) return 0;  // 未移动成功
    check_auto_combine();
    return 1;
}

// 自动合并检查(默认空实现;可堆叠物品子类启用)
// 在CItem::move成功后调用,环境已切换完毕
void CItem::check_auto_combine()
{
    CContainer* env;
    DTItemList search;
    CContainer* t;
    int32_t total;
    const char* id;
    int index;

    if (!query_attr(CAN_COMBINE)) return;  // 只有可堆叠物品才合并
    if (!(env = environment())) return;    // 无环境(销毁中)不合并
    if (env->query_attr(IS_ROOM)) return;  // 房间内(地面)不自动合并

    // yaopin 模式：跳过箱子检查和 equipped/owner 检查（复刻原 CIyaopin::move）
    int yaopin = query("yaopin_combine");

    if (!yaopin && env->query_attr(IS_BOX) && !can_combine_in_box())
        return;  // 箱子内:按子类策略

    // 已被装备/绑定的堆不参与合并（yaopin 模式跳过）
    if (!yaopin && query("equipped")) return;
    if (!yaopin && querystr("owner")[0]) return;

    copy_list(&search, env->get_item_list());
    id    = querystr("base_name");
    index = query("index");
    total = query_amount();
    for (auto val : search) {
        t = val;
        if (t == this) continue;
        if (!t->query_attr(IS_ITEM)) continue;
        if (EQUALSTR(t->querystr("base_name"), id)
            && t->query("index") == index) {
            // 不合并已装备/绑定的同类堆（yaopin 模式跳过）
            if (!yaopin && t->query("equipped")) continue;
            if (!yaopin && t->querystr("owner")[0]) continue;
            total += static_cast<CItem*>(t)->query_amount();
            destruct(t);
        }
    }
    // 数量/重量/价值由set_amount统一维护
    set_amount(total);
}

// 分开成两份
CItem* CItem::split(int32_t amount)
{
    CItem* t;
    int32_t left = m_amount - amount;

    if (amount <= 0 || left < 0) return 0;  // 分不出来
    if (left == 0) return this;

    set_amount(left);
    t = static_cast<CItem*>(g_item_mgr->load_item(querystr("base_name")));
    if (t) {
        if (query("index")) t->create(query("index"));
        t->set_amount(amount);
    }

    return t;
}

int CItem::can_use() { return 0; }

int32_t CItem::query_attr(int attr)
{
    switch (attr) {
        case IS_ITEM:
            return 1;
        case CAN_GET: {
            if (_M.undefinedp("no_get")) return 1;
            return 0;
        }
        case CAN_USE:
            return can_use();
    }

    return CContainer::query_attr(attr);
}

int CItem::do_use(CChar* me, CContainer* target) { return 0; }

// 行为
int CItem::do_action(int act, CChar* me) { return 0; }

int32_t CItem::query_amount() { return m_amount; }

int32_t CItem::query_max_amount()
{
    return (m_max_amount == 0) ? 1 : m_max_amount;
}

void CItem::set_amount(int32_t amount)
{
    if (amount < 0) amount = 0;  // 非法数量归零

    if (m_max_amount == 0) {
        m_max_amount = amount;
    }

    if (amount == 0) {
        m_amount = 0;
        set("amount", 0l);  // 数量同步到_M，防止_M残留旧数量
        // 毁灭自身
        DESTRUCT_OB(this, "CItem::set_amount");
        return;
    }
    else {
        m_amount = amount;
        // 数量无条件同步到_M
        set("amount", amount);
        if (query("base_weight"))
            set_weight(
                StackingLogic::total_weight(query("base_weight"), amount));
        if (query("base_value"))
            set("value",
                StackingLogic::total_value(query("base_value"), amount));
    }

    CContainer* env = environment();
    if (env) {
        if (env->query_attr(IS_BOX)) static_cast<CBox*>(env)->save_local();
        if (userp(env)) env->save();
    }

    update_me();
}

int32_t CItem::add_amount(int32_t val)
{
    set_amount(m_amount + val);

    return m_amount + val;
}

int CItem::do_look(CChar* me)
{
    char msg[255];
    DTItemList* list;
    CContainer* item;

    // 1,先传描述
    say(snprintf(msg, 255, "%s %s", short_desc(), long_desc()), me);

    list = get_item_list();
    if (list->size()
        && (DIFFERSTR(querystr("status"), "locked") || wizardp(me))) {
        add_menu_item("里面有：", "", me);
        for (auto val : *list) {
            item = val;
            add_menu_item(
                item->name(),
                snprintf(msg, 80, "$0get %ld from $1", item->object_id()), me);
        }
    }

    return 0;
}

int32_t CItem::receive_damage(const char* szType, int32_t damage)
{
    int val;

    if (damage <= 0) return 0;
    if (DIFFERSTR(szType, "dp")) return 0;

    val = query(szType) - damage;

    if (val > 0)
        set(szType, val);
    else {
        set(szType, 0l);
        call_out(destruct_me, 1);
    }

    return damage;
}

int32_t CItem::receive_wound(const char* type, int32_t damage)
{
    int32_t val;
    char eff_key[40];

    if (damage <= 0) return 0;
    if (DIFFERSTR(type, "dp")) return 0;

    snprintf(eff_key, 40, "eff_%s", type);

    val = query(eff_key) - damage;

    if (val > 0)
        set(eff_key, val);
    else {
        set(eff_key, 0l);
        call_out(destruct_me, 1);
    }

    if (query(type) > val) set(type, val);
    return damage;
}

void CItem::destruct_me(CContainer* ob, int32_t param1, int32_t param2)
{
    const char* msg = ob->querystr("destruct_msg");
    if (msg[0] && ob->environment()) tell_object(ob->environment(), msg);

    DESTRUCT_OB(ob, "CItem::destruct_me");
}

// 当移动到容器中时呼叫
void CItem::init()
{
    if (query_amount() > 1) {
        if (!query_attr(CAN_COMBINE)) {
            if (userp(environment())) {
                char msg[255];
                environment()->move(load_room("jy"));
                g_channel.do_channel(
                    this, NULL, "news",
                    snprintf(msg, 255,
                             "$HIR%s(%s)因涉嫌复制而被捕入狱，判处监禁一周。",
                             environment()->name(1), environment()->id(1)));
                LOG_ERROR("%s(%s)拥有复制道具%s%d个.", environment()->name(1),
                          environment()->id(1), name(1), query_amount());
            }

            set("数目", query_amount());
            set("复制人", environment()->id(1));
            set_amount(1);
            move(load_room("pk"));
        }
    }
    // 将C++重量同步到_M镜像字段(供存盘/客户端显示)。
    // 不再强制1000重量下限:负重不变量由set_amount统一维护。
    set("weight", query_weight());
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:do_use(me, target) -> number
int l_do_use(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CItem* item = dynamic_cast<CItem*>(obj);
    if (!item) return 0;

    CChar* me          = NULL;
    CContainer* target = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (lua_isuserdata(L, 3))
        target = dynamic_cast<CContainer*>(get_object(L, 3));

    lua_pushinteger(L, item->do_use(me, target));
    return 1;
}

// self:do_action_base(nAct, me) -> number - 调用基类 CItem::Do_Action（绕过 Lua
// 重写）
int l_do_action_base(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    int act   = (int)luaL_checkinteger(L, 2);
    CChar* me = NULL;
    if (lua_isuserdata(L, 3)) me = dynamic_cast<CChar*>(get_object(L, 3));

    CLuaEquip* e = dynamic_cast<CLuaEquip*>(obj);
    if (e) {
        lua_pushinteger(L, e->lua_do_action_base(act, me));
        return 1;
    }

    // 非 equip 派生（CItem 系）直接调用其 Do_Action
    CItem* item = dynamic_cast<CItem*>(obj);
    if (item) {
        lua_pushinteger(L, item->do_action(act, me));
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

// self:set_amount(n)
int l_set_amount(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CItem* item = dynamic_cast<CItem*>(obj);
    if (!item) return 0;
    int32_t n = (int32_t)luaL_checkinteger(L, 2);
    item->set_amount(n);
    return 0;
}

// self:Split(n) -> object - 从物品中分出一部分，返回新物品
int l_split(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CItem* item = dynamic_cast<CItem*>(obj);
    if (!item) return 0;
    int32_t n = (int32_t)luaL_checkinteger(L, 2);
    lua_bind_push_object(L, item->split(n));
    return 1;
}
