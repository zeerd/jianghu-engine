#include "CmdWizard.h"
#include "../engine/Channel.h"

int CCmdWizard::promote(CChar* me, int32_t target, char* arg)
{
    int grant;
    char msg[255];
    CChar* who;

    if (!(who = find_player(target))) return 0;

    grant = atoi(arg);  // 提升权限级别
    if (grant >= me->query("grant")
        || grant < CChar::
                   GRANT_PLAYER)  // 不可以低于PLAYER。[30~50)为玩家权限区间
        return notify_fail("你不能改变对方的权限到此级别！");

    if (me->query("grant") <= who->query("grant"))
        return notify_fail("你无权改变对方的权限！");

    who->set("grant", grant);  // 授权

    snprintf(msg, 255, "%s提升%s的权限到%d级。", me->name(), who->name(),
             grant);
    tell_object(who, msg);
    who->save();

    g_channel.do_channel(&g_channel, 0, "sys", msg);
    LOG_INFO("%s", msg);

    return 1;
}

int CCmdWizard::whois(CChar* me, int32_t target, char* arg)
{
    char cmd[255];
    const char* msg;
    CUser* who;

    if (!(who = find_player(target))) return 0;

    CContainer* env = who->environment();
    if (!env) {
        LOG_INFO("%s无环境中。", who->name());
        msg = "无环境";
    }
    else
        msg = env->name();

    tell_object(
        me,
        snprintf(cmd, 255,
                 "【系统】%s(%s)：位于%s，在线%ld秒，发呆%ld秒。客户端：%s版",
                 who->name(1), get_ip(static_cast<CChar*>(who)), msg,
                 g_current_time - who->query("login/time"), who->query_idle(),
                 who->querystr_temp("client/version")));

    return 1;
}

// 踢人
int CCmdWizard::kick(CChar* me, int32_t target, char* arg)
{
    char cmd[255];
    CUser* who;

    if (!(who = find_online(arg))) return notify_fail("格式：kickout <ID>");

    // 判断权限
    if (who != me && who->query_grant() >= me->query_grant())
        return notify_fail("你没有权力踢出对方。");

    snprintf(cmd, 255, "【系统】%s(%s)被%s请出了这个世界。", who->name(),
             get_ip(static_cast<CChar*>(who)), me->name());
    message_vision_room(me, "notify", cmd);

    me->call_out(do_kick, 1, who->object_id());
    return 1;
}

// 踢人容易当机!
void CCmdWizard::do_kick(CContainer* ob, int32_t param1, int32_t param2)
{
    CUser* who;

    if (!(who = find_player(param1))) return;

    drop_all(who);
    who->save_to_server();

    LOG_INFO("巫师踢出: %s(id:%s)", who->name(1), who->id(1));
    who->offline();

    DESTRUCT_OB(who, "cmd:kickout");
}

// 查找物件位置

int CCmdWizard::ban(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (strcmp(arg, "all") == 0) {
        // 列出所有黑名单
        CVector v;

        g_channel.banlist.keys(v);
        for (int i = 0; i < v.count(); i++)
            tell_object(me, snprintf(msg, 255, "地址：%s", v[i]));
        return 1;
    }

    if (target) {
        if (!(who = find_player(target))) return notify_fail("没有这个人。");

        // 判断权限
        if (who != me && who->query_grant() >= me->query_grant())
            return notify_fail("你的等级不够。");

        g_channel.banlist.set(get_ip(who), 1);
        g_channel.do_channel(&g_channel, 0, "rumor",
                             snprintf(msg, 255, "$WHT%s将%s(%s)加入黑名单。",
                                      me->name(1), who->name(1), get_ip(who)));
    }
    else {
        g_channel.banlist.set(arg, 1);
        g_channel.do_channel(&g_channel, 0, "rumor",
                             snprintf(msg, 255, "$WHT%s将地址(%s)加入黑名单。",
                                      me->name(1), arg));
    }

    return 1;
}

// UNBAN掉某人
int CCmdWizard::unban(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (target) {
        if (!(who = find_player(target))) return notify_fail("没有这个人。");

        if (g_channel.banlist.query(get_ip(who)))
            g_channel.do_channel(
                &g_channel, 0, "rumor",
                snprintf(msg, 255, "$WHT%s解除%s(%s)的黑名单。", me->name(1),
                         who->name(1), get_ip(who)));
        else
            return notify_fail("并没有加入黑名单。");
        g_channel.banlist.del(get_ip(who));
    }
    else {
        if (g_channel.banlist.query(arg))
            g_channel.do_channel(
                &g_channel, 0, "rumor",
                snprintf(msg, 255, "$WHT%s将地址(%s)从黑名单里删掉。",
                         me->name(1), arg));
        else
            return notify_fail("并没有加入黑名单。");
        g_channel.banlist.del(arg);
    }

    return 1;
}

// 封掉某ID
int CCmdWizard::freeze(CChar* me, int32_t target, char* arg)
{
    char msg[255];

    if (set_invalid_account(arg, 1))
        g_channel.do_channel(&g_channel, 0, "rumor",
                             snprintf(msg, 255, "%s的档案被封结了！", arg));
    else
        tell_object(me, "冻结失败!");

    LOG_INFO("%s(%s)封掉了帐号%s.", me->name(1), me->id(1), arg);
    return 1;
}

