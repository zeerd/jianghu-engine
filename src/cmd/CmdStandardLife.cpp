#include "CmdStandard.h"
#include "../char/CharRules.h"
#include "../room/Room.h"
#include "../item/FightRoom.h"
#include "../engine/Channel.h"
#include "../combat/CombatConfig.h"
#include "../interfaces/IActionHandler.h"
#include "../interfaces/IDialogSource.h"
#include "../interfaces/IClientObject.h"

void wakeup1(CContainer* ob, int32_t param1, int32_t param2)
{
    CChar* me = static_cast<CChar*>(ob);

    if (!me) return;

    me->set("hp", me->query("eff_hp"));
    if (me->query("mp") < me->query("max_mp"))
        me->set("mp", me->query("max_mp"));

    me->enable_player();

    while ((me->environment())->is_character())
        me->move((me->environment())->environment());

    message_vision("$N一觉醒来，精力充沛地活动了一下筋骨。", me);
    me->delete_temp("block_msg/all");
    tell_object(me, "你一觉醒来，只觉精力充沛。该活动一下了。");
    me->flush_my_info();
}

int CCmdStandard::valid_move(CChar* me)
{
    if (me->over_encumbranced()) return notify_fail("你的负荷过重，动弹不得。");

    if (me->is_busy()) return notify_fail("你的动作还没有完成，不能移动。");

    if (me->query_temp("no_move")) return notify_fail("你现在不能移动！");

    return 1;
}

int CCmdStandard::go_door(CChar* me, int32_t target, char* arg)
{
    CContainer* env = me->environment();  //, *obj;
    CDoor* door;
    char msg[255];

    if (!valid_move(me)) return 0;

    notify_fail("你要往哪个方向走？");
    if (!env->query_attr(CChar::IS_ROOM)) return 0;

    door = static_cast<CRoom*>(env)->present_door(arg);
    if (!door) return 0;

    if (!static_cast<CRoom*>(env)->valid_leave(me, door->querystr("id")))
        return 0;

    if (door->enter_door(me)) {
        me->delete_temp("pending");
        if (me->query_temp("party/follow")) {
            CChar* who = (CChar*)find_online(me->querystr_temp("party/master"));
            if (who
                && who->query_temp(snprintf(msg, 255, "party/friend_%s",
                                            me->querystr("id")))) {
                tell_object(
                    who, snprintf(msg, 255, "$HIR%s(%s)取消了与你的组队跟随。",
                                  me->name(), me->id()));
            }
            me->delete_temp("party/follow");
            tell_object(me, snprintf(msg, 255, "你取消了与%s的组队跟随。",
                                     who->name()));
        }

        // 旧环境内的物件跟随
        DTItemList search;
        CContainer* obj;

        copy_list(&search, env->get_item_list());

        for (auto val : search) {
            obj = val;
            if (!is_living(obj)) continue;
            if (obj == me) continue;
            if (obj->environment() != env) continue;  // 已经移动
            // 检查队伍中的跟随情况
            if (me->query_temp("party/number")
                && me->query_temp(
                    snprintf(msg, 255, "party/friend_%s", obj->querystr("id")))
                && obj->query_temp("party/follow")) {
                if (!valid_move(static_cast<CChar*>(obj))) {
                    snprintf(msg, 255, "我们对%s不抛弃不放弃。", obj->name());
                    message_vision_room(env, "room", msg);
                }
                if (door->enter_door(static_cast<CChar*>(obj))) {
                    obj->delete_temp("pending");
                }
            }
            else
                static_cast<CChar*>(obj)->follow_me(me, arg);
        }

        search.clear();
        return 1;
    }

    return 0;
}

