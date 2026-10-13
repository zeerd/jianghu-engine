/** @file Equip.cpp @brief implementation of the CEquip class. */
#include <windows.h>

#include "Equip.h"

/*
    对于又能吃又能装备的物品，我们可以把它先继承吃类，加上wield()私有函数，
    重载Do_Action，并指向其自己的wield()函数。另外还要重载Query(CAN_EQUIP)和
    Query(EQUIPPED)函数。使之可以装备。

    对于又能split，又能装备之物品，也这样处理。
*/
//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CEquip::CEquip() {}

CEquip::~CEquip() {}

int CEquip::can_equip(CChar* me)
{
    if (query("female_only") && EQUALSTR(me->querystr("gender"), "男性"))
        return 0;

    if (me->query("level") < query("equip_lvl"))  // 等级限制
        return 0;

    if (!querymap("apply"))  // 只有装备后加属性的才能装备
        return 0;

    return 1;
}

void CEquip::on_moved(CContainer* me) {}

// 装备
int CEquip::do_wield()
{
    CChar* owner;
    CChar::DTEquipList* list;
    int32_t pos, pos1 = -1, pos2 = -1, pos3 = -1;

    if (!environment()->query_attr(CChar::IS_CHAR)) return 0;

    owner = (CChar*)environment();
    // If already worn, just recognize it.
    if (query("equipped")) return 0;  // 已经装备了

    // 装备位置
    pos  = query("wield_position");
    pos3 = query("wield_secondary_position");
    if (pos3 == 0) {
        pos3 = -1;
    }
    if (pos == WIELD_NONE) return 0;  // 没有装备位置

    CMapping* m;
    m = query_entire_dbase();
    // 不是主人不能装备
    if (!m->undefinedp("owner")
        && DIFFERSTR(querystr("owner"), owner->querystr("id")))
        return 0;

    if (pos == WIELD_HAND) {
        pos2 = WIELD_LEFT_HAND;
        pos1 = WIELD_RIGHT_HAND;
    }
    else if (pos == WIELD_ERR) {
        pos2 = WIELD_LEFT_ERR;
        pos1 = WIELD_RIGHT_ERR;
    }
    else if (pos == WIELD_WRIST) {
        pos2 = WIELD_LEFT_WRIST;
        pos1 = WIELD_RIGHT_WRIST;
    }
    else {
        pos1 = pos;
    }

    pos = -1;
    if (pos == -1 && pos1 != -1) {
        if (!owner->query_equip(pos1))  // 右手为空，可以装备
            pos = pos1;
    }

    if (pos == -1 && pos2 != -1) {
        if (!owner->query_equip(pos2))  // 左手为空，可以装备
            pos = pos2;
    }

    if (pos == -1 && pos3 != -1) {
        if (!owner->query_equip(pos3))  // 特殊位为空，可以装备
            pos = pos3;
    }

    if (pos == -1 || owner->query_equip(pos)) return 0;

    // 可以装备到nPos上
    list         = owner->get_equip_list();
    (*list)[pos] = object_id();

    char key[80];
    m = querymap("apply");

    if (m) {
        // 修正数值属性
        CVector v;

        m->keys(v);

        for (int i = 0; i < v.count(); i++) {
            CMapping::svalue_t* sv;
            sv = m->find_string_in_mapping(v[i]);

            if (sv->type == T_NUMBER)
                owner->add_temp(snprintf(key, 80, "apply/%s", v[i]),
                                sv->u.number);
            else
                owner->set_temp(snprintf(key, 80, "apply/%s", v[i]),
                                sv->u.string.get());
        }
    }

    set("equipped", pos);  // 装备位置

    return 1;
}

// 卸下装备
int CEquip::do_un_wield()
{
    CChar* owner;
    CChar::DTEquipList* list;

    if (!environment()->query_attr(CChar::IS_CHAR)) return 0;
    if (!query("equipped")) return 0;  // 并未装备

    owner = (CChar*)environment();

    // 去掉角色装备
    list = owner->get_equip_list();
    for (CChar::iterator_equip p = list->begin(); p != list->end(); p++) {
        if (p->second == object_id()) {
            list->erase(p);
            break;
        }
    }

    // 去掉修正
    CMapping* m = querymap("apply");
    char key[80];

    if (m) {
        // 修正数值属性
        CVector v;

        m->keys(v);

        for (int i = 0; i < v.count(); i++) {
            CMapping::svalue_t* sv;
            sv = m->find_string_in_mapping(v[i]);

            if (sv->type == T_NUMBER)
                owner->add_temp(snprintf(key, 80, "apply/%s", v[i]),
                                -sv->u.number);
            else if (sv->type == T_STRING)
                owner->delete_temp(snprintf(key, 80, "apply/%s", v[i]));
        }
    }

    del("equipped");

    return 1;
}

int32_t CEquip::query_attr(int attr)
{
    switch (attr) {
        case CAN_EQUIPPED: {
            CContainer* me;

            if (!(me = environment()) || !me->query_attr(IS_CHAR))
                return 0;  // 非角色不能装备
            return can_equip(static_cast<CChar*>(me));
        }
        case IS_EQUIP:
            return 1;
    }

    return CItem::query_attr(attr);
}

// 设置装备修正值
int CEquip::do_action(int act, CChar* me)
{
    // 判断有否主人
    if (!environment()) {
        char tmp[255];
        snprintf(tmp, 255, "%s执行装备失败。重量：%ld，无环境。\n", name(),
                 query_weight());
        LOG_ERROR("%s", tmp);
        return 0;
    }

    if (act == CChar::DO_WIELD && query_attr(CAN_EQUIPPED)) return do_wield();

    if (act == CChar::DO_UNWIELD) return do_un_wield();

    return CItem::do_action(act, me);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:Do_Wield() -> number - 调用 CEquip 的 protected Do_Wield（供 do_action
// 脚本使用）
int l_do_wield(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CLuaEquip* e = dynamic_cast<CLuaEquip*>(obj);
    if (e) {
        lua_pushinteger(L, e->lua_do_wield());
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

// self:Do_UnWield() -> number - 调用 CEquip 的 protected Do_UnWield（供
// do_action 脚本使用）
int l_do_unwield(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CLuaEquip* e = dynamic_cast<CLuaEquip*>(obj);
    if (e) {
        lua_pushinteger(L, e->lua_do_un_wield());
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}
