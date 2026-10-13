#include "CmdUser.h"
#include "../engine/Channel.h"
#include "../npc/Npc.h"

int CCmdUser::talk(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_talk的场景使用Talk命令
    CContainer* env = me->environment();
    if ((env->query("no_talk") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止和人说话。");

    CChar* who;
    char msg[80], tmp[40];

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要和谁讲话，看清楚点！");
    if (!living(who) || who == me) return notify_fail("对方显然无法和你讲话。");

    notify_fail("对方并不愿意理你。");
    if (me->is_fighting() || who->is_fighting()) {
        if (who->query_attr(CChar::IS_NPC)
            && EQUALSTR(who->querystr("name"), "霜儿"))
            return static_cast<CNpc*>(who)->do_talk(me, arg);
        else
            return 0;
    }

    if (who->query_attr(CChar::IS_NPC)) {
        return static_cast<CNpc*>(who)->do_talk(me, arg);
    }

    CContainer* where = me->environment();

    // 与玩家的交互

    // 与玩家交易
    who->add_menu_item(snprintf(msg, 80, "查看%s的货品", who->name()),
                       "$0listgoods $1", me);
    // 与玩家组队
    if (EQUALSTR(me->querystr_temp("party/master"), who->querystr("id"))) {
        who->add_menu_item(snprintf(msg, 80, "解除与%s的组队", who->name()),
                           "$0partyleave $1", me);
        if (!me->query_temp("party/follow")) {
            who->add_menu_item(snprintf(msg, 80, "组队跟随"),
                               "$0partyfollow $1", me);
        }
        else {
            who->add_menu_item(snprintf(msg, 80, "取消组队跟随"),
                               "$0partyfollow $1", me);
        }
    }
    else if (me->query_temp(
                 snprintf(msg, 80, "party/friend_%s", who->querystr("id")))) {
        who->add_menu_item(snprintf(msg, 80, "踢除%s出队", who->name()),
                           "$0partykick $1", me);
        who->add_menu_item("查看在线队员", "$0partylist $1", me);
    }
    else
        who->add_menu_item(snprintf(msg, 80, "邀请%s加入队伍", who->name()),
                           "$0party $1", me);

    if (me->querystr("resident")[0])  // 有自己的房子
    {
        // 如果在家外
        if (DIFFERSTR(where->querystr("owner"), me->querystr("id"))
            && DIFFERSTR(where->querystr("second_owner"), me->querystr("id"))) {
            who->add_menu_item(snprintf(msg, 80, "邀请%s做客", who->name()),
                               "$0invite $1", me);
        }
        else  // 在家里 不能赶房主
        {
            if (DIFFERSTR(where->querystr("owner"), who->querystr("id")))
                who->add_menu_item(
                    snprintf(msg, 80, "把%s赶出家门", who->name()),
                    snprintf(tmp, 40, "$0kickout 0 %s", who->id(1)), me);
        }
    }

    if (where->query("sleep_room")
        && DIFFERSTR(who->querystr("gender"),
                     me->querystr("gender")))  // 可以睡觉
        who->add_menu_item(snprintf(msg, 80, "想和%s睡觉", who->name()),
                           "$0sleep $1", me);

    if (EQUALSTR(who->querystr("couple/id"),
                 me->querystr("id"))  // 夫妻可以传授
        && EQUALSTR(who->querystr("id"), me->querystr("couple/id"))) {
        who->add_menu_item(
            snprintf(msg, 80, "查看%s的技能", me->querystr("couple/gender")),
            "$0skill $1", me);
        who->add_menu_item(
            snprintf(msg, 80, "教%s武艺", me->querystr("couple/gender")),
            "$0teach $1", me);
    }

    if (me->query("recruit_exp") > 0)
        who->add_menu_item(snprintf(msg, 80, "收%s为徒", who->name()),
                           "$0recruit $1", me);

    if (who->query("recruit_exp") > 0)
        who->add_menu_item(snprintf(msg, 80, "拜%s为师", who->name()),
                           "$0apprentice $1", me);

    who->add_menu_item(snprintf(msg, 80, "和%s没什么好说的", who->name()), "",
                       me);
    who->send_menu(me);
    return 1;
}

int CCmdUser::vote(CChar* me, int32_t target, char* arg)
{
    char channel[20];
    CMapping voted_player;
    char msg[255];

    static int32_t vote_id = 100;

    analyse_string(arg, channel, 20);
    if (strcmp(arg, "open") != 0 && strcmp(arg, "close") != 0
        || !strlen(channel))
        return notify_fail("用指令/vote <close|open> <频道>");

    if (me->query("age") < VOTE_AGE_LIMIT
        || me->query("level") < VOTE_LEVEL_LIMIT) {
        if (me->query("age") < VOTE_AGE_LIMIT)
            return notify_fail("你年龄不满%d岁，没有投票权。", VOTE_AGE_LIMIT);

        if (me->query("level") < VOTE_LEVEL_LIMIT)
            return notify_fail("你等级不到%d级，没有投票权。",
                               VOTE_LEVEL_LIMIT);
    }

    const char* choicename = (strcmp(arg, "open") == 0 ? "打开" : "关闭");

    if (strcmp(channel, "chat") != 0 && strcmp(channel, "party") != 0
        && strcmp(channel, "new") != 0)
        return notify_fail("不能%s%s这个频道。", choicename, channel);

    CUser* player;
    if (!(player = find_player(target)))
        return notify_fail("目前找不到这个人。");

    char myvariable[80];
    const char* myid = me->id(1);

    snprintf(myvariable, 80, "%s_%s_%s", arg, channel,
             player->id(1));  // for checking double votes.

    char block_key[20];

    snprintf(block_key, 20, "chblk/%s", channel);

    if (strcmp(arg, "close") == 0) {
        if (g_current_time < player->query(block_key))
            return notify_fail("%s的交谈频道已经被关闭了。", player->name());
    }
    else {
        if (g_current_time > player->query(block_key))
            return notify_fail("%s的交谈频道已经被打开了。", player->name());
    }

    voted_player.copy_mapping(g_channel.voted_players.querymap(myvariable));

    if (!voted_player.undefinedp(myid))
        return notify_fail("你已经投过相同的票了。");
    else {
        CMapping m;

        m.set("obj", me->object_id());
        m.set("ip", get_ip(me));
        voted_player.set(myid, m);
    }

    int total_votes_required = determine_total();
    int votes_count          = counting(&voted_player, myvariable);

    g_channel.voted_players.set(myvariable, voted_player);

    // remove vote in 5 min if not enough people respond

    if (g_channel.votes_casted.undefinedp(myvariable)) {
        g_channel.votes_casted.set(myvariable, 1);

        // 取消动议
        vote_id++;
        g_channel.set(snprintf(msg, 80, "cancel_vote%d/myvariable", vote_id),
                      myvariable);
        g_channel.set(snprintf(msg, 80, "cancel_vote%d/choicename", vote_id),
                      choicename);
        g_channel.set(snprintf(msg, 80, "cancel_vote%d/playername", vote_id),
                      player->name(1));
        g_channel.call_out(nullify_vote, 600, vote_id);
    }

    int diff = total_votes_required - votes_count;

    // announce to mu
    MAP2USER* ulist        = users();
    const char* ch_channel = "闲聊";

    if (strcmp(channel, "new") == 0) ch_channel = "新手";
    if (strcmp(channel, "party") == 0) ch_channel = "门派";

    if (diff > 0) {
        message("vote",
                snprintf(msg, 255,
                         "$BLK【表决】%s(%s)投票%s%s(%s)的%s频道，需要%"
                         "d票通过，还差%d票！$COM",
                         me->name(1), me->id(1), choicename, player->name(1),
                         player->id(1), ch_channel, total_votes_required, diff),
                ulist);
    }
    else {
        if (strcmp(arg, "open") == 0)
            player->del(block_key);
        else
            player->set(block_key, g_current_time + 86400);

        // block for 1 day.
        g_channel.voted_players.del(myvariable);
        g_channel.votes_casted.del(myvariable);
        message(
            "vote",
            snprintf(msg, 255,
                     "$HIW【表决】%s(%s)投票%s%s(%s)的%s频道，%"
                     "s的交谈频道被%s了！$COM",
                     me->name(1), me->id(1), choicename, player->name(1),
                     player->id(1), ch_channel, player->name(1), choicename),
            ulist);
    }

    return 1;
}

int CCmdUser::determine_total()
{
    int total_required = 0;
    CMapping site;
    const char* ip;

    // to determine how many votes necessary to block channel

    MAP2USER* ulist = users();
    iterator_user p;

    for (p = ulist->begin(); p != ulist->end(); p++) {
        if ((p->second)->query("age") > VOTE_AGE_LIMIT
            && (p->second)->query("level") > VOTE_LEVEL_LIMIT) {
            ip = get_ip(p->second);
            site.add(ip, 1);

            // 每个IP只计一票
            if (site.query(ip) <= 1) total_required++;
        }
    }

    total_required = total_required / 3;
    if (total_required < 3) total_required = 3;
    if (total_required > 20) total_required = 20;
    return total_required;
}

int CCmdUser::counting(CMapping* players, const char* myvariable)
{
    CMapping site;
    const char* ip;
    int count = 0;

    CVector v;

    players->keys(v);

    for (int i = 0; i < v.count(); i++) {
        CMapping* m = players->querymap(v[i]);
        if (!m) continue;

        CUser* obj = find_player(m->query("obj"));

        if (!obj)  // this player has logged out.
        {
            players->del(v[i]);
            continue;
        }

        ip = get_ip(obj);
        site.add(ip, 1);

        // each site can count to max_voter.
        if (site.query(ip) <= 1) count++;
    }

    return count;
}

void CCmdUser::nullify_vote(CContainer* ob, int32_t vote_id, int32_t param2)
{
    char msg[255];
    const char* myvariable = g_channel.querystr(
        snprintf(msg, 80, "cancel_vote%d/myvariable", vote_id));
    const char* choice = g_channel.querystr(
        snprintf(msg, 80, "cancel_vote%d/choicename", vote_id));
    const char* playername = g_channel.querystr(
        snprintf(msg, 80, "cancel_vote%d/playername", vote_id));

    if (!g_channel.voted_players.undefinedp(myvariable)) {
        message("vote",
                snprintf(msg, 255,
                         "$HIW【表决】因为没有足够的人响应%s%"
                         "s的交谈频道，投票取消了！$COM",
                         choice, playername),
                users());

        g_channel.voted_players.del(myvariable);
        g_channel.votes_casted.del(myvariable);
    }
}

int CCmdUser::nick_name(CChar* me, int32_t target, char* arg)
{
    char msg[80];
    int len = strlen(arg);
    if (len < 1 || len > 20) return notify_fail("起个响亮点的绰号！");

    const char* ptr;

    ptr = check_legal_name(arg);
    if (ptr[0]) return notify_fail(ptr);

    me->set("nickname", arg);
    tell_object(me, snprintf(msg, 80, "你改变绰号为“%s”。", arg));
    return 1;
}

int CCmdUser::invite(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_invite的场景使用Talk命令
    CContainer* env = me->environment();
    if ((env->query("no_invite") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止邀请他人。");

    CChar* who;
    char msg[80];

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要邀请谁去你的家做客？");

    if (!me->querystr("resident")[0])
        return notify_fail("你还没有定居，怎么邀请人家？");

    if (who == me) return notify_fail("你对自己说：算了还是自己邀请自己吧。");

    if (EQUALSTR(who->querystr_temp("invite"), me->querystr("id")))
        return notify_fail("对方已经接受了你的邀请。");

    message_vision("$HIC$N热情的对$n说：请到我家来玩吧！", me, who);
    me->set_temp("pending/invite", who->querystr("id"));

    // 增加菜单指令
    // 注意，此处不能用$1代替自己。因为对方未必选中的是你。
    char tmp[40];

    me->add_menu_item(snprintf(msg, 80, "接受%s的邀请", me->name()),
                      snprintf(tmp, 40, "$0accept %ld", me->object_id()), who);

    me->add_menu_item("拒绝邀请", "", who);
    me->send_menu(who);

    return 1;
}

// 接受邀请
int CCmdUser::accept(CChar* me, int32_t target, char* arg)
{
    CChar* who;

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要接受谁的邀请？");

    if (who == me) return notify_fail("你对自己说：算了还是自己邀请自己吧。");

    if (EQUALSTR(me->querystr_temp("invite"), who->querystr("id")))
        return notify_fail("你已经接受了对方的邀请。");

    if (DIFFERSTR(who->querystr_temp("pending/invite"), me->querystr("id")))
        return notify_fail("对方并没有邀请你。");

    message_vision("$HIC$N接受了$n的邀请。", me, who);
    me->set_temp("invite", who->querystr("id"));
    who->delete_temp("pending/invite");

    who->say("你接受了邀请。", me);
    who->send_menu(me);

    return 1;
}

// 刷新玩家收徒经验
int CCmdUser::recruit_exp(CChar* me, int32_t target, char* arg)
{
    int32_t recruit_exp = 0;
    char msg[255];

    const char* master = me->id(1);

    if (wizardp(me)) {
        CChar* who;

        if (!(who = get_room_char(me, target)) && !(who = find_player(target)))
            who = me;

        if (strlen(arg))
            master = arg;
        else
            master = who->id(1);
    }

    CMapping* ptr = g_channel.query_recruit(master);

    if (!ptr || !ptr->get_count()) return notify_fail("你还没有收徒经验！");

    tell_object(me,
                snprintf(msg, 255, "当前你有徒弟%ld个：", ptr->get_count()));

    CVector v;

    ptr->keys(v);

    for (int i = 0; i < v.count(); i++) {
        CMapping* p = ptr->querymap(v[i]);
        tell_object(me,
                    snprintf(msg, 255, "徒弟%s(%s)贡献经验：%ld。(%s)",
                             p->querystr("name"), v[i], p->query("exp") / 100,
                             p->query("apprentice") ? "弟子" : "----"));

        recruit_exp += (p->query("exp") / 100);
    }

    if (recruit_exp > 50)
        me->set("recruit_exp", recruit_exp);
    else
        me->set("recruit_exp", 50);

    me->flush_my_info();

    tell_object(me, snprintf(msg, 255, "你的收徒经验共计%d。", recruit_exp));
    return 1;
}

// 同步

int CCmdUser::mv(CChar* me, int32_t target, char* arg)
{
    int32_t bx, by, ex, ey, ac;
    sscanf(arg, "%d %d %d %d %d", &bx, &by, &ex, &ey, &ac);

    // 玩家定位
    me->set_temp("map/bx", bx);
    me->set_temp("map/by", by);
    me->set_temp("map/ex", ex);
    me->set_temp("map/ey", ey);
    me->set_temp("map/ac", ac);

    // 向房间内其他玩家发送信息
    DTItemList* t = (me->environment())->get_item_list();
    CContainer* obj;

    for (auto val : *t) {
        obj = val;
        if (obj == me) continue;
        if (userp(obj))  // 是玩家
            static_cast<CUser*>(obj)->location_object(me);
    }

    return 1;
}

// Loading 结束，初始定位 loc 0 x y
int CCmdUser::loc(CChar* me, int32_t target, char* arg)
{
    int32_t x, y;

    if (!userp(me)) return 0;

    sscanf(arg, "%d %d", &x, &y);
    set_live_point(me, x, y);

    me->delete_temp("client/loading");

    (me->environment())->on_enter(me);  // 正式进入地图

    static_cast<CUser*>(me)->location_object(me);

    return 1;
}

// 显示在线玩家列表 who 0 1 1带页码 从0开始 类似于商品信息
// 每页显示10人
int CCmdUser::who(CChar* me, int32_t target, char* arg)
{
    char type[20];
    analyse_string(arg, type, 20);
    int begin = atol(arg) * 12;
    static_cast<CUser*>(me)->send_user_online(begin, begin + 12, atol(type));
    return 1;
}