int CCmdStandard::Sleep(CChar* me, int32_t target, char* arg)
{
    CContainer* where = me->environment();

    if (!where->query("sleep_room") && !me->query_temp("force_sleep")
        && !char_rule_yes("sleep_anywhere", me, nullptr))
        return notify_fail("这里不是睡觉的地方。");

    if (me->is_busy()) return notify_fail("你正忙着呢！");

    if (me->is_fighting()) return notify_fail("战斗中不能睡觉！");

    if ((g_current_time - me->query("last_sleep")) < 90)
        return notify_fail("你刚睡过一觉, 先活动活动吧。 ");

    if (!target || target == me->object_id()) {
        me->disable_player("睡梦中");
        int rnd;

        if (where->query("sleep_room")) {
            tell_object(me,
                        "你往被中一钻，开始睡觉。\n不一会儿，你就进入了梦乡。");
            me->set("last_sleep", g_current_time);
            me->set_temp("block_msg/all", 1);
            message_vision("$N往被中一钻，不一会便鼾声大作，进入了梦乡。", me);
            rnd = g_utils.random((60 - me->query_con()) / 3);
        }
        else {
            tell_object(me,
                        "你就地一躺，开始睡觉。\n不一会儿，你就进入了梦乡。");
            me->set("last_sleep", g_current_time);
            me->set_temp("block_msg/all", 1);
            message_vision("$N就地一躺，不一会便鼾声大作，进入了梦乡。", me);
            rnd = 10 + g_utils.random((100 - me->query_con()) / 3);
        }

        if (rnd < 1) rnd = 0;
        me->call_out(wakeup1, rnd + 10);
        me->delete_temp("nextday");

        return 1;
    }

    char msg[255];
    CChar* obj;

    if (!(obj = get_room_char(me, target)))
        return notify_fail("你想跟谁一起睡？");

    if (!is_user(obj)) return notify_fail("人家可不愿理你！");

    // can't sleep with npc...

    if (!(EQUALSTR(me->querystr("gender"), "男性")
              && EQUALSTR(obj->querystr("gender"), "女性")
          || EQUALSTR(me->querystr("gender"), "女性")
                 && EQUALSTR(obj->querystr("gender"), "男性")))
        return notify_fail("你们没法睡哦！");

    if (!obj->query_attr(CChar::IS_LIVING))
        return notify_fail("%s没法跟你睡了。", obj->name());

    if (!where->query("sleep_room")) return notify_fail("就在这里？不太好吧。");

    if (obj->is_busy()) return notify_fail("对方正忙着呢！");

    if (me->query("hp") * 100 / me->query("max_hp") < 50)
        return notify_fail("你现在是心有余而力不足。");

    if (me->querymap("condition") && !me->query_condition("wc_poison"))
        return notify_fail(
            "你的呼吸慢慢变的急促起来，但现在中毒在身，看起来是心有余而力不"
            "足。");

    if (obj->querymap("condition") && !obj->query_condition("wc_poison"))
        return notify_fail(
            "对方的呼吸慢慢变的急促起来，但对方看起来象是中毒在身，实在是心"
            "有余而力不足。");

    if (obj->query_temp("apply/index"))
        return notify_fail(
            "对方穿的这么整齐，好象是要打算出门去的吧，现在还有空睡觉吗？");

    if (me->query_temp("apply/index"))
        return notify_fail("你穿的这么整齐，是去上街还是要睡觉呢？");

    if (is_user(obj) && obj->query_temp("pending/sleep") != me->object_id()) {
        message_vision("\n$RED$N含情脉脉地对$n说：我．．．\n\n", me, obj);

        me->set_temp("pending/sleep", obj->object_id());
        tell_object(
            obj,
            snprintf(msg, 255,
                     "$YEL看来%s(%s)很想跟你．．．如果你愿意，请与其交谈。",
                     me->name(), me->id()));
        tell_object(me, "$YEL对方正在考虑中．．．\n");

        return 1;
    }

    message_vision("$RED\n$N冲着$n会心地一笑，点了点头。\n\n", me, obj);

    me->command("unwield 0 all");
    obj->command("unwield 0 all");

    me->update_me();
    obj->update_me();

    if (EQUALSTR(me->querystr("gender"), "男性")) {
        tell_object(
            me,
            snprintf(msg, 255,
                     "$HIY\n\n你搂着%s温软的身体，不由得心醉神迷．．．\n\n\n",
                     obj->name()));
        tell_object(
            obj, snprintf(msg, 255,
                          "$HIY\n\n你躺在%s的怀里，不由得心醉神迷．．．\n\n\n",
                          me->name()));
    }
    else {
        tell_object(
            obj,
            snprintf(msg, 255,
                     "$HIY \n\n你搂着%s温软的身体，不由得心醉神迷．．．\n\n\n",
                     me->name()));
        tell_object(
            me, snprintf(msg, 255,
                         "$HIY\n\n你躺在%s的怀里，不由得心醉神迷．．．\n\n\n",
                         obj->name()));
    }

    me->set("last_sleep", g_current_time);
    obj->set("last_sleep", g_current_time);
    me->set_temp("block_msg/all", 1);
    obj->set_temp("block_msg/all", 1);
    message_vision("$HIY $N和$n钻入被中，搂在一起睡着了．．．\n\n\n", me, obj);

    me->disable_player("睡梦中");
    obj->disable_player("睡梦中");

    me->call_out(wakeup2, g_utils.random(45 - me->query_con()) + 10,
                 obj->object_id());
    obj->call_out(wakeup2, g_utils.random(45 - obj->query_con()) + 10,
                  me->object_id());

    return 1;
}