// 解开某ID
int CCmdWizard::unfreeze(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    set_invalid_account(arg, 0);
    tell_object(me, snprintf(msg, 255, "你解冻了帐号%s.", arg));
    LOG_INFO("%s(%s)解冻了帐号%s.", me->name(1), me->id(1), arg);

    return 1;
}

// 改某ID的密码
int CCmdWizard::chpass(CChar* me, int32_t target, char* arg)
{
    char msg[255], tmp[80];
    const char *id, *pass;

    analyse_string(arg, tmp, 80);
    id   = arg;
    pass = tmp;

    if (change_password(id, pass))
        tell_object(me, snprintf(msg, 255, "修改帐号%s密码为%s！", id, pass));
    else
        tell_object(me, "修改密码失败！");
    return 1;
}

// 改某ID的的身份验证密码
int CCmdWizard::chverifypass(CChar* me, int32_t target, char* arg)
{
    char msg[255], tmp[80];
    const char *id, *pass;

    analyse_string(arg, tmp, 80);

    id   = arg;
    pass = tmp;

    if (change_verify_password(id, pass))
        tell_object(
            me, snprintf(msg, 255, "修改帐号%s的身份校验密码为%s！", id, pass));
    else
        tell_object(me, "修改身份校验密码失败！");
    return 1;
}

int CCmdWizard::who(CChar* me, int32_t target, char* arg)
{
    iterator_user p;
    MAP2USER* list = users();
    CUser* who;
    char msg[255];

    typedef map<string, CUser*, less<string> > MYLIST;
    MYLIST list2;
    MYLIST::iterator p2;

    CMapping ip;
    int count1 = 0, count2 = 0;
    int count3 = 0, count4 = 0;

    for (p = list->begin(); p != list->end(); p++) {
        who              = p->second;
        const char* myip = get_ip(who);
        snprintf(msg, 255, "%s %s", myip, who->id(1));
        list2[msg] = who;

        if (!ip.query(myip)) {
            count1++;                          // IP计数
            if (!who->query_temp("net_dead"))  // 连线
                count2++;
        }

        count3++;

        if (!who->query_temp("net_dead")) count4++;

        ip.add(myip, 1);
    }

    tell_object(
        me, snprintf(msg, 255,
                     "\n当前在线玩家%d(%d)人。连入机器%d(%d)台。\n---------"
                     "------------------------------------------------------",
                     count4, count3, count2, count1));

    // 排序
    for (p2 = list2.begin(); p2 != list2.end(); p2++) {
        who = p2->second;

        const char* area  = "";
        const char* place = "";
        CContainer* env   = who->environment();
        if (env) {
            area  = env->querystr("area");
            place = env->name(1);
        }
        tell_object(me, snprintf(msg, 255, "%s(%s)(%d)(%s/%s) (%s)",
                                 who->name(1), who->id(1), who->object_id(),
                                 place, area, get_ip(who)));
    }

    return 1;
}

// 死亡恢复
int CCmdWizard::recover(CChar* me, int32_t target, char* arg)
{
    CChar* who;

    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    int32_t exp = who->query("death/combat_exp_loss");
    who->add("combat_exp", exp);

    LOG_INFO("%s恢复了%ld点死亡经验！", who->name(1), exp);

    if (who->query("death/skill_loss")) {
        who->skill_death_recover();
    }

    who->del("death");

    tell_object(me, "恢复完成！");

    return 1;
}

// 监听信息
int CCmdWizard::snoop(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (strcmp(arg, "none") == 0) {
        who = g_channel.query_snooping(me);

        if (who && who->query_grant() >= me->query_grant())
            tell_object(
                who, snprintf(msg, 255, "%s停止了对你的监听。", me->name(1)));

        g_channel.snoop(me);
        tell_object(me, "解除监听。");
        return 1;
    }

    if (target == 0) {
        who = g_channel.query_snooping(me);

        if (who)
            tell_object(me, snprintf(msg, 255, "你当前正在监听%s的信息。",
                                     who->name(1)));
        else
            tell_object(me, "你没有监听任何人。");

        return 1;
    }

    if (!(who = find_player(target))) return notify_fail("没有这个人。");

    if (who == me) return notify_fail("用snoop none解除监听。");

    if (who->query_grant() >= me->query_grant())
        tell_object(
            who, snprintf(msg, 255, "%s开始监听你接收到的信息。", me->name(1)));

    tell_object(me,
                snprintf(msg, 255, "你开始监听%s接收到的信息。", who->name(1)));

    g_channel.snoop(me, who);

    return 1;
}

// 重新生成场景！！

int CCmdWizard::chblk(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255], arg1[40];

    analyse_string(arg, arg1, 40);

    if (!(who = find_online(arg))) return notify_fail("你要关谁的频道。");
    if (wizardp(who)) return notify_fail("不能关闭巫师的频道。");

    if (strcmp(arg1, "open") == 0) {
        who->del("chblk");
        tell_object(me,
                    snprintf(msg, 255, "你打开了%s的交谈频道。", who->name(1)));
        tell_object(who,
                    snprintf(msg, 255, "%s打开了你的交谈频道。", me->name(1)));
    }
    else {
        who->set("chblk/all", g_current_time + 1800);
        tell_object(me,
                    snprintf(msg, 255, "你关闭了%s的交谈频道。", who->name(1)));
    }

    return 1;
}
