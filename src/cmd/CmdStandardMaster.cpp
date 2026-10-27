#include "CmdStandard.h"
#include "../skill/Skill.h"
#include "../npc/Npc.h"
#include "LuaHook.h"

int CCmdStandard::apprentice(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];

    if (!(who = get_room_char(me, target))) return 0;

    // 禁止在设置为no_apprentice的场景使用Apprentice命令
    CContainer* env = me->environment();
    if ((env->query("no_apprentice") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止拜师。");

    if (!who->query_attr(CChar::IS_LIVING)) {
        return notify_fail("你必须先把%s弄醒。", who->name());
    }

    if (who == me) return notify_fail("拜自己为师？好主意．．：P");

    if (is_user(who) && who->query("recruit_exp") == 0)
        return notify_fail("对方还没有收徒资格！");

    if (me->is_apprentice_of(who)) {
        message_vision("$N恭恭敬敬地向$n磕头请安，叫道：「师父！」", me, who);
        return 1;
    }

    const char* bangpai = me->querystr("family/family_name");
    if (bangpai[0] && DIFFERSTR(bangpai, who->querystr("family/family_name")))
        return notify_fail("未经本派师门同意不能改换门派！");

    if (EQUALSTR(who->querystr("family/master_id"), me->querystr("id")))
        return notify_fail("开什么玩笑？拜自己的徒弟为师？");

    // If the target is willing to recruit us already, we do it.
    if (who->query_temp("pending/recruit") == me->object_id()) {
        if (EQUALSTR(me->querystr("family/family_name"),
                     who->querystr("family/family_name"))) {
            message_vision(
                "$N决定投入$n门下！！$N跪了下来向$"
                "n恭恭敬敬地磕了四个响头，叫道：「师父！」",
                me, who);
            me->set("score", 0L);
        }
        else
            message_vision(
                "$N决定拜$n为师。$N跪了下来向$"
                "n恭恭敬敬地磕了四个响头，叫道：「师父！」",
                me, who);

        who->recruit_apprentice(me);
        who->delete_temp("pending/recruit");

        tell_object(who, "恭喜你新收了一名弟子！");
        tell_object(me, snprintf(msg, 255, "恭喜您成为%s的第%ld代弟子。",
                                 me->querystr("family/family_name"),
                                 me->query("family/generation")));
        return 1;
    }

    tell_object(me, snprintf(msg, 255, "你想要拜%s为师。", who->name()));
    me->set_temp("pending/apprentice", who->object_id());
    if (who->query_attr(CChar::IS_USER)) {
        tell_object(who,
                    snprintf(msg, 255, "如果你愿意收%s为弟子，请与其交谈。",
                             me->name()));
    }
    else
        who->attempt_apprentice(me);
    if (me->is_apprentice_of(who)) {
    }

    return 1;
}

int CCmdStandard::recruit(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;

    if (!(who = get_room_char(me, target))) return 0;

    // 禁止在设置为no_recruit的场景使用Recruit命令
    CContainer* env = me->environment();
    if ((env->query("no_recruit") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止收徒。");

    if (who == me) return notify_fail("收自己为弟子？好主意．．．不过没有用。");

    if (is_user(me) && me->query("recruit_exp") == 0)
        return notify_fail("你还没有收徒资格！");

    if (who->is_apprentice_of(me)) {
        message_vision("$N拍拍$n的头，说道：「好徒儿！」", me, who);
        return 1;
    }

    if (!me->querystr("family/family_name")[0])
        return notify_fail(
            "你并不属于任何帮派，你必须先加入一个帮派，或自己创一个才能收徒"
            "。");

    if (!me->query("family/generation"))
        return notify_fail("你乃弃徒，先求哪一位本门师父将你重列门墙再说吧。");

    const char* bangpai = who->querystr("family/family_name");
    if (bangpai[0]
        && DIFFERSTR(me->querystr("family/family_name"),
                     who->querystr("family/family_name")))
        return notify_fail("未经其师尊同意就挖人家墙角不太好吧。");

    if (EQUALSTR(me->querystr("family/master_id"), who->querystr("id")))
        return notify_fail(
            "开什么玩笑？人家是你师父，还有什么要从你这里学的？");

    // If the target is willing to apprentice us already, we do it.
    if (who->query_temp("pending/apprentice") == me->object_id()) {
        if (!who->query_attr(CChar::IS_LIVING)) {
            message_vision(
                "$N决定收$n为弟子。不过看样子$n显然没有办法行拜师之礼。", me,
                who);
            return 1;
        }

        if (who->querystr("family/family_name")[0]
            && DIFFERSTR(me->querystr("family/family_name"),
                         who->querystr("family/family_name"))) {
            message_vision(
                "$N决定投入$n门下！！\n$N跪了下来向$"
                "n恭恭敬敬地磕了四个响头，叫道：「师父！」",
                me, who);
            who->set("score", 0L);
        }
        else {
            message_vision(
                "$N决定收$n为弟子。\n$n跪了下来向$"
                "N恭恭敬敬地磕了四个响头，叫道：「师父！」",
                me, who);
        }

        me->recruit_apprentice(who);
        who->delete_temp("pending/apprentice");

        tell_object(me, "恭喜你新收了一名弟子！");
        tell_object(who, snprintf(msg, 255, "恭喜您成为%s的第%ld代弟子。",
                                  who->querystr("family/family_name"),
                                  who->query("family/generation")));
        return 1;
    }

    me->set_temp("pending/recruit", who->object_id());
    message_vision("$N想要收$n为弟子。", me, who);

    tell_object(who, snprintf(msg, 255, "如果你愿意拜%s为师父，请与其交谈。",
                              me->name()));

    return 1;
}

int CCmdStandard::expell(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;

    if (!(who = get_room_char(me, target))) return 0;

    // 禁止在设置为no_expell的场景使用Expell命令
    CContainer* env = me->environment();
    if ((env->query("no_expell") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止弃徒。");

    if (!who->query_attr(CChar::IS_USER))
        return notify_fail("你只能开革玩家所扮演的人物。");

    if (me->query("family/privs") == -1
        && EQUALSTR(me->querystr("family/family_name"),
                    who->querystr("family/family_name"))) {
        message_vision(
            snprintf(msg, 255,
                     "$N对着$n说道：从今天起，你再也不是我%s的弟子了，你走吧！",
                     me->querystr("family/family_name")),
            me, who);
        tell_object(who, "你被开革出师门了！");
    }
    else if ((who)->is_apprentice_of(me)) {
        message_vision(
            "$N对着$"
            "n说道：从今天起，你我师徒恩断情绝，你走吧！江湖风波，善恶无形"
            "，好自为之。。。",
            me, who);
        tell_object(who, "你被师父开革出师门了！");
    }
    else
        return notify_fail("这个人不是你的弟子。");

    char myfamily[80];
    who->release_master();

    me->del("zhangmen");
    who->del("family");
    who->del("class");
    snprintf(myfamily, 80, "%s弃徒", who->querystr("family/family_name"));
    who->set("title", myfamily);
    who->update_me();
    who->save();

    CMapping* skills = who->query_skills();

    CVector v;

    skills->keys(v);
    // 所有技能降一半
    for (int i = 0; i < v.count(); i++) {
        (*skills)[v[i]] /= 2;
    }

    return 1;
}

int CCmdStandard::teach(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_teach的场景使用Teach命令
    CContainer* env = me->environment();
    if ((env->query("no_teach") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止传授技能。");

    CChar* who;

    if (me->is_busy()) return notify_fail("你现在正忙着呢。");

    if (me->is_fighting()) return notify_fail("临阵磨枪？来不及啦。");

    if (!me->querymap("couple")) return notify_fail("你还没有伴侣呢！");

    who = (CChar*)(me->environment())->present(me->querystr("couple/id"));

    if (who == NULL)
        return notify_fail("你的%s不在这里。", me->querystr("couple/gender"));

    if (!is_living(who))
        return notify_fail("嗯....你得先把%s弄醒再说。", who->name());

    if (who->is_busy()) return notify_fail("%s现在正忙着呢。", who->name());

    if (who && who->is_fighting()) return notify_fail("临阵磨枪？来不及啦。");

    if (me->query("add_skill/count"))
        return notify_fail("你暂时无法指点你伴侣的武功。。");

    if (!strlen(arg))  // 无参数，显示可以教的技能
    {
        CMapping* skills;
        CSkill* sk;
        char skill_name[80], cmd[80];

        skills = me->query_skills();

        int pt = who->query("level") * who->query_int() / 50;
        if (pt <= 0) pt = 1;

        CVector v;

        skills->keys(v);
        for (int i = 0; i < v.count(); i++) {
            int32_t need_points = who->skill_need_points(v[i]);

            sk = load_skill(v[i]);

            sprintf(skill_name, "%-20s %d", sk->name(1), need_points / pt + 1);
            who->add_menu_item(skill_name,
                               snprintf(cmd, 80, "$0teach $1 %s", v[i]), me);
        }

        who->add_menu_item("离开", "", me);
        who->send_menu(me);

        return 1;
    }

    int my_skill = me->query_skill(arg, 1);

    if (!my_skill) return notify_fail("这项技能你还不会呢！");

    int student_skill = who->query_skill(arg, 1);

    if (my_skill <= student_skill)
        return notify_fail("这项技能%s的程度已经不输于你了。", who->name());

    CSkill* sk = load_skill(arg);

    notify_fail("依%s目前的能力，没有办法学习这种技能。", who->name());
    if (!sk->valid_learn(who)) return 0;

    if (student_skill > 351)
        return notify_fail(
            "你的爱侣学习潜力已经发挥到头了，这项技能还是爱侣自己去江湖历练"
            "中领悟更高的境界吧。");

    // 增加忘记技能判断
    if (!who->can_learn(arg))
        return notify_fail("这项技能%s永世不得修习！", who->name());

    int gin_cost = 100;

    if (!student_skill) {
        gin_cost *= 2;
        who->set_skill(arg, 0);
    }

    if (me->query("sk_exp") * 3 > me->query_exp())
        return notify_fail("你已经没有多余的技能经验教你的爱侣了。");
    if (who->query("sk_exp") * 3 > who->query_exp())
        return notify_fail(
            "你的爱侣学习潜力已经发挥到头了。还是帮助对方再历练历练吧。");

    // 现在计算所需的经验
    int32_t need_points = who->skill_need_points(arg);
    int32_t need_exp    = need_points / who->learn_bonus() + 1;

    // 计算玩家的经验够不够
    if (who->query_valid_exp() < need_exp)
        return notify_fail("学习%s还需%ld点经验，%s需要再历练历练。",
                           sk->name(1), need_exp, who->name());
    if (me->query_valid_exp() < need_exp)
        return notify_fail("教会%s还需%ld点经验，你需要再历练历练。",
                           sk->name(1), need_exp);

    char msg[255];

    tell_object(me, snprintf(msg, 255, "你向%s解释有关「%s」的疑问。",
                             who->name(), sk->name(1)));

    if (who->query("env/no_learn"))
        return notify_fail("但是%s现在并不准备听你解释问题。", who->name());

    tell_object(who, snprintf(msg, 255, "%s向你解释有关「%s」的问题。",
                              me->name(), sk->name(1)));

    if (me->query("hp") > gin_cost) {
        if (is_user(me)) me->receive_damage("hp", gin_cost);
    }
    else {
        tell_object(me, "但是你太累了，没有办法再教。");
        tell_object(who,
                    snprintf(msg, 255, "但是%s显然太累了，没有办法教你什麽。",
                             me->name()));
        return 1;
    }

    if (who->query("hp") > gin_cost) {
        if (strcmp(sk->type(), "martial") == 0
            && student_skill > CChar::level2skill(who->query("level"))) {
            tell_object(
                who, snprintf(msg, 255,
                              "也许是缺乏实战经验，你对%s的解释总是无法领会。",
                              me->name()));
        }
        else {
            tell_object(who,
                        snprintf(msg, 255, "你听了%s的指导，似乎有些心得。",
                                 me->name()));
            who->add("sk_exp", need_exp);
            me->add("sk_exp", need_exp);
            who->improve_skill(arg, need_points + 1);
        }

        who->receive_damage("hp", gin_cost);
    }
    else {
        tell_object(who, "你今天太累了，结果什么也没有学到。");
    }

    return 1;
}

// 请教技能。
/*
    师傅指点，一次一级。消耗100 HP
    需要经验：按到下一级所需learned计算。
    need_points = (skill + 1) * (skill + 1) - learned;
    一点经验可提高 pt = (level / 10 + 1) * int / 3;
    那么所需经验为：need_exp = need_points / pt + 1;
*/

int CCmdStandard::learn(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_learn的场景使用Learn命令
    CContainer* env = me->environment();
    if ((env->query("no_learn") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止学习技能。");

    int master_skill, my_skill;
    CChar* who;
    char msg[255];

    if (me->is_busy()) return notify_fail("你现在正忙着呢。");

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要向谁请教？");

    if (!strlen(arg)) return notify_fail("指令格式：learn skill_name");

    if (me->is_fighting()) return notify_fail("临阵磨枪？来不及啦。");

    if (!who->query_attr(CChar::IS_LIVING))
        return notify_fail("嗯．．．你得先把对方弄醒再说。");

    if (!me->is_apprentice_of(who) && !(who->recognize_apprentice(me))) {
        who->say("您见笑了，我这点雕虫小技怎够资格「指点」您什么？", me);
        who->send_menu(me);
        return 0;
    }

    if (!who) return notify_fail("你找不到这个人。");

    if (who->is_fighting()) return notify_fail("你的师傅暂时没空指点你武功。");

    if (!(master_skill = who->query_skill(arg, 1)))
        return notify_fail("这项技能你恐怕必须找别人学了。");

    if (who->prevent_learn(me, arg))
        return notify_fail("显然对方不愿教你这项技能。");

    if (who->query("add_skill/count"))
        return notify_fail("你的师傅暂时没空指点你武功。。");

    my_skill = me->query_skill(arg, 1);

    if (my_skill >= master_skill)
        return notify_fail("这项技能你的程度已经不输你师父了。");

    CSkill* sk = load_skill(arg);
    if (!sk) return notify_fail("你无法学习这项技能。");

    notify_fail("你无法学习这项技能。");
    if (!sk->valid_learn(me)) return 0;

    // 增加忘记技能判断
    if (!me->can_learn(arg)) return notify_fail("这项技能你永世不得修习！");

    if (who->query("env/no_teach")) {
        who->say("关于这个问题，以后再说吧。");
        who->send_menu(me);
        return 0;
    }
    if (who->query(snprintf(msg, 255, "env/%s", arg))) {
        who->say("这门武功，今天师傅还不想教你。");
        who->send_menu(me);
        return 0;
    }
    if (is_user(who)) {
        int max_leran = 400;
        if (sk->query("max_learan")) max_leran = sk->query("max_learan");

        if (my_skill > max_leran) {
            who->say(
                "徒儿啊，你现在也是一代宗师了，这项技能还是你自己去江湖历练"
                "中领悟更高的境界吧。");
            who->send_menu(me);
            return 0;
        }
        else {
            if (who->query("hp") > 350) {
                who->receive_damage("hp", 100);
            }
            else {
                who->say("徒儿啊，你师傅我今天太累了，改天再说吧。", me);
                who->send_menu(me);
                return 1;
            }
        }
    }

    if (me->query("hp") < 100)
        return notify_fail("你今天太累了，结果什么也没有学到。");

    // 玩家的技能经验在总经验中的比重不能超过1/3
    if (me->query("sk_exp") * 3 > me->query_exp()) {
        who->say("习武重在实践。我看你还是努力提高一下自身素质吧。", me);
        who->send_menu(me);
        return 1;
    }

    // 现在计算所需的经验
    int32_t need_points = me->skill_need_points(arg);
    int32_t need_exp    = need_points / me->learn_bonus() + 1;

    // 计算玩家的经验够不够
    if (me->query_valid_exp() < need_exp) {
        who->say(snprintf(msg, 255, "学习%s还需%d点经验，你再去历练历练吧。",
                          sk->name(1), need_exp),
                 me);
        who->send_menu(me);
        return 1;
    }

    // 计算玩家的技能上限有否达到
    if (strcmp(sk->type(), "martial") == 0
        && my_skill >= CChar::level2skill(me->query("level"))) {
        who->say(
            snprintf(msg, 255, "你缺乏实战经验，无法领会%s。", sk->name(1)),
            me);
        who->send_menu(me);
        return 1;
    }

    me->add("sk_exp", need_exp);
    me->improve_skill(arg, need_points + 1);
    me->receive_damage("hp", 100);

    me->flush_my_info();
    return 1;
}

int CCmdStandard::abandon(CChar* me, int32_t target, char* arg)
{
    CMapping* smap;
    CSkill* sk;
    char msg[255];

    if (!me->query_temp("validate"))
        return notify_fail(
            "请先使用 </validate 你的密码 你的暗码> "
            "命令验证你的身份后方可放弃你的技能。");

    if (!strlen(arg)) return notify_fail("你要放弃哪一项技能？");

    smap = me->query_skill_map();
    if (smap->get_count()) {
        CVector v;

        smap->keys(v);
        for (int i = 0; i < v.count(); i++) {
            if (strcmp(smap->querystr(v[i]), arg) == 0) {
                me->map_skill(v[i]);
            }
        }
    }

    int lvl = me->query_skill(arg, 1);

    if (!me->delete_skill(arg)) return notify_fail("你并没有学过这项技能。");

    if (!(sk = load_skill(arg))) return notify_fail("没有这项技能。");

    me->say(snprintf(msg, 255, "你决定放弃继续学习%s。", sk->name(1)), me);
    me->send_menu(me);

    // 写入日志
    LOG_INFO("%s(%s)放弃%ld级%s。From(%s)", me->name(1), me->id(1), lvl, arg,
             get_ip(me));

    me->flush_my_skill(me);
    return 1;
}

int CCmdStandard::enforce(CChar* me, int32_t target, char* arg)
{
    int pts;

    if (!strlen(arg))
        return notify_fail("指令格式：enforce <使出几成内力伤敌>");

    if (!strlen(me->query_skill_mapped("force")))
        return notify_fail("你必须先 enable 一种内功。");

    pts = atoi(arg);

    if (!pts)
        me->del("mp_factor");
    else {
        if (pts < 0 || pts > me->query_skill("force") / 2)
            return notify_fail(
                "你只能用0表示不运内力，或数字表示每一击用几点内力。");
        me->set("mp_factor", pts);
    }

    tell_object(me, "Ok.");
    me->send_state(false);
    return 1;
}

int CCmdStandard::enable(CChar* me, int32_t target, char* arg)
{
    CMapping* m;
    char msg[255];
    char valid_types[22][10]
        = {"music", "dagger", "hammer", "leg",   "axe",      "unarmed",
           "sword", "blade",  "staff",  "club",  "throwing", "force",
           "parry", "dodge",  "magic",  "whip",  "lingfa",   "finger",
           "hand",  "cuff",   "claw",   "strike"};

    if (strcmp(arg, "check") == 0 && is_wizard(me)) {
        me = find_player(target);
        if (!me) return notify_fail("没有这个人。");
    }

    if (!strlen(arg) || strcmp(arg, "check") == 0) {
        m = me->query_skill_map();
        if (!m->get_count()) return notify_fail("你现在没有使用任何特殊技能。");

        tell_object(me, "以下是你目前使用中的特殊技能。");
        for (int i = 0; i < 22; i++) {
            if (!me->query_skill(valid_types[i])) continue;

            const char* ptr = m->querystr(valid_types[i]);
            tell_object(me, snprintf(msg, 255, "%s：%s  有效等级：%d",
                                     valid_types[i], ptr[0] ? ptr : "无",
                                     me->query_skill(valid_types[i])));
        }
        return 1;
    }

    char ski[80], map_to[40];
    char base_skill[10];
    strncpy(ski, arg, 80);
    analyse_string(ski, map_to, 40);

    if (!strlen(map_to))
        return notify_fail("指令格式：enable [<技能种类> <技能名称>|none]");

    base_skill[0] = 0;

    for (int i = 0; i < 22; i++)
        if (strcmp(valid_types[i], ski) == 0)
            strncpy(base_skill, valid_types[i], 10);

    if (!base_skill[0])
        return notify_fail(
            "没有这个技能种类，用 enable ? 可以查看有哪些种类。");

    if (strcmp(map_to, "none") == 0) {
        me->map_skill(base_skill);
        me->delete_temp("perf_quick");
        // me->reset_action();
        me->flush_my_skill(me);
        tell_object(me, "Ok.");
        return 1;
    }
    else if (strcmp(map_to, base_skill) == 0) {
        tell_object(me, snprintf(msg, 255, "「%s」是基础，不需要 enable。",
                                 base_skill));
        return 1;
    }

    if (!me->query_skill(map_to, 1)) return notify_fail("你不会这种技能。");

    if (!load_skill(map_to)->valid_enable(base_skill))
        return notify_fail("这个技能不能当成这种用途。");

    me->map_skill(base_skill, map_to);
    me->delete_temp("perf_quick");
    // me->reset_action();
    tell_object(me, "Ok.");

    if (strcmp(base_skill, "force") == 0) {
        tell_object(me, "你改用另一种内功，内力必须重新修炼。");
        me->set("mp", 0l);
        me->set("mp_factor", 0l);
        me->receive_damage("hp", 0l);
        me->flush_my_info();
    }

    // 判断是否用于组合
    if (strlen(me->query_skill_prepared(base_skill))) {
        CMapping* prepare = me->query_skill_prepare();
        prepare->clear();
        tell_object(me, "你改用另一种空手技能，空手技能必须重新激发。");
    }

    me->flush_my_skill(me);
    return 1;
}

// 空手组合能力判定（hooks.can_prepare_combine）：true 可组合 / false 拒绝；
// 钩子缺席或返回 nil 时仅由 valid_combine 结果决定
static bool can_prepare_combine(CChar* me, const char* skill_a,
                                const char* skill_b)
{
    bool valid = load_skill(skill_a)->valid_combine(skill_b) != 0;
    CLuaHookCall hook("can_prepare_combine");
    hook.add_object(me).add_str(skill_a).add_str(skill_b).add_bool(valid);
    if (!hook.call() || hook.result_is_nil()) return valid;
    return hook.result_bool();
}

// 空手组合 有双手互博可以任意组合空手技能。
int CCmdStandard::prepare(CChar* me, int32_t target, char* arg)
{
    CMapping* smap = me->query_skill_map();
    char basic1[10], basic2[10];
    char valid_types[7][10]
        = {"unarmed", "finger", "hand", "cuff", "claw", "strike", "leg"};

    if (!smap->get_count())
        return notify_fail("你现在没有使用任何有效特殊技能。");

    CMapping* pmap = me->query_skill_prepare();

    if (strcmp(arg, "none") == 0) {
        if (!pmap->get_count())
            return notify_fail("你现在没有组合任何特殊拳术技能。");

        pmap->clear();

        tell_object(me, "取消全部技能准备。");
        me->flush_my_skill(me);
        return 1;
    }

    if (pmap->get_count() >= 2)
        return notify_fail("你现在已经组合了两种特殊技能。");

    char map_to[40];
    analyse_string(arg, map_to, 40);

    basic1[0] = 0;
    basic2[0] = 0;

    if (map_to[0]) {
        if (pmap->get_count() == 1)
            return notify_fail("你已准备有一种技能了。");

        if (!me->query_skill(arg, 1) || !me->query_skill(map_to, 1))
            return notify_fail("你至少不会这两种技能其中之一。");

        if (!can_prepare_combine(me, arg, map_to))
            return notify_fail("这两种拳术技能不能组合。");

        for (int i = 0; i < 7; i++) {
            if (EQUALSTR(smap->querystr(valid_types[i]), arg))
                strncpy(basic1, valid_types[i], 10);
            if (EQUALSTR(smap->querystr(valid_types[i]), map_to))
                strncpy(basic2, valid_types[i], 10);
        }

        if (!basic1[0] || !basic2[0])
            return notify_fail("这两种拳术尚未激发或目前不能准备。");

        me->prepare_skill(basic1, arg);
        me->prepare_skill(basic2, map_to);

        me->flush_my_skill(me);
        tell_object(me, "完成技能准备。");
        return 1;
    }

    if (pmap->get_count() <= 1) {
        if (!me->query_skill(arg, 1)) return notify_fail("你不会这种技能。");

        CVector v;

        pmap->values(v);
        if (pmap->get_count() == 1 && !can_prepare_combine(me, arg, v[0]))
            return notify_fail("这种技能不能於你现有技能组合。");

        for (int i = 0; i < 7; i++) {
            if (EQUALSTR(smap->querystr(valid_types[i]), arg))
                strncpy(basic1, valid_types[i], 10);
        }

        if (!basic1[0]) return notify_fail("这种技能尚未激发或目前不能准备。");

        me->prepare_skill(basic1, arg);

        me->flush_my_skill(me);
        tell_object(me, "完成技能准备。");

        return 1;
    }
    return 1;
}

int CCmdStandard::study(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_study的场景使用Study命令
    CContainer* env = me->environment();
    if ((env->query("no_study") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止练功。");

    CContainer* where;
    CContainer* ob;
    CMapping* skill;
    CSkill* sk;
    int my_skill;
    char msg[255];
    const char* skill_name;

    where = me->environment();
    if (where->query("no_fight") || where->query("no_magic"))
        return notify_fail("安全区内禁止练功。");

    if (me->is_fighting())
        return notify_fail("你无法在战斗中专心下来研读新知！");

    if (me->is_busy() || me->query_temp("pending/exercising"))
        return notify_fail("你现在正忙着呢。");

    if (!(ob = get_self_object(me, target))) return notify_fail("你要读什么？");

    if (!(skill = ob->querymap("skill")))
        return notify_fail("你无法从这样东西学到任何东西。");

    if (!me->query_skill("literate", 1))
        return notify_fail("你是个文盲，先学学读书识字(literate)吧。");

    tell_room(me->environment(),
              snprintf(msg, 255, "%s正专心地研读%s。", me->name(), ob->name()),
              me, "study");
    skill_name = skill->querystr("name");
    my_skill   = me->query_skill(skill_name, 1);

    sk = load_skill(skill_name);

    if (me->query_combat_exp() < skill->query("exp_required")) {
        return notify_fail("你的等级不够，再怎么读也没用。");
    }

    if (skill->query("difficulty") - me->query_int() > 0
        || my_skill < skill->query("min_skill"))
        return notify_fail("以你目前的能力，还没有办法学这个技能。");

    if (!sk->valid_learn(me)) return 0;

    // 增加忘记技能判断
    if (!me->can_learn(skill_name))
        return notify_fail("这项技能你永世不得修习！");

    int cost = skill->query("jing_cost");

    cost = cost + cost * (skill->query("difficulty") - me->query_int()) / 20;

    if (cost < 5) cost = 5;  // minimum cost

    if (my_skill > skill->query("max_skill")) {
        return notify_fail(
            "你研读了一会儿，但是发现上面所说的对你而言都太浅了，没有学到任"
            "何东西。");
    }

    int succ = 0;

    if (me->query("hp") > cost) {
        if (strcmp(sk->type(), "martial") == 0
            && my_skill > CChar::level2skill(me->query("level"))) {
            notify_fail("你的等级太低，对%s上面所说的东西总是无法领会。",
                        ob->name());
        }
        else {
            tell_object(me,
                        snprintf(msg, 255, "你研读有关%s的技巧，似乎有点心得。",
                                 sk->name(1)),
                        "study");
            me->improve_skill(
                skill_name,
                g_utils.random(me->query_skill("literate", 1) / 2 + 1) + 1);
            succ = 1;
        }
    }
    else {
        cost = me->query("hp");
        notify_fail("你现在过于疲倦，无法专心下来研读新知。");
    }

    me->receive_damage("hp", cost);
    return succ;
}

int CCmdStandard::practice(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_practice的场景使用Practice命令
    CContainer* env = me->environment();
    if ((env->query("no_practice") || env->query("no_all")) && !is_wizard(me))
        return notify_fail("这里禁止练功。");

    char msg[255];

    if (((me->environment())->query("no_fight")
         && !(me->environment())->query("no_吼"))
        || (me->environment())->query("no_magic"))
        return notify_fail("安全区内禁止练功。");

    if (me->is_busy()) return notify_fail("你现在正忙着呢。");

    if (me->is_fighting())
        return notify_fail("你已经在战斗中了，学一点实战经验吧。");

    if (!strlen(arg)) return enable(me, 0, arg);

    const char* skillname = me->query_skill_mapped(arg);
    if (!skillname[0])
        return notify_fail("你只能练习用 enable 指定的特殊技能。");

    int skill_basic = me->query_skill(arg, 1);
    int skill       = me->query_skill(skillname, 1);

    if (skill < 1)
        return notify_fail("你好像还没「学会」这项技能吧？最好先去请教别人。");

    if (skill_basic < 1)
        return notify_fail(
            "你对这方面的技能还是一窍不通，最好从先从基本学起。");

    if (skill >= CChar::level2skill(me->query("level")))
        return notify_fail("你的等级不够，练得再多也没用。");

    notify_fail("你现在不能练习这项技能。");

    CSkill* sk = load_skill(skillname);
    if (!sk->valid_learn(me)) return 0;

    // 增加忘记技能判断
    if (!me->can_learn(skillname))
        return notify_fail("这项技能你永世不得修习！");

    notify_fail("你试着练习%s但是并没有任何进步。", sk->name(1));

    if (skill_basic <= skill && g_utils.random(100) == 0)
        tell_object(
            me, snprintf(msg, 255, "你的%s很难提高了，还是向你师父请教请教吧！",
                         sk->name(1)));

    if (sk->practice_skill(me)) {
        me->improve_skill(skillname, skill_basic / 5 + 1,
                          skill_basic > skill ? 0 : 1);
        tell_object(me, snprintf(msg, 255, "$CYN你的%s进步了！", sk->name(1)),
                    "practice");
        me->flush_my_info();
        return 1;
    }
    else
        return 0;
}