void CCmdStandard::wakeup2(CContainer* ob, int32_t param1, int32_t param2)
{
    CChar* me = static_cast<CChar*>(ob);

    int kee_cost = 50 + 5 * (me->query("str") - me->query("con"));

    if (kee_cost > me->query("max_hp") / 2)
        kee_cost = me->query("max_hp") / 2 + 1;

    if (me->query("mp") < me->query("max_mp"))
        me->set("mp", me->query("max_mp"));

    me->add("max_atman", 1);
    me->add("hp", -kee_cost);

    me->enable_player();

    while (me->environment()->is_character())
        me->move((me->environment())->environment());

    // sleep后,春毒自解
    me->apply_condition("wc_poison", 0);

    if (EQUALSTR(me->querystr("gender"), "男性")) {
        message_vision("$HIY$N醒了过来，似乎还沉浸在幸福之中。", me);
        me->set_temp("block_msg/all", 0l);
        tell_object(me, "你醒了过来，似乎还沉浸在幸福之中。");
        return;
    }

    message_vision("$HIY$N醒了过来，脸上还挂着甜蜜的笑容。", me);
    me->set_temp("block_msg/all", 0l);
    tell_object(me, "你醒了过来，脸上还挂着甜蜜的笑容。");
}

int CCmdStandard::exercise(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_exercise的场景使用Exercise命令
    CContainer* env = me->environment();
    if ((env->query("no_exercise") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止练功。");

    int hp_cost, busy_time;

    if ((me->environment())->query("no_fight"))
        return notify_fail("安全区内禁止练功。");

    if (me->is_busy() || me->query_temp("pending/exercising"))
        return notify_fail("你现在正忙着呢。");

    if (me->is_fighting()) return notify_fail("战斗中不能练内功，会走火入魔。");

    if (!strlen(me->query_skill_mapped("force")))
        return notify_fail("你必须先用 enable 选择你要用的内功心法。");

    hp_cost = me->query("env/exercise");
    if (!hp_cost)
        return notify_fail("你要花多少气练功？用set exercise 10设置。");
    if (hp_cost < 10)
        return notify_fail(
            "你最少要花 10 点「气」才能练功。用set exercise 10设置。");

    if (me->query("hp") < hp_cost)
        return notify_fail("你现在的气太少了，无法产生内息运行全身经脉。");

    tell_object(me, "你坐下来运气用功，一股内息开始在体内流动。", "exercise");

    busy_time = hp_cost / 20;
    me->start_busy(busy_time + 1);
    me->set_temp("pending/exercising", 1);
    me->set_temp("exercise_cost", hp_cost);

    message_channel("exercise", "$N盘膝坐下，开始修炼内力。", me);
    me->start_busy(exercising, halt_exercise);
    me->send_state();
    return 1;
}

int CCmdStandard::exercising(CChar* me)
{
    int exercise_cost = me->query_temp("exercise_cost");
    int neili_gain    = 1 + me->query_skill("force") / 5;
    CContainer* where = me->environment();

    if (exercise_cost < neili_gain) neili_gain = exercise_cost;
    if (exercise_cost < 1) {
        if (me->query("mp") > me->query("max_mp") * 2)
            me->set("mp", me->query("max_mp") * 2);

        return 0;
    }

    if (me->query("hp") < neili_gain) {
        message_channel("exercise", "$N一口真气没上来，终止了打坐。", me);

        // 防止内力过高
        if (me->query("mp") > me->query("max_mp") * 2)
            me->set("mp", me->query("max_mp") * 2);

        me->send_state();
        me->flush_my_info();

        return 0;
    }

    me->add("mp", neili_gain);
    me->set_temp("exercise_cost", exercise_cost -= neili_gain);

    me->add("hp", -neili_gain);

    if (exercise_cost > 0) return 1;

    me->set_temp("pending/exercising", 0l);
    message_channel("exercise", "$N运功完毕，深深吸了口气，站了起来。", me);
    if (me->query("mp") >= me->query("max_mp") * 2) {
        if (me->query("max_mp")
            > me->query_skill("force") * 10 + me->query("reward/max_mp")) {
            tell_object(me, "你的内力修为似乎已经达到了瓶颈。", "exercise");
            me->set("mp", me->query("max_mp"));
        }
        else {
            me->add("max_mp", 1);
            me->set("mp", me->query("max_mp"));
            tell_object(me, "你的内力增加了！！", "exercise");
        }
    }

    me->send_state();
    me->flush_my_info();
    return 0;
}

int CCmdStandard::halt_exercise(CChar* me, CChar* who, char* how)
{
    if (me->query("mp") > me->query("max_mp") * 2)
        me->set("mp", me->query("max_mp") * 2);
    me->set_temp("pending/exercising", 0l);

    // 显示中止原因信息
    if (strcmp(how, "halt") == 0)
        message_vision("$N把正在运行的真气强行压回丹田，站了起来。", me);
    else if (strcmp(how, "kill") == 0)
        message_vision("受到$n攻击，$N强行压下一口真气，站了起来！", me, who);
    else
        message_vision("$N一口真气没上来，终止了打坐。", me);

    me->flush_my_info();
    me->send_state();
    return 1;
}

int CCmdStandard::say(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[512], names[40];

    if (!strlen(arg)) {
        message_vision("$HIC$N自言自语不知道在说些什么。", me);
        return 1;
    }

    if (!g_channel.check_channel_msg(me, "say", arg, 0)) return 0;

    // 禁止在设置为no_say的场景使用Say命令
    CContainer* env = me->environment();
    if ((env->query("no_say") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止说话。");

    if (me->query("no_say") || me->query_temp("no_say"))
        return notify_fail("由于发送了大量信息，你的说话权利被禁止了。");

    who = get_room_char(me, target);
    if (!who || who == me)
        strcpy(names, "$HIC$N说道：");
    else
        strcpy(names, "$HIC$N对$n说道：");

    if (me->query("hp") < me->query("max_hp") / 5) {
        replace_string(arg, " ", " ... ", 400);
        snprintf(msg, 512, "%s%s ...", names, arg);
    }
    else
        snprintf(msg, 512, "%s%s", names, arg);

    if (me == who)
        message_vision(msg, me);
    else {
        message_vision(msg, me, who);

        // who对say作出反应
        if (who) who->relay_say(me, arg);
    }

    return 1;
}

int CCmdStandard::look(CChar* me, int32_t target, char* arg)
{
    CContainer* who;

    if (target) {
        if (!(who = get_room_object(me, target))
            && !(who = get_self_object(me, target)))
            return notify_fail("你要观察什么？");
    }
    else
        who = me->environment();

    if (!dynamic_cast<IClientObject*>(who)->do_look(me))
        dynamic_cast<IDialogSource*>(who)->send_menu(me);

    return 1;
}

int CCmdStandard::tell(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_tell的场景使用Tell命令
    CContainer* env = me->environment();
    if ((env->query("no_tell") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止私聊。");

    char msg[512];
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("没有这个人。。。");

    if (!is_living(who) || who->query_temp("net_dead"))
        return notify_fail("对方听不到你讲话。");

    char key[80];

    snprintf(key, 80, "notell/%s", me->querystr("id"));

    if (!is_wizard(me) && (who->query_temp(key) || who->query_temp("notell/all")))
        return notify_fail("%s不接受你的悄悄话。", who->name(1));

    if (!g_channel.check_channel_msg(me, "tell", arg, 0)) return 0;

    snprintf(msg, 512, "&C=%d&i=%s&n=%s&M=%s&", TELLOBJECT, me->querystr("id"),
             me->name(1), arg);
    who->send_command(msg, SEND_WAIT_BUFFERED);

    tell_object(
        me, snprintf(msg, 400, "$HIG【私聊】你告诉%s：%s", who->name(), arg));

    // 判断对方是否发呆中
    int idle;
    if (is_user(who) && (idle = static_cast<CUser*>(who)->query_idle()) > 180) {
        tell_object(me, snprintf(msg, 400,
                                 "$HIG%s已经发呆了%d秒，可能不能马上回答你哦～",
                                 who->name(), idle));
    }

    return 1;
}
