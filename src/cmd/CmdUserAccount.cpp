#include "CmdUser.h"
#include "../engine/Channel.h"

int CCmdUser::quit(CChar* me, int32_t target, char* arg)
{
    char cmd[255];

    if (me->is_fighting() && !me->query_temp("net_dead"))
        return notify_fail("战斗中不能退出。");

    if (me->is_busy())
        return notify_fail("( 你上一个动作还没有完成，不能退出。)");

    drop_all(me);
    me->save_to_server();

    LOG_INFO("玩家主动退出: %s(id:%s)", me->name(1), me->id(1));
    // 离线
    static_cast<CUser*>(me)->offline();
    tell_room(me->environment(), snprintf(cmd, 255, "%s离开游戏。", me->name()),
              me);

    g_channel.do_channel(&g_channel, 0, "sys", cmd);

    DESTRUCT_OB(me, "cmd::quit");
    return 1;
}

int CCmdUser::save(CChar* me, int32_t target, char* arg)
{
    if (g_current_time < me->query_temp("last_save_time")) {
        message_vision_room(me, "tell", "您在三十秒内刚存过盘。");
        return 1;
    }

    me->set_temp("last_save_time", g_current_time + 30);

    CContainer* where = me->environment();
    if (where->query("valid_startroom")
        || EQUALSTR(where->querystr("owner"), me->querystr("id"))
        || EQUALSTR(where->querystr("second_owner"), me->querystr("id"))) {
        me->set("start_city", where->querystr("base_name"));
        tell_object(me, "当你下次连线进来时，会从这里开始。");
    }

    if (me->save())
        tell_object(me, "档案储存完毕。");
    else
        tell_object(me, "储存失败。");

    return 1;
}

int CCmdUser::set_param(CChar* me, int32_t target, char* arg)
{
    char str_wiz_only[] = "[\"invisibility\",\"immortal\"]";
    CVector wiz_only;
    char val[40], msg[255];
    CMapping* env;
    char key[40];

    env = me->querymap("env");
    if (!strlen(arg)) {
        tell_object(me, "你目前设定的环境变数有：");
        if (!env)
            tell_object(me, "没有设定任何环境变数。");
        else {
            CVector v;
            int i;

            env->keys(v);

            for (i = 0; i < v.count(); i++) {
                CMapping::svalue_t* sv = env->find_string_in_mapping(v[i]);
                if (sv->type == T_NUMBER)
                    tell_object(
                        me, snprintf(msg, 80, "%s = %d", v[i], sv->u.number));
                else if (sv->type == T_STRING)
                    tell_object(me, snprintf(msg, 80, "%s = %s", v[i],
                                             sv->u.string.get()));
            }
        }
        return 1;
    }

    if (env && env->get_count() >= MAX_ENV_VARS)
        return notify_fail("你设的环境变数太多了，请先用 unset 删掉几个吧。");

    analyse_string(arg, val, 40);

    wiz_only.load_buffer(str_wiz_only);
    if (!wizardp(me) && wiz_only.find(arg) != wiz_only.end())
        return notify_fail("只有巫师能用这个设定。");

    if (strcmp(arg, "wimpy") == 0 && (atol(val) < 0 || atol(val) > 80))
        return notify_fail("wimpy [<逃跑时「气」的百分比>]");

    if (!check_valid_char(arg)) return notify_fail("你设置环境变量名非法！");

    snprintf(key, 40, "env/%s", arg);

    if (val[0] == '-' || (val[0] >= '0' && val[0] <= '9')) {
        me->set(key, atol(val));
        snprintf(msg, 255, "你设置环境变量：%s 为 %s。", arg, val);
        message_vision_room(me, "tell", msg);
    }
    else {
        snprintf(msg, 255, "你设置环境变量：%s 为 %s 非法。", arg, val);
        message_vision_room(me, "tell", msg);
    }
    return 1;
}

int CCmdUser::unset(CChar* me, int32_t target, char* arg)
{
    char key[80];
    if (!strlen(arg)) return 0;

    snprintf(key, 80, "env/%s", arg);
    me->del(key);
    tell_object(me, "Ok.");
    return 1;
}

int CCmdUser::mudage(CChar* me, int32_t target, char* arg)
{
    int sec, min, hrs, day, mon, age;
    char msg[160], tmp[255], str[255];

    if (me->query("hp") < 10)
        return notify_fail("你的精神不能集中来算自己的年岁。");

    age = me->query("mud_age");

    sec = age % 60;
    age /= 60;
    min = age % 60;
    age /= 60;
    hrs = age % 24;
    age /= 24;
    day = age % 30;
    mon = age / 30;

    msg[0] = 0;
    if (mon) snprintf(msg, 80, "%s月", chinese_number(mon, str));

    if (day) {
        snprintf(tmp, 80, "%s天", chinese_number(day, str));
        strcat(msg, tmp);
    }

    if (hrs) {
        snprintf(tmp, 80, "%s小时", chinese_number(hrs, str));
        strcat(msg, tmp);
    }
    if (min) {
        snprintf(tmp, 80, "%s分", chinese_number(min, str));
        strcat(msg, tmp);
    }

    me->add("hp", -5);

    tell_object(me, snprintf(tmp, 255, "你已经为%s奋斗%s了。", MAIN_NAME, msg));
    return 1;
}

