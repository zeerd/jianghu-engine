#include "CmdStandard.h"
#include "../item/Item.h"
#include "../npc/Npc.h"
#include "../interfaces/IActionHandler.h"

int CCmdStandard::get(CChar* me, int32_t target, char* arg)
{
    char cmd[255], szEnv[40], szNum[40];
    CContainer *who, *env, *obj = NULL;
    int32_t num = 0;
    int ap      = 30;
    CMapping* m;

    if (me->is_busy()) return notify_fail("你上一个动作还没有完成！");

    env = me->environment();
    if (strlen(arg)) {
        analyse_string(arg, szEnv, 40);
        analyse_string(szEnv, szNum, 40);
        if (atol(szEnv)) env = env->present(atol(szEnv));
        num = atol(szNum);
    };

    if (!env || EQUALSTR(env->querystr("status"), "locked")) return 0;
    if (env->query_attr(CChar::IS_FIGHTROOM) && !me->is_fighting()
        && !wizardp(me))
        return notify_fail(
            "你想拣战场里的东西，但是太危险了，你犹豫了一下，又退缩了。");

    if (living(env)
        && (me->query("grant") < 70
            || wiz_level(me) <= wiz_level(static_cast<CChar*>(env))))
        return notify_fail("你的巫师等级必须比对方高，才能搜身。");

    who = env->present(target);

    if (!check_ap(ap, me)) return 0;

    if (!who || !who->query_attr(CChar::CAN_GET))
        return notify_fail("你要拣什么。");

    if (env->query("sleep_room") && userp(who)
        && DIFFERSTR(env->querystr("area"), "house"))
        return notify_fail("这是客店，光天化日之下，你也敢背人？");

    if (userp(who) && who->query_temp("fb_nodie"))
        return notify_fail("非死亡副本，不允许背人。");

    m = who->query_entire_dbase();
    if (!m->undefinedp("no_get")
        && !(wizardp(me) && me->query("env/override"))) {
        strncpy(cmd, who->querystr("no_get"), 255);
        return notify_fail(cmd[0] ? cmd : "这个东西拿不起来。");
    }

    // 判断是否主人
    if (!m->undefinedp("owner") && !(wizardp(me) && me->query("env/override"))
        && DIFFERSTR(who->querystr("owner"), me->querystr("id")))
        return notify_fail("这不是你的东西。");

    if (num && who->query_attr(CChar::IS_ITEM)) {
        obj = static_cast<CItem*>(who)->split(num);
    }

    if (!obj) {
        obj = who;
        num = who->query_amount();
    }

    snprintf(cmd, 255, "%s拾起数量%d的%s。", me->name(), num, obj->name());

    if (obj->move(me))
        message_vision_room(me, "chat", cmd);
    else {
        notify_fail("此物你无法负担。");
        if (obj != who) {
            DESTRUCT_OB(obj, "cmd::get");
            static_cast<CItem*>(who)->add_amount(num);
        }

        return 0;
    }

    resume_ap(ap, me);

    if (me->is_fighting()) me->start_busy(2);
    return 1;
}

int CCmdStandard::use(CChar* me, int32_t target, char* arg)
{
    CContainer* who;
    CContainer* obj = me;

    // 禁止在设置为no_use的场景使用use命令
    CContainer* env = me->environment();
    if ((env->query("no_use") || env->query("no_all")) && userp(me))
        return notify_fail("这里禁止使用道具。");

    char szTarget[40];

    if (me->is_busy()) return notify_fail("你上一个动作还没有完成！");

    int ap = 50;
    if (!(who = get_self_object(me, target))) return 0;

    if (strlen(arg)) {
        analyse_string(arg, szTarget, 40);
        if (!(obj = get_room_object(me, atol(szTarget)))
            && !(obj = get_self_object(me, atol(szTarget))))
            return notify_fail("你要对谁使用？");
    }

    if (who->query_attr(CChar::CAN_USE) && check_ap(ap, me)) {
        CContainer* item = who;
        int32_t amount   = who->query_amount();
        if (amount > 1) item = static_cast<CItem*>(who)->split(1);

        int32_t objectid = item->object_id();

        if (static_cast<CItem*>(item)->do_use(me, obj)) {
            resume_ap(ap, me);

            if (g_last_destruct_object != objectid && who != item) {
                DESTRUCT_OB(item, "cmd::use");
                who->set_amount(amount);
            }

            if (me->is_fighting()) me->start_busy(2);
            return 1;
        }

        if (who != item) {
            DESTRUCT_OB(item, "cmd::use");
            who->set_amount(amount);
        }
    }

    return 0;
}

