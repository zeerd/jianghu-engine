/**
 * @file AutoFight.cpp
 * @brief NPC aggression AI / auto-fight triggers */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "AutoFight.h"
#include "Container.h"
#include "Char.h"
#include "Channel.h"
#include "Rank.h"
#include "Login.h"
#include "Message.h"

CAutoFight g_auto_fight;

void CAutoFight::auto_fight(CChar* me, CChar* obj, std::string type)
{
    CALLOUTFUNC func;

    if (type == "berserk")
        func = start_berserk;
    else if (type == "hatred")
        func = start_hatred;
    else if (type == "vendetta")
        func = start_vendetta;
    else
        func = start_aggressive;

    // Don't let NPC autofight NPC.
    if (!is_user(me) && !is_user(obj)) return;

    if (me->query_temp("looking_for_trouble")) return;
    me->set_temp("looking_for_trouble", 1);

    // This call_out gives victim a chance to slip trough the fierce guys.
    me->call_out(func, 5 + g_utils.random(5), obj->object_id());
}

void CAutoFight::start_berserk(CContainer* ob, int32_t target, int32_t param2)
{
    char msg[255];
    CChar* me  = static_cast<CChar*>(ob);
    CChar* obj = static_cast<CChar*>((me->environment())->present(target));
    int shen;

    if (!obj) return;

    me->set_temp("looking_for_trouble", 0l);

    if (me->is_fighting(obj) || !me->query_attr(CChar::IS_LIVING)
        || (me->environment())->query("no_fight")  // Are we in a peace room?
    )
        return;

    shen = 0 - me->query("repute");
    message_vision("$N用一种异样的眼神扫视著在场的每一个人。", me);

    if (!is_user(me) || me->query("mp") > (g_utils.random(shen) + shen) / 10)
        return;
    // 如果是npc或者内力大于shen的一个随机变化范围除于10，也就是说shen相比较内力而言太底了
    // 不能压制住杀气！
    if (shen > me->query("score") && !is_wizard(obj)) {
        message_vision(
            snprintf(msg, 255, "$N对著$n喝道：%s看你实在很不顺眼，去死吧。",
                     query_self_rude(me)),
            me, obj);
        me->kill_ob(obj);
    }
    else {
        message_vision(
            snprintf(msg, 255,
                     "$N对著$n喝道：喂！%s%s正想找人打架，陪我玩两手吧！",
                     query_rude(obj), query_self_rude(me)),
            me, obj);
        me->fight_ob(obj);
    }
}

void CAutoFight::start_hatred(CContainer* ob, int32_t target, int32_t param2)
{
    static char catch_hunt_msg[][80] = {
        {
            "$N和$n仇人相见分外眼红，立刻打了起来！",
        },
        {
            "$N对著$n大喝：「可恶，又是你！」",
        },
        {
            "$N和$n一碰面，二话不说就打了起来！",
        },
        {
            "$N一眼瞥见$n，「哼」的一声冲了过来！",
        },
        {
            "$N一见到$n，愣了一愣，大叫：「我宰了你！」",
        },
        {
            "$N喝道：「$n，我们的帐还没算完，看招！」",
        },
        {"$N喝道：「$n，看招！」"},
    };

    CChar* me  = static_cast<CChar*>(ob);
    CChar* obj = static_cast<CChar*>((me->environment())->present(target));
    if (!obj) return;  // Are we still exist( not becoming a corpse )?

    me->set_temp("looking_for_trouble", 0l);

    if (me->is_fighting(obj)
        || !me->query_attr(CChar::IS_LIVING)  // Are we capable for a fight?
        || (me->environment())->query("no_fight")  // Are we in a peace room?
    )
        return;

    // We found our hatred! Charge!
    message_vision(catch_hunt_msg[g_utils.random(7)], me, obj);
    me->kill_ob(obj);
}

void CAutoFight::start_vendetta(CContainer* ob, int32_t target, int32_t param2)
{
    CChar* me  = static_cast<CChar*>(ob);
    CChar* obj = static_cast<CChar*>((me->environment())->present(target));

    if (!obj) return;  // Are we still exist( not becoming a corpse )?

    me->set_temp("looking_for_trouble", 0l);

    if (me->is_fighting(obj)                  // Are we busy fighting?
        || !me->query_attr(CChar::IS_LIVING)  // Are we capable for a fight?
        || (me->environment())->query("no_fight")  // Are we in a peace room?
    )
        return;

    // We found our vendetta opponent! Charge!
    me->kill_ob(obj);
}

void CAutoFight::start_aggressive(CContainer* ob, int32_t target,
                                  int32_t param2)
{
    CChar* me  = static_cast<CChar*>(ob);
    CChar* obj = static_cast<CChar*>((me->environment())->present(target));

    if (!obj) return;  // Are we still exist( not becoming a corpse )?

    me->set_temp("looking_for_trouble", 0l);

    if (me->is_fighting(obj)                  // Are we busy fighting?
        || !me->query_attr(CChar::IS_LIVING)  // Are we capable for a fight?
        || (me->environment())->query("no_fight")  // Are we in a peace room?
    )
        return;

    // We got a nice victim! Kill him/her/it!!!
    me->kill_ob(obj);
}
