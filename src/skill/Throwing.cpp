/**
 * @file Throwing.cpp
 * @brief implementation of the CThrowing class. */

#include <windows.h>

#include "Throwing.h"
#include "WeaponEffect.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CThrowing::CThrowing() {}

CThrowing::~CThrowing() {}

void CThrowing::create()
{
    set_name("基本投掷");

    Magic_t* magic;

    magic             = add_magic("sneak", do_sneak);
    magic->name       = "偷袭";
    magic->mp         = 30;
    magic->target     = CAST_TO_OTHER;
    magic->valid_cast = CAST_IN_BOTH;
}

CMapping* CThrowing::query_action(CChar* me)
{
    static CMapping m_actions;

    m_actions.set("action", "$N扬起$w向$n投去。");
    return &m_actions;
}

CContainer* CThrowing::get_first_throwing_weapon(CChar* me, const char* name)
{
    if (!me) return 0;

    CContainer* weapon = NULL;
    if ((weapon = me->query_weapon())
        && EQUALSTR(weapon->querystr("skill_type"), "throwing"))
        return weapon;

    weapon           = NULL;
    CContainer* obj  = NULL;
    DTItemList* list = me->get_item_list();
    for (auto val : *list) {
        obj = val;
        if ((obj != NULL)
            && EQUALSTR(obj->querystr("skill_type"), "throwing")) {
            if (weapon == NULL) {
                weapon = obj;
            }
            if (name == NULL) {
                break;
            }
            else {
                if (EQUALSTR(obj->querystr("id"), name)) {
                    weapon = obj;
                    break;
                }
            }
        }
    }

    return weapon;
}

int CThrowing::do_sneak(CChar* me, CContainer* ob)
{
    int sp, dp;
    CChar* target = NULL;
    CContainer* weapon;

    if (ob && ob->is_character()) target = static_cast<CChar*>(ob);

    if ((me->environment())->query("no_fight"))
        return notify_fail("这里禁止偷袭。");

    if (!target || target == me) return notify_fail("你想偷袭谁？");

    if (!wizardp(me) && wizardp(target))
        return notify_fail("玩家不能偷袭巫师。");

    if (userp(target)) {
        return notify_fail("不能偷袭玩家。");
    }

    if (!(weapon = get_first_throwing_weapon(me)))
        return notify_fail("没有暗器，怎么偷袭。");

    if (me->is_fighting() && target->is_fighting()
        && me->query_team() == target->query_team())
        return notify_fail("不能偷袭本方队员！");

    if (me->query("mp") < 100) return notify_fail("你的法力不足！");

    int poi = me->query_skill("poison", 1);

    char msg[255];
    tell_object(me,
                snprintf(msg, 255, "你不动声色地掏出一枚%s%s，准备偷袭%s ...",
                         ((poi > 0) ? "淬了毒的" : ""), weapon->name(1),
                         target->name()));

    if (me->is_fighting()) {
        sp = me->query_skill("throwing");
        sp = g_utils.random(sp);
        dp = target->query_skill("dodge");
        dp = dp / 2;
    }
    else {
        sp = me->query_skill("throwing");
        sp = g_utils.random(sp);
        dp = target->query_skill("dodge");
        dp = g_utils.random(dp / 2);
    }

    if (sp > dp) {
        if (me->is_fighting()) {
            target->receive_damage("hp", sp + dp, me);
            me->start_busy(2);
            me->add("mp", -100);
            message_vision(
                snprintf(msg, 255, "$p只觉一阵刺痛，受到了%d点暗器伤害！\n",
                         sp + dp),
                me, target);
            if (poi > 0) {
                target->apply_condition("feng_poison", sp - dp);
            }
        }
        else {
            // 此处不能有伤害，以免有人拿这个去磨BOSS。
            target->kill_ob(me);
            me->kill_ob(target);
            me->add("mp", -30);
            message_vision("$p只觉一阵刺痛，中暗器了！\n", me, target);
        }
    }
    else {
        tell_object(
            me, snprintf(msg, 255, "%s运气太好了，刚好一个侧身躲过了你的偷袭。",
                         target->name()));
        me->add("mp", -30);
        me->start_busy(1);
    }

    CWeaponEffect::throw_weapon(me, target, weapon, 0);

    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// get_first_throwing_weapon(me[, name]) -> object
// （CThrowing::get_first_throwing_weapon）
// 返回身上第一件暗器（可指定名字），无则返回 nil
int l_get_first_throwing_weapon(lua_State* L)
{
    CChar* me = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CChar*>(get_object(L, 1));
    if (!me) return 0;
    const char* name = lua_isstring(L, 2) ? luaL_checkstring(L, 2) : NULL;
    CContainer* w
        = CThrowing::get_first_throwing_weapon(me, name ? name : NULL);
    lua_bind_push_object(L, w);
    return 1;
}

// throw_weapon(me, victim, weapon[, damage])
// （CCombatd::throw_weapon：消耗暗器） CCombatd::throw_weapon
// 仅处理暗器数量消耗与"用尽"提示，不结算伤害
int l_throw_weapon(lua_State* L)
{
    CChar* me          = NULL;
    CChar* victim      = NULL;
    CContainer* weapon = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CChar*>(get_object(L, 1));
    if (lua_isuserdata(L, 2)) victim = dynamic_cast<CChar*>(get_object(L, 2));
    if (lua_isuserdata(L, 3))
        weapon = dynamic_cast<CContainer*>(get_object(L, 3));
    int32_t damage = (int32_t)luaL_optinteger(L, 4, 0);
    if (me && weapon) CWeaponEffect::throw_weapon(me, victim, weapon, damage);
    return 0;
}