int CCmdStandard::wield(CChar* me, int32_t target, char* arg)
{
    char cmd[80];
    int ap = 25;
    CContainer* who;

    if (!(who = get_self_object(me, target))) return 0;

    // 禁止在设置为no_wield的场景使用wield命令
    CContainer* env = me->environment();
    if ((env->query("no_wield") || env->query("no_all")) && userp(me))
        return notify_fail("这里禁止装备道具。");

    if (me->is_busy()) return notify_fail("你正忙着");

    notify_fail("你无法装备这样东西。");
    if (who->query_attr(CChar::CAN_EQUIPPED) && check_ap(ap, me)
        && dynamic_cast<IActionHandler*>(who)->do_action(CChar::DO_WIELD)) {
        strncpy(cmd, who->querystr("wield_msg"), 80);
        if (!strlen(cmd)) {
            strncpy(cmd, who->querystr("wear_msg"), 80);
            if (!strlen(cmd)) strcpy(cmd, "$N装备$n。");
        }

        message_vision(cmd, me, who);
        resume_ap(ap, me);
        me->flush_my_equip();
        me->flush_my_info();

        if (me->is_fighting()) me->start_busy(2);
        return 1;
    }

    return 0;
}

int CCmdStandard::un_wield(CChar* me, int32_t target, char* arg)
{
    char cmd[80];
    int ap = 25;
    CContainer* who;

    // 禁止在设置为no_unwield的场景使用unwield命令
    CContainer* env = me->environment();
    if ((env->query("no_unwield") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止卸下道具。");

    if (me->is_busy()) return notify_fail("你正忙着。");

    if (strcmp(arg, "all") == 0)  // 无参数表示全部卸下
    {
        DTItemList search;
        CContainer* obj;

        copy_list(&search, me->get_item_list());
        for (auto val : search) {
            obj = val;
            if (check_ap(ap, me)
                && dynamic_cast<IActionHandler*>(obj)->do_action(
                    CChar::DO_UNWIELD)) {
                strncpy(cmd, obj->querystr("unwield_msg"), 80);
                if (!strlen(cmd)) strcpy(cmd, "$N放下手中的$n。");
                message_vision(cmd, me, obj);
                resume_ap(ap, me);
            }
        }
        me->flush_my_equip();
        me->flush_my_info();

        if (me->is_fighting()) me->start_busy(2);
        return 1;
    }

    if (!(who = get_self_object(me, target))) return 0;
    if (check_ap(ap, me)
        && dynamic_cast<IActionHandler*>(who)->do_action(CChar::DO_UNWIELD)) {
        strncpy(cmd, who->querystr("unwield_msg"), 80);
        if (!strlen(cmd)) {
            strncpy(cmd, who->querystr("remove_msg"), 80);
            if (!strlen(cmd)) strcpy(cmd, "$N放下手中的$n。");
        }

        message_vision(cmd, me, who);
        resume_ap(ap, me);
        me->flush_my_equip();
        me->flush_my_info();

        if (me->is_fighting()) me->start_busy(2);
        return 1;
    }
    return 0;
}

int CCmdStandard::do_drop(CChar* me, CContainer* obj, CContainer* env,
                          int32_t x, int32_t y)
{
    char msg[255];

    // 定位
    set_live_point(obj, x, y);

    int32_t amount = obj->query_amount();
    if (obj->move(env)) {
        if (obj->is_character()) {
            message_vision(
                snprintf(msg, 255, "$N将$n从背上放了下来，躺在%s里。",
                         env->name()),
                me, obj);
        }
        else {
            message_vision(snprintf(msg, 255, "$N向%s里丢下数量%d的$n。",
                                    env->name(), amount),
                           me, obj);
            if (!obj->query("value")) {
                tell_object(
                    me, "因为这样东西并不值钱，所以人们并不会注意到它的存在。");
                DESTRUCT_OB(obj, "cmd::drop");
            }
        }
        return 1;
    }
    return 0;
}

int CCmdStandard::drop(CChar* me, int32_t target, char* arg)
{
    CContainer *env, *who;
    char szTo[80], szY[80], szEnv[40], szNum[20];
    int ap      = 30;
    int32_t num = 0;
    int32_t x = 0, y = 0;
    CContainer* obj = 0;
    CMapping* m;

    env = me->environment();

    // 禁止在设置为no_drop的场景使用Drop命令
    if ((env->query("no_drop") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止丢弃物品。");

    if (strlen(arg)) {
        analyse_string(arg, szY, 80);
        analyse_string(szY, szTo, 80);
        analyse_string(szTo, szEnv, 40);
        analyse_string(szEnv, szNum, 20);

        if (atol(szEnv)) env = env->present(atol(szEnv));
        num = atol(szNum);

        x = atol(arg);
        y = atol(szY);
    }

    if (!env || EQUALSTR(env->querystr("status"), "locked")) return 0;

    if (!(who = get_self_object(me, target))) return 0;

    if (!x || !y)  // 无坐标，得丢弃者的目的坐标
    {
        x = me->query_temp("map/ex");
        y = me->query_temp("map/ey");
    }

    m = who->query_entire_dbase();
    if (!m->undefinedp("no_drop")) {
        const char* ptr = m->querystr("no_drop");
        return notify_fail(ptr[0] ? ptr : "这样东西不能随意丢弃。");
    }

    if (me->is_busy()) return notify_fail("你正忙着。");

    // 判断房间里是否还能容纳
    DTItemList* list = env->get_item_list();
    if (list->size() >= (size_t)env->query("max_items"))
        return notify_fail("%s里已经够拥挤了。", env->name());

    if (!check_ap(ap, me)) return 0;

    if (!num) num = 1;
    if (who->query_attr(CChar::IS_ITEM)) {
        obj = static_cast<CItem*>(who)->split(num);
    }

    if (!obj) obj = who;

    if (obj->is_character()) {
        LOG_INFO("%s(%s) drop %s(%s) into %s.", me->name(1), me->id(1),
                 obj->name(1), obj->id(1), env->name(1));

        if (env->query("no_drop_man") && !wizardp(me))
            return notify_fail("空间太小，此物无法容纳。");
    }

    if (!do_drop(me, obj, env, x, y)) {
        // 把东西再还给本人。
        if (obj->environment() != me) obj->move(me);
        return 0;
    }

    resume_ap(ap, me);
    return 1;
}

int CCmdStandard::do_give(CChar* me, CContainer* obj, CChar* who)
{
    char cmd[255];

    if (who->query("env/no_accept"))
        return notify_fail("对方好象不愿意收下你的东西。");

    int32_t objid = obj->object_id();
    snprintf(cmd, 255, "$N给$n数量%ld的%s。", obj->query_amount(), obj->name());

    if (who->query_attr(CChar::IS_NPC)) {
        if (!static_cast<CNpc*>(who)->accept_object(me, obj)) {
            return notify_fail("对方不要你的东西。");
        }
    }

    message_vision(cmd, me, who);

    if (g_last_destruct_object == objid)  // 已经被销毁
    {
        me->save();
        return 1;
    }

    if (obj && obj->move(who)) {
        me->save();
        return 1;
    }

    return 0;
}

int CCmdStandard::give(CChar* me, int32_t target, char* arg)
{
    CContainer *env, *who;
    CChar* Target;
    char szTarget[40], szNum[20];
    int ap          = 30;
    int32_t num     = 0;
    CContainer* obj = 0;

    if (!(env = me->environment())) return 0;
    if (!strlen(arg)) return 0;

    analyse_string(arg, szTarget, 40);
    analyse_string(szTarget, szNum, 20);

    if (!(Target = get_room_char(me, atol(szTarget))) || Target == me) return 0;
    num = atol(szNum);

    if (me->is_busy()) return notify_fail("你正忙着。");

    if (!living(Target)) return notify_fail("对方没办法接受你的礼物。");

    if (!check_ap(ap, me)) return 0;

    if (!num) num = 1;

    if (!(who = get_self_object(me, target))) return 0;

    if (who->query_attr(CChar::IS_USER))
        return notify_fail("送人？你有何居心？");

    CMapping* m = who->query_entire_dbase();
    if (!m->undefinedp("no_give")
        && !(wizardp(me) && me->query("env/override"))) {
        const char* ptr = m->querystr("no_give");
        return notify_fail(ptr[0] ? ptr : "这样东西不能随意给人。");
    }

    if (!m->undefinedp("no_drop")
        && !(wizardp(me) && me->query("env/override")))
        return notify_fail("这样东西不能随便给人。");

    // 判断是否主人
    if (!m->undefinedp("owner") && !(wizardp(me) && me->query("env/override"))
        && DIFFERSTR(who->querystr("owner"), Target->querystr("id")))
        return notify_fail("这不是%s的东西。", Target->name(1));

    if (!m->undefinedp("obj") && !(wizardp(me) && me->query("env/override"))
        && DIFFERSTR(who->querystr("obj"), Target->querystr("id")))
        return notify_fail("这不是%s的东西。", Target->name(1));

    if (who->query_attr(CChar::IS_ITEM)) {
        obj = static_cast<CItem*>(who)->split(num);
    }

    if (!obj) obj = who;

    if (!do_give(me, obj, Target)) {
        // 把东西再还给本人。
        if (obj->environment() != me) obj->move(me);
        return 0;
    }

    resume_ap(ap, me);
    return 1;
}
