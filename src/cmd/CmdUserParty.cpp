#include "CmdUser.h"

int CCmdUser::party(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_invite的场景使用Talk命令
    CContainer* env = me->environment();
    if ((env->query("no_invite") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止邀请他人。");

    CChar* who;
    char msg[80];

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要邀请谁加入你的队伍？");

    if (who == me) return notify_fail("你对自己说：算了还是自己邀请自己吧。");

    if (me->querystr_temp("party/master")[0])
        return notify_fail(
            "你现在正在与别人队伍中，要想自己组建队伍需要先行离队。");

    if (me->query_temp("party/number") >= CPartyConfig::max_members())
        return notify_fail("已经邀请了这么多的队友，足够闯荡江湖了吧？");

    if (EQUALSTR(who->querystr_temp("party/master"), me->querystr("id")))
        return notify_fail("对方已经接受了你的邀请。");

    message_vision("$HIC$N热情的对$n说：朋友，一起组队行走江湖吗？", me, who);
    me->set_temp("party/invite", who->querystr("id"));

    // 增加菜单指令
    // 注意，此处不能用$1代替自己。因为对方未必选中的是你。
    char tmp[40];

    me->add_menu_item(snprintf(msg, 80, "接受%s的组队邀请", me->name()),
                      snprintf(tmp, 40, "$0accept_party %ld", me->object_id()),
                      who);

    me->add_menu_item("拒绝组队邀请", "", who);
    me->send_menu(who);

    return 1;
}

// 接受组队邀请
int CCmdUser::accept_party(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[40];

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要接受谁的邀请？");

    if (who == me) return notify_fail("你对自己说：算了还是自己邀请自己吧。");

    if (me->querystr_temp("party/master")[0])
        return notify_fail("你现在已经和别人组队过了。");

    if (DIFFERSTR(who->querystr_temp("party/invite"), me->querystr("id")))
        return notify_fail("对方并没有邀请你。");

    if (me->querystr_temp("party/invite")[0])
        return notify_fail(
            "你已经对其他人发出了组队邀请，暂时不能接受别人的组队邀请。");

    message_vision("$HIC$N接受了$n的组队邀请。", me, who);
    me->set_temp("party/master", who->querystr("id"));
    me->delete_temp("party/follow");
    me->set_leader(who);
    who->delete_temp("party/invite");
    who->add_temp("party/number", 1);
    who->set_temp(snprintf(msg, 40, "party/friend_%s", me->querystr("id")), 1);

    who->say("你接受了组队邀请。", me);
    who->send_menu(me);

    return 1;
}

// 离开队伍
int CCmdUser::partyleave(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (!me->querystr_temp("party/master")
        || !me->querystr_temp("party/master")[0]) {
        return notify_fail("你并没有加入别人的队伍。");
    }

    if (!(who = find_online(me->querystr_temp("party/master")))) {
        me->delete_temp("party/master");
        me->delete_temp("party/follow");
        me->set_leader(me);
        return notify_fail("队长已不在线，你可以离队。");
    }

    if (!(who = find_player(target)))
        return notify_fail("你要解除与谁的组队关系？");

    if (who == me) return notify_fail("这样做好象不合适吧？");

    if (DIFFERSTR(me->querystr_temp("party/master"), who->querystr("id")))
        return notify_fail("你并没有和对方组队。");

    if (!who->query_temp(
            snprintf(msg, 255, "party/friend_%s", me->querystr("id")))) {
        me->delete_temp("party/master");
        me->delete_temp("party/follow");
        me->set_leader(me);
        return notify_fail("对方并没有与你组队，或已经将你赶出了队伍。");
    }

    message_vision("$HIC$N取消与$n的组队。", me, who);
    tell_object(who, snprintf(msg, 255, "$HIR%s(%s)取消了与你的组队。",
                              me->name(), me->id()));
    me->delete_temp("party/master");
    me->delete_temp("party/follow");
    me->set_leader(me);
    who->add_temp("party/number", -1);
    who->delete_temp(snprintf(msg, 255, "party/friend_%s", me->querystr("id")));

    me->say(snprintf(msg, 255, "你取消了与%s的组队。", who->name()), me);
    who->send_menu(me);

    return 1;
}

// 踢除出队
int CCmdUser::partykick(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (!(who = find_player(target)))
        return notify_fail("你要解除与谁的组队关系？");

    if (who == me) return notify_fail("这样做好象不合适吧？");

    if (DIFFERSTR(who->querystr_temp("party/master"), me->querystr("id"))) {
        me->add_temp("party/number", -1);
        me->delete_temp(
            snprintf(msg, 255, "party/friend_%s", who->querystr("id")));
        return notify_fail("对方并没有与你组队，或已经自己离开了队伍。");
    }

    if (!me->query_temp(
            snprintf(msg, 255, "party/friend_%s", who->querystr("id"))))
        return notify_fail("你并没有和对方组队。");

    message_vision("$HIC$N取消与$n的组队。", me, who);
    tell_object(who, snprintf(msg, 255, "$HIR%s(%s)取消了与你的组队。",
                              me->name(), me->id()));
    who->delete_temp("party/master");
    who->delete_temp("party/follow");
    who->set_leader(who);
    me->add_temp("party/number", -1);
    me->delete_temp(snprintf(msg, 255, "party/friend_%s", who->querystr("id")));

    who->say(snprintf(msg, 255, "你取消了与%s的组队。", who->name()), me);
    who->send_menu(me);

    return 1;
}

int CCmdUser::partyfollow(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_follow的场景使用Follow命令
    CContainer* env = me->environment();
    if ((env->query("no_follow") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止跟随他人。");
    char msg[255];

    CChar* who;

    if (!(who = get_room_char(me, target))) return notify_fail("你要跟随谁？");
    if (DIFFERSTR(me->querystr_temp("party/master"), who->querystr("id"))) {
        me->delete_temp("party/follow");
        return notify_fail("你并没有和对方组队？");
    }
    if (!who->query_temp(
            snprintf(msg, 255, "party/friend_%s", me->querystr("id")))) {
        me->delete_temp("party/master");
        me->delete_temp("party/follow");
        return notify_fail("对方并没有与你组队，或已经将你赶出了队伍。");
    }

    if (me->query_temp("party/follow")) {
        message_vision("$HIC$N取消与$n的组队跟随。", me, who);
        tell_object(who, snprintf(msg, 255, "$HIR%s(%s)取消了与你的组队跟随。",
                                  me->name(), me->id()));
        me->delete_temp("party/follow");
        me->say(snprintf(msg, 255, "你取消了与%s的组队跟随。", who->name()),
                me);
        who->send_menu(me);
    }
    else {
        message_vision("$YEL$N决定开始跟随$n的队伍一起行动。", me, who);
        tell_object(who, snprintf(msg, 255, "$HIR%s(%s)决定跟随你的队伍行动。",
                                  me->name(), me->id()));
        me->set_temp("party/follow", 1);
        me->say(snprintf(msg, 255, "你决定跟随%s的队伍行动。", who->name()),
                me);
        who->send_menu(me);
    }

    return 1;
}

int CCmdUser::partylist(CChar* me, int32_t target, char* arg)
{
    CChar* who = NULL;
    char msg[255];

    if (me->query_temp("party/number")) {
        who = me;
    }
    else if (me->querystr_temp("party/master")
             && (me->querystr_temp("party/master")[0])) {
        who = find_online(me->querystr_temp("party/master"));
    }
    if (!who) {
        me->delete_temp("party");
        return notify_fail("你并没有处于队伍中。");
    }
    tell_object(me, "$HIG你所处队伍中有以下成员：");
    tell_object(me, snprintf(msg, 255, "$CYN  队长：%s(%s)", who->name(),
                             who->querystr("id")));

    CMapping* m = who->querymap_temp("party");
    CVector key;
    m->keys(key);
    m->set("number", key.count());
    for (int i = 0; i < key.count(); i++) {
        if (strncmp(key[i], "friend_", 7)) continue;
        char tid[20];
        strcpy(tid, key[i] + 7);
        tid[strlen(key[i]) - 7] = 0;
        CChar* mem              = find_online(tid);
        if (!mem) {
            m->del(key[i]);
            m->add("number", -1);
            continue;
        }
        tell_object(me, snprintf(msg, 255, "$CYN  成员：%s(%s)", mem->name(),
                                 mem->querystr("id")));
    }

    return 1;
}

int CCmdUser::pair(CChar* me, int32_t target, char* arg)
{
    CChar* who         = find_player(target);
    const char* yourid = me->querystr("couple/id");
    const char* myid   = who->querystr("couple/id");

    int32_t melv  = me->query("level");
    int32_t wholv = who->query("level");

    if (EQUALSTR(me->id(1), myid) && EQUALSTR(who->id(1), yourid)
        && (abs(melv - wholv) <= CPartyConfig::couple_level_diff())) {
        int result = party(me, target, arg);
        if (result) me->set_temp("party/couple", 1);
        return result;
    }
    else {
        return notify_fail("这不太合适吧？");
    }
}

int CCmdUser::accept_pair(CChar* me, int32_t target, char* arg)
{
    CChar* who         = find_player(target);
    const char* yourid = me->querystr("couple/id");
    const char* myid   = who->querystr("couple/id");

    int32_t melv  = me->query("level");
    int32_t wholv = who->query("level");

    if (EQUALSTR(me->id(1), myid) && EQUALSTR(who->id(1), yourid)
        && (abs(melv - wholv) <= CPartyConfig::couple_level_diff())) {
        int result = accept_party(me, target, arg);
        if (result) me->set_temp("party/couple", 1);
        return result;
    }
    else {
        return notify_fail("这不太合适吧？");
    }
}

int CCmdUser::pairleave(CChar* me, int32_t target, char* arg)
{
    return partyleave(me, target, arg);
}

int CCmdUser::pairkick(CChar* me, int32_t target, char* arg)
{
    return partykick(me, target, arg);
}

int CCmdUser::pairfollow(CChar* me, int32_t target, char* arg)
{
    return partyfollow(me, target, arg);
}

int CCmdUser::pairlist(CChar* me, int32_t target, char* arg)
{
    return partylist(me, target, arg);
}