// 修改密码
int CCmdUser::passwd(CChar* me, int32_t target, char* arg)
{
    char msg[40];

    analyse_string(arg, msg, 40);

    // 检测旧密码
    if (!me->query_temp("pending/modify_pass")) {
        if (!verify_user(me->querystr("id"), arg))
            return notify_fail("与原密码不符，不能设置新密码！");

        if (!verify_password(me->id(1), msg))
            return notify_fail("身份验证未通过!");

        me->set_temp("pending/modify_pass", 1);
        tell_object(me, "旧密码检测通过，请继续输入新密码！");
        return 1;
    }

    if (strlen(arg) < 5) return notify_fail("密码不能少于5个字元！");
    if (strlen(arg) > 20) return notify_fail("您的密码也太长了！");

    if (!change_password(me->querystr("id"), arg))
        return notify_fail("修改密码失败！");

    me->delete_temp("pending/modify_pass");
    tell_object(me, "修改密码成功！");
    return 1;
}

// 验证玩家身份
int CCmdUser::validate(CChar* me, int32_t target, char* arg)
{
    char msg[40];

    analyse_string(arg, msg, 40);

    if (!me->query_temp("validate")) {
        if (!verify_user(me->querystr("id"), arg))
            return notify_fail("密码不符，不能进行进一步的命令操作！");

        if (!verify_password(me->id(1), msg))
            return notify_fail("暗码不符，不能进行进一步的命令操作！");

        me->set_temp("validate", 1);
        tell_object(me, "身份验证通过，你现在可以继续进行你的操作！");
    }
    return 1;
}

// 修改暗码
int CCmdUser::verify_pass(CChar* me, int32_t target, char* arg)
{
    char msg[40];

    analyse_string(arg, msg, 40);
    if (!msg[0]) {
        strncpy(msg, arg, 40)[39] = 0;
        arg[0]                    = 0;
    }

    if (!verify_password(me->id(1), arg)) return notify_fail("身份验证未通过!");

    if (strlen(msg) < 5) return notify_fail("暗码不能少于5个字元！");
    if (strlen(msg) > 20) return notify_fail("您的暗码也太长了！");

    if (!change_verify_password(me->querystr("id"), msg))
        return notify_fail("修改暗码失败！");

    tell_object(me, "修改暗码成功！");
    return 1;
}

// 升级系统
int CCmdUser::upgrade(CChar* me, int32_t target, char* arg)
{
    int level         = me->query("level");
    int32_t valid_exp = me->query_valid_exp();  // 可用来升级的经验
    int32_t need_exp  = CChar::level2_exp(level + 1) - CChar::level2_exp(level);

    if (valid_exp < need_exp) return notify_fail("你当前的经验不够升一级！");

    me->set("level", level + 1);
    setup_char(me);
    tell_object(me, "升级成功！");
    me->flush_my_info();
    return 1;
}

// 关闭频道
int CCmdUser::tune(CChar* me, int32_t target, char* arg)
{
    CVector* v;
    char msg[80];

    v = me->queryvec("channels");

    if (arg) {
        if (v && v->find(arg) != v->end()) {
            tell_object(me, snprintf(msg, 80, "关闭%s频道。", arg));
            v->remove(arg);
            return 1;
        }
    }

    tell_object(me, "要打开某个频道只要用该频道说话即可。");
    return 1;
}

int CCmdUser::refuse_tell(CChar* me, int32_t target, char* arg)
{
    char key[80];
    CUser* player;

    if (target) {
        char msg[80];
        if (!(player = find_player(target)))
            return notify_fail("目前找不到这个人。");

        snprintf(key, 80, "notell/%s", player->querystr("id"));
        tell_object(
            me, snprintf(msg, 80, "你拒绝收听%s的悄悄话。", player->name()));
    }
    else {
        strcpy(key, "notell/all");
        tell_object(me, "你拒绝收听所有人的悄悄话。");
    }

    me->set_temp(key, 1);
    return 1;
}

// 收听
int CCmdUser::listen(CChar* me, int32_t target, char* arg)
{
    char key[80];
    CUser* player;

    if (target) {
        char msg[80];
        if (!(player = find_player(target)))
            return notify_fail("目前找不到这个人。");

        snprintf(key, 80, "notell/%s", player->querystr("id"));
        me->delete_temp(key);
        me->delete_temp("notell/all");
        tell_object(
            me, snprintf(msg, 80, "你开始收听%s的悄悄话。", player->name()));
    }
    else {
        me->delete_temp("notell");
        tell_object(me, "你开始收听所有人的悄悄话。");
    }

    return 1;
}

int CCmdUser::refuse(CChar* me, int32_t target, char* arg)
{
    CMapping m;
    char msg[80];

    m.set("practice", 1);
    m.set("study", 1);
    m.set("exercise", 1);
    m.set("combat", 1);

    if (!strlen(arg)) {
        CMapping* ch = me->querymap_temp("refuse");
        if (!ch) return notify_fail("你目前没有设置任何信息通道。");

        tell_object(me, "你目前已经关闭的信息通道有：");

        CVector v;

        ch->keys(v);
        for (int i = 0; i < v.count(); i++)
            tell_object(me, snprintf(msg, 80, "%s通道", v[i]));

        return 1;
    }

    // 打开所有通道
    if (strcmp(arg, "none") == 0) {
        me->delete_temp("refuse");
        tell_object(me, "信息通道恢复正常。");
        return 1;
    }

    if (!m.query(arg)) return notify_fail("不允许关闭该信息通道。");

    me->set_temp(snprintf(msg, 80, "refuse/%s", arg), 1);
    tell_object(me, snprintf(msg, 80, "OK，关闭了%s的信息通道！", arg));
    return 1;
}
